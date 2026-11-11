# RC1 / Cloud UI integration review — 2026-10-19

Status: **preparation only; no merge performed**. This document records a non-destructive comparison of `release-candidate/4.0.2-rc1` (RC1) with `feature/cloud-control-ui-mvp` (new interface).

## Comparison

- Common ancestor: `ae1551e2d348a03afc12354ad4f688918c93ec50`.
- RC1 has three commits not contained in the Cloud UI branch; the aggregate changed-file list against the common ancestor contains **README.md only**.
- Cloud UI has 114 commits absent from RC1, including the new interface and security-related code and documents.
- PR #3 targets RC1 and GitHub reported it non-mergeable on 2026-10-19. Divergence alone does not prove the exact conflict hunk; a genuine merge/conflict check is still required.

## Proposed resolution (not yet executed)

1. Preserve both historical branches and RC1 release/tag.
2. On this dedicated integration branch, merge the current RC1 head into the Cloud UI lineage using a real two-parent Git merge once the actual conflict is confirmed.
3. For conflicting `README.md` content, retain the Cloud UI project overview as the current development README. Preserve the RC1 explanation in its existing RC1 branch and link to the prerelease; do not overwrite the development README with historical test-build marketing.
4. Verify that the resulting source tree contains all Cloud UI changes and no other unexpected file changes, then run Windows/Linux CI on the *exact* merge commit.
5. Keep PR #3 in draft; do not merge to `main`, publish, tag, or run the experimental app with real data until security/release gates are independently satisfied.

## Security gates still open

The DPAPI task codec is tested but not yet integrated into normal task-file reads/writes. Existing saved tasks may contain plaintext sensitive flags. A failing decrypt/read must **not** silently become an empty task list or permit overwriting the original. The Firefox malware warning and third-party dependency/licensing review are unresolved; CI success does not prove malware absence.

This branch is only an isolated preparation area.