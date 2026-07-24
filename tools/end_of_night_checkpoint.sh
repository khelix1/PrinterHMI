#!/usr/bin/env bash
set -euo pipefail

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

local_commit="$(git rev-parse HEAD)"
echo "Checkpoint: $branch ${local_commit:0:12}"
git log -1 --oneline --decorate

if [[ "$branch" == "main" ]]; then
    git push -u origin main --follow-tags
else
    git push -u origin "$branch"
fi

git fetch origin "$branch" --quiet
remote_commit="$(git rev-parse "origin/$branch")"

if [[ "$local_commit" != "$remote_commit" ]]; then
    echo "ERROR: local and origin/$branch do not match after push" >&2
    echo "local:  $local_commit" >&2
    echo "remote: $remote_commit" >&2
    exit 1
fi

echo "PASS: origin/$branch matches ${local_commit:0:12}"
