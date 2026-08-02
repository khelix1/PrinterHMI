# Git workflow

## Stable baseline

`main` represents integrated, device-tested work. Annotated tags identify
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
git merge --ff-only <feature-name>
git branch -d <feature-name>
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
