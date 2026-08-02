#!/usr/bin/env bash
set -euo pipefail

# PRINTERHMI_STABLE_RELEASE_V2
expected_origin_repo="khelix1/PrinterHMI_v3_2"
version="6.0.1"
tag="v${version}"

repo_dir="$(git rev-parse --show-toplevel 2>/dev/null)" || {
    echo "ERROR: run this inside the PrinterHMI Git repository" >&2
    exit 1
}
cd "$repo_dir"

[[ "$(git branch --show-current)" == "main" ]] || {
    echo "ERROR: stable releases must be published from main" >&2
    exit 1
}

if [[ -n "$(git status --porcelain)" ]]; then
    echo "ERROR: working tree is not clean" >&2
    git status --short
    exit 1
fi

[[ "$(tr -d '[:space:]' < version.txt)" == "$version" ]] || {
    echo "ERROR: version.txt is not ${version}" >&2
    exit 1
}

origin_url="$(git remote get-url origin 2>/dev/null)" || {
    echo "ERROR: remote 'origin' is not configured" >&2
    exit 1
}

case "$origin_url" in
    "https://github.com/${expected_origin_repo}.git"|"git@github.com:${expected_origin_repo}.git") ;;
    *)
        echo "ERROR: origin does not point to ${expected_origin_repo}" >&2
        echo "origin: $origin_url" >&2
        exit 1
        ;;
esac

for command in gh sha256sum python3 tar; do
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
stack_commit="$(git rev-parse --short=8 HEAD)"

git fetch origin main --tags --quiet
remote_main="$(git rev-parse origin/main)"

git merge-base --is-ancestor "$remote_main" "$local_commit" || {
    echo "ERROR: origin/main contains history not present locally" >&2
    exit 1
}

if git show-ref --verify --quiet "refs/tags/$tag"; then
    [[ "$(git rev-list -n 1 "$tag")" == "$local_commit" ]] || {
        echo "ERROR: local $tag points to a different commit" >&2
        exit 1
    }
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
    [[ "$remote_tag_commit" == "$local_commit" ]] || {
        echo "ERROR: origin tag $tag points to a different commit" >&2
        exit 1
    }
fi

if gh release view "$tag" --repo "$expected_origin_repo" >/dev/null 2>&1; then
    echo "ERROR: stable release $tag already exists; refusing to replace it" >&2
    exit 1
fi

echo "Building complete PrinterHMI ${tag} stack from ${short_commit}..."
"$repo_dir/tools/build_v6_stack.sh"

stack_base="PrinterHMI-v${version}-full-stack-${stack_commit}"
stack_dir="$repo_dir/dist/$stack_base"
stack_archive="$repo_dir/dist/${stack_base}.tar.gz"
notes="$repo_dir/docs/releases/v${version}.md"
firmware="$stack_dir/p4/PrinterHMI-v${version}-ota.bin"

for required in "$firmware" "$stack_archive" "$notes" \
    "$stack_dir/STACK_MANIFEST.json" "$stack_dir/SHA256SUMS"; do
    [[ -s "$required" ]] || {
        echo "ERROR: required release output is missing: $required" >&2
        exit 1
    }
done

(
    cd "$stack_dir"
    sha256sum -c SHA256SUMS
)

asset_name="PrinterHMI-${tag}-ota.bin"
checksum_name="${asset_name}.sha256"
stack_name="PrinterHMI-${tag}-full-stack.tar.gz"
stack_checksum_name="${stack_name}.sha256"
asset_dir="$(mktemp -d)"
trap 'rm -rf "$asset_dir"' EXIT

cp "$firmware" "$asset_dir/$asset_name"
cp "$stack_archive" "$asset_dir/$stack_name"

(
    cd "$asset_dir"
    sha256sum "$asset_name" > "$checksum_name"
    sha256sum "$stack_name" > "$stack_checksum_name"
)

echo "Stable release assets:"
ls -lh \
    "$asset_dir/$asset_name" \
    "$asset_dir/$checksum_name" \
    "$asset_dir/$stack_name" \
    "$asset_dir/$stack_checksum_name"
cat "$asset_dir/$checksum_name" "$asset_dir/$stack_checksum_name"

if ! git show-ref --verify --quiet "refs/tags/$tag"; then
    git tag -a "$tag" -m "PrinterHMI ${tag}"
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
    "$asset_dir/$stack_name" \
    "$asset_dir/$stack_checksum_name" \
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

for published in \
    "$asset_name" \
    "$checksum_name" \
    "$stack_name" \
    "$stack_checksum_name"; do
    grep -Fxq "$published" <<<"$release_assets" || {
        echo "ERROR: release asset was not published: $published" >&2
        exit 1
    }
done

echo "PASS: origin/main matches $short_commit"
echo "PASS: stable release $tag published"
echo "PASS: OTA and complete IDF6 stack assets published"
