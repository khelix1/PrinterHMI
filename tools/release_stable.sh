#!/usr/bin/env bash
set -euo pipefail

# PRINTERHMI_STABLE_RELEASE_V1
expected_origin_repo="khelix1/PrinterHMI_v3_2"
version="4.2.2"
tag="v${version}"

repo_dir="$(git rev-parse --show-toplevel 2>/dev/null)" || {
    echo "ERROR: run this inside the PrinterHMI Git repository" >&2
    exit 1
}

cd "$repo_dir"

if [[ "$(git branch --show-current)" != "main" ]]; then
    echo "ERROR: stable releases must be published from main" >&2
    exit 1
fi

if [[ -n "$(git status --porcelain)" ]]; then
    echo "ERROR: working tree is not clean" >&2
    git status --short
    exit 1
fi

if [[ "$(tr -d '[:space:]' < version.txt)" != "$version" ]]; then
    echo "ERROR: version.txt is not ${version}" >&2
    exit 1
fi

origin_url="$(git remote get-url origin 2>/dev/null)" || {
    echo "ERROR: remote 'origin' is not configured" >&2
    exit 1
}

case "$origin_url" in
    "https://github.com/${expected_origin_repo}.git"|"git@github.com:${expected_origin_repo}.git")
        ;;
    *)
        echo "ERROR: origin does not point to ${expected_origin_repo}" >&2
        echo "origin: $origin_url" >&2
        exit 1
        ;;
esac

for command in idf.py gh sha256sum; do
    command -v "$command" >/dev/null 2>&1 || {
        echo "ERROR: required command not found: $command" >&2
        exit 1
    }
done

gh auth status >/dev/null 2>&1 || {
    echo "ERROR: GitHub CLI is not authenticated" >&2
    exit 1
}

"$repo_dir/tools/audit/public_tree_audit.sh"
"$repo_dir/tools/audit/version_audit.sh"
git diff-tree --check --root --no-commit-id HEAD

local_commit="$(git rev-parse HEAD)"
short_commit="${local_commit:0:12}"

git fetch origin main --tags --quiet
remote_main="$(git rev-parse origin/main)"

if ! git merge-base --is-ancestor "$remote_main" "$local_commit"; then
    echo "ERROR: origin/main contains history not present locally" >&2
    exit 1
fi

if git show-ref --verify --quiet "refs/tags/$tag"; then
    tag_commit="$(git rev-list -n 1 "$tag")"
    if [[ "$tag_commit" != "$local_commit" ]]; then
        echo "ERROR: local $tag points to a different commit" >&2
        exit 1
    fi
fi

if git ls-remote --exit-code --tags origin "refs/tags/$tag" >/dev/null 2>&1; then
    remote_tag_commit="$(
        git ls-remote --tags origin "refs/tags/$tag^{}" |
        awk 'NR == 1 {print $1}'
    )"
    if [[ -z "$remote_tag_commit" ]]; then
        remote_tag_commit="$(
            git ls-remote --tags origin "refs/tags/$tag" |
            awk 'NR == 1 {print $1}'
        )"
    fi
    if [[ "$remote_tag_commit" != "$local_commit" ]]; then
        echo "ERROR: origin tag $tag points to a different commit" >&2
        exit 1
    fi
fi

if gh release view "$tag" --repo "$expected_origin_repo" >/dev/null 2>&1; then
    echo "ERROR: stable release $tag already exists; refusing to replace it" >&2
    exit 1
fi

echo "Building PrinterHMI ${tag} from ${short_commit}..."
idf.py build

firmware="$repo_dir/build/PrinterHMI.bin"
notes="$repo_dir/docs/releases/v4.2.1.md"

[[ -s "$firmware" ]] || {
    echo "ERROR: firmware not found: $firmware" >&2
    exit 1
}

[[ -s "$notes" ]] || {
    echo "ERROR: release notes not found: $notes" >&2
    exit 1
}

asset_name="PrinterHMI-${tag}-ota.bin"
checksum_name="${asset_name}.sha256"
asset_dir="$(mktemp -d)"
trap 'rm -rf "$asset_dir"' EXIT

cp "$firmware" "$asset_dir/$asset_name"
(
    cd "$asset_dir"
    sha256sum "$asset_name" > "$checksum_name"
)

echo "Stable release assets:"
ls -lh "$asset_dir/$asset_name" "$asset_dir/$checksum_name"
cat "$asset_dir/$checksum_name"

if ! git show-ref --verify --quiet "refs/tags/$tag"; then
    git tag -a "$tag" \
        -m "PrinterHMI ${tag}"
fi

git push --atomic -u origin main "refs/tags/$tag"
git fetch origin main --tags --quiet

[[ "$(git rev-parse origin/main)" == "$local_commit" ]] || {
    echo "ERROR: origin/main does not match the release commit" >&2
    exit 1
}

[[ "$(git rev-list -n 1 "$tag")" == "$local_commit" ]] || {
    echo "ERROR: $tag does not resolve to the release commit" >&2
    exit 1
}

gh release create "$tag" \
    "$asset_dir/$asset_name" \
    "$asset_dir/$checksum_name" \
    --repo "$expected_origin_repo" \
    --verify-tag \
    --title "PrinterHMI ${tag}" \
    --notes-file "$notes"

release_assets="$(
    gh release view "$tag" \
        --repo "$expected_origin_repo" \
        --json assets \
        --jq '.assets[].name'
)"

grep -Fxq "$asset_name" <<<"$release_assets" || {
    echo "ERROR: firmware asset was not published" >&2
    exit 1
}

grep -Fxq "$checksum_name" <<<"$release_assets" || {
    echo "ERROR: checksum asset was not published" >&2
    exit 1
}

echo "PASS: origin/main matches $short_commit"
echo "PASS: stable release $tag published"
echo "PASS: $asset_name and checksum published"
