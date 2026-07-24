#!/usr/bin/env bash
set -euo pipefail

# PUBLIC_CHECKPOINT_V1
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
    echo "Commit verified work or leave it explicitly uncommitted; nothing was pushed." >&2
    exit 1
fi

if ! git remote get-url origin >/dev/null 2>&1; then
    echo "ERROR: remote 'origin' is not configured" >&2
    exit 1
fi

origin_url="$(git remote get-url origin)"
case "$origin_url" in
    "https://github.com/${expected_origin_repo}.git"|"git@github.com:${expected_origin_repo}.git")
        ;;
    *)
        echo "ERROR: origin does not point to ${expected_origin_repo}" >&2
        echo "origin: $origin_url" >&2
        exit 1
        ;;
esac

if ! command -v rg >/dev/null 2>&1; then
    echo "ERROR: rg (ripgrep) is required for the public-tree safety audit" >&2
    exit 1
fi

if [[ "$branch" == "main" ]]; then
    echo "Auditing public main tree..."

    if git ls-files | rg -n '(\.bak($|_)|\.backup($|_)|\.before_|\.orig$|\.rej$|~$)'; then
        echo "ERROR: tracked backup or editor-recovery files found" >&2
        exit 1
    fi

    if git grep -nE \
        '192\.168\.68\.[0-9]{1,3}|INSERT CONTACT METHOD' \
        -- . ':(exclude)tools/end_of_night_checkpoint.sh'; then
        echo "ERROR: personal LAN default or unresolved public placeholder found" >&2
        exit 1
    fi

    if git grep -nE \
        '#define[[:space:]]+(WIFI_(SSID|PASS)|MOONRAKER_API_KEY)[[:space:]]+"[^"]+"' \
        -- . ':(exclude)tools/end_of_night_checkpoint.sh'; then
        echo "ERROR: embedded network credential definition found" >&2
        exit 1
    fi

    echo "PASS: public-tree safety audit"
fi

git diff-tree --check --root --no-commit-id HEAD

local_commit="$(git rev-parse HEAD)"
echo "Checkpoint: $branch ${local_commit:0:12}"
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

if [[ "$branch" == "main" ]]; then
    git push --atomic -u origin main --follow-tags
else
    git push -u origin "$branch"
fi

git fetch origin --prune --tags --quiet
remote_commit="$(git rev-parse "origin/$branch")"

if [[ "$local_commit" != "$remote_commit" ]]; then
    echo "ERROR: local and origin/$branch do not match after push" >&2
    echo "local:  $local_commit" >&2
    echo "remote: $remote_commit" >&2
    exit 1
fi

echo "PASS: origin/$branch matches ${local_commit:0:12}"
