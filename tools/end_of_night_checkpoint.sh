#!/usr/bin/env bash
set -euo pipefail

# PUBLIC_CHECKPOINT_V2_NIGHTLY
expected_origin_repo="khelix1/PrinterHMI_v3_2"

repo_dir="$(git rev-parse --show-toplevel 2>/dev/null)" || {
    echo "ERROR: run this inside the PrinterHMI Git repository" >&2
    exit 1
}

cd "$repo_dir"

branch="$(git branch --show-current)"
if [[ -z "$branch" ]]; then
    echo "ERROR: detached HEAD; switch to a named branch first" >&2
    exit 1
fi

if [[ -n "$(git status --porcelain)" ]]; then
    echo "ERROR: working tree is not clean" >&2
    git status --short
    echo "Commit verified work before checkpointing." >&2
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

if [[ "$branch" == "main" ]]; then
    "$repo_dir/tools/audit/public_tree_audit.sh"
    "$repo_dir/tools/audit/version_audit.sh"
fi

git diff-tree --check --root --no-commit-id HEAD

local_commit="$(git rev-parse HEAD)"
short_commit="${local_commit:0:12}"

echo "Checkpoint: $branch $short_commit"
git log -1 --oneline --decorate

if git ls-remote --exit-code --heads origin "$branch" >/dev/null 2>&1; then
    git fetch origin "$branch" --quiet
    remote_before="$(git rev-parse "origin/$branch")"

    if ! git merge-base --is-ancestor "$remote_before" "$local_commit"; then
        echo "ERROR: origin/$branch contains history not present locally" >&2
        echo "Fetch and reconcile the branch before checkpointing." >&2
        exit 1
    fi
fi

# Feature branches are backed up, but never published as nightly firmware.
if [[ "$branch" != "main" ]]; then
    git push -u origin "$branch"
    git fetch origin --prune --tags --quiet

    remote_commit="$(git rev-parse "origin/$branch")"

    if [[ "$local_commit" != "$remote_commit" ]]; then
        echo "ERROR: local and origin/$branch do not match after push" >&2
        exit 1
    fi

    echo "PASS: origin/$branch matches $short_commit"
    echo "Nightly build skipped: only integrated main produces firmware."
    exit 0
fi

nightly_reply=""

if [[ -t 0 ]]; then
    read -r -p "Build and publish a nightly firmware release? [y/N] " \
        nightly_reply
else
    echo "Non-interactive checkpoint: nightly build skipped."
fi

case "$nightly_reply" in
    y|Y|yes|YES|Yes)
        ;;
    *)
        git push --atomic -u origin main --follow-tags
        git fetch origin --prune --tags --quiet

        remote_commit="$(git rev-parse origin/main)"

        if [[ "$local_commit" != "$remote_commit" ]]; then
            echo "ERROR: local main and origin/main do not match after push" >&2
            echo "local:  $local_commit" >&2
            echo "remote: $remote_commit" >&2
            exit 1
        fi

        echo "PASS: origin/main matches $short_commit"
        echo "Nightly build skipped by operator."
        exit 0
        ;;
esac

for command in gh sha256sum python3; do
    if ! command -v "$command" >/dev/null 2>&1; then
        echo "ERROR: required command not found: $command" >&2
        exit 1
    fi
done

if ! gh auth status >/dev/null 2>&1; then
    echo "ERROR: GitHub CLI is not authenticated" >&2
    exit 1
fi

nightly_date="$(date +%Y-%m-%d)"
nightly_tag="nightly-${nightly_date}-${short_commit}"
stable_version="$(tr -d '[:space:]' < version.txt)"

restore_stable_version()
{
    printf '%s\n' "$stable_version" > "$repo_dir/version.txt"
}

# The exact tag fits esp_app_desc_t.version (31 characters plus NUL).
trap restore_stable_version EXIT
printf '%s\n' "$nightly_tag" > "$repo_dir/version.txt"

echo "Building nightly firmware identity ${nightly_tag}..."
"$repo_dir/tools/build_idf6_hosted3.sh"

firmware="$repo_dir/build-idf6-hosted3/PrinterHMI.bin"

if [[ ! -s "$firmware" ]]; then
    echo "ERROR: expected firmware not found: $firmware" >&2
    exit 1
fi

if ! grep -aFq "$nightly_tag" "$firmware"; then
    echo "ERROR: nightly identity was not embedded in firmware" >&2
    exit 1
fi

restore_stable_version
trap - EXIT

if [[ -n "$(git status --porcelain)" ]]; then
    echo "ERROR: nightly version restoration left a dirty tree" >&2
    git status --short
    exit 1
fi

asset_name="PrinterHMI-${nightly_tag}.bin"
checksum_name="${asset_name}.sha256"

asset_dir="$(mktemp -d)"
trap 'rm -rf "$asset_dir"' EXIT

cp "$firmware" "$asset_dir/$asset_name"

(
    cd "$asset_dir"
    sha256sum "$asset_name" > "$checksum_name"
)

echo "Nightly asset:"
ls -lh "$asset_dir/$asset_name" "$asset_dir/$checksum_name"
cat "$asset_dir/$checksum_name"

# Publish the tested source before attaching firmware to its exact commit.
git push --atomic -u origin main --follow-tags
git fetch origin --prune --tags --quiet

remote_commit="$(git rev-parse origin/main)"

if [[ "$local_commit" != "$remote_commit" ]]; then
    echo "ERROR: local main and origin/main do not match after push" >&2
    echo "local:  $local_commit" >&2
    echo "remote: $remote_commit" >&2
    exit 1
fi

if git ls-remote --exit-code --tags origin \
    "refs/tags/$nightly_tag" >/dev/null 2>&1; then
    git fetch origin "refs/tags/$nightly_tag:refs/tags/$nightly_tag" --quiet

    tag_commit="$(git rev-list -n 1 "$nightly_tag")"

    if [[ "$tag_commit" != "$local_commit" ]]; then
        echo "ERROR: $nightly_tag points to a different commit" >&2
        exit 1
    fi
else
    git tag -a "$nightly_tag" \
        -m "PrinterHMI nightly $nightly_date ($short_commit)"
    git push origin "refs/tags/$nightly_tag"
fi

if gh release view "$nightly_tag" \
    --repo "$expected_origin_repo" >/dev/null 2>&1; then
    echo "Nightly release already exists: $nightly_tag"
else
    commit_subject="$(git log -1 --format=%s)"

    gh release create "$nightly_tag" \
        "$asset_dir/$asset_name" \
        "$asset_dir/$checksum_name" \
        --repo "$expected_origin_repo" \
        --verify-tag \
        --prerelease \
        --title "PrinterHMI Nightly $nightly_date ($short_commit)" \
        --notes "Automated nightly firmware from tested main.

Commit: $local_commit
Firmware identity: $nightly_tag
Change: $commit_subject
ESP-IDF: $(idf.py --version)

This is a development prerelease. Stable release assets remain immutable."
fi

release_assets="$(
    gh release view "$nightly_tag" \
        --repo "$expected_origin_repo" \
        --json assets \
        --jq '.assets[].name'
)"

grep -Fxq "$asset_name" <<<"$release_assets" || {
    echo "ERROR: nightly firmware asset was not published" >&2
    exit 1
}

grep -Fxq "$checksum_name" <<<"$release_assets" || {
    echo "ERROR: nightly checksum asset was not published" >&2
    exit 1
}

echo "PASS: origin/main matches $short_commit"
echo "PASS: nightly release $nightly_tag"
echo "PASS: $asset_name and checksum published"
