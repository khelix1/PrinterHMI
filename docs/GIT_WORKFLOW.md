# Git workflow

## Stable baseline

`main` represents integrated work, which may use a Development identity.
Nightly and Stable release notes record their validation status; do not infer
complete hardware coverage solely from a branch name. Annotated tags identify
important rollback points. The initial Git baseline is:

```text
known-good-v8-2026-07-22
```

## Feature work

```bash
git switch main
git pull --ff-only
git switch -c <feature-name>
```

After implementation, inspect the exact change:

```bash
git status --short
git diff --stat
git diff
```

Build and test on hardware before committing. Merge only a verified result:

```bash
git add <paths>
git commit -m "Describe the verified outcome"
git switch main
git pull --ff-only origin main
git merge --ff-only <feature-name>
```

## End-of-night checkpoint

Do not automatically commit an unknown or untested tree. At the end of a work
session:

1. Finish the target test or explicitly leave the work on its feature branch.
2. Confirm `git status --short` is empty for a completed checkpoint.
3. Confirm the latest commit is the intended tested state.
4. Push the active integrated branch and tags.

```bash
git status --short
git log --oneline --decorate -1
git push origin main --follow-tags
git fetch origin
git rev-parse main
git rev-parse origin/main
```

The final two hashes must match. Feature branches containing incomplete work
may be pushed explicitly for backup, but must not be merged into `main` until
tested:

```bash
git push -u origin HEAD
```

The repository includes `tools/end_of_night_checkpoint.sh`. On a clean,
tested `main` branch it runs the public-tree audit, builds `PrinterHMI.bin`,
creates its checksum, pushes `main`, creates a commit-specific nightly tag,
publishes a GitHub prerelease and verifies the remote commit and both assets.

On a clean feature branch the checkpoint backs up the branch without creating
firmware; only integrated main publishes a nightly. Keep the feature branch
until the intended publication completes, then verify ancestry and delete it.

It requires ESP-IDF 6.0.2 at the standard project path and an
authenticated GitHub CLI. The checkpoint invokes the canonical IDF6 build
wrapper itself:

```bash
gh auth status
./tools/build_idf6_hosted3.sh
./tools/end_of_night_checkpoint.sh
```

Do not use the checkpoint to publish uncommitted or untested work.

## Known-good tag

Create a known-good tag only after build, OTA/reboot/power-cycle and functional
verification:

```bash
git tag -a known-good-<name>-YYYY-MM-DD -m "Known-good <name>"
git push origin --follow-tags
```

## Project-root archive

```bash
project_name=PrinterHMI_v3_2
git archive \
  --format=tar.gz \
  --prefix="${project_name}/" \
  --output="../${project_name}_<checkpoint>.tar.gz" \
  <tag-or-commit>
```

Verify the first archive entry is `PrinterHMI_v3_2/`.

## Recovery

To inspect or restore an older state, create a branch rather than rewriting
the current work:

```bash
git switch -c inspect-known-good known-good-v8-2026-07-22
```

Never use `git reset --hard` as a routine recovery method.

## Stable publication and branch archives

For Stable, prepare the final version/release notes and use
`./tools/release_stable.sh` from clean integrated main instead of the nightly
checkpoint. It builds the full stack and publishes the versioned assets.

A merged local branch can be deleted after confirming it is an ancestor of main.
For unmerged experiments, first push an archive tag at the complete local tip,
including any commits ahead of its remote branch. Verify that tag before
removing a branch name. Archive tags retain history; they do not merge it.
`chore/stale-code-cleanup` retains the latest parked 3D viewer work, and
`feature/theme-maker` is separately parked. Neither is an active cleanup branch
to merge wholesale into current main.
