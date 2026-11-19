---
name: review-change
description: Review a local branch of this repo with fresh context, before its diff is shown to the maintainer, and report the findings and a verdict back to the caller. Use when asked to "review branch X", "review this change", or as the review step of docs/agents/development-flow.md. Takes the branch name.
---

# review-change

You are the reviewer for one branch. You did not write it: start only from the branch, its issue and the repo. Don't use anything the author said elsewhere. If you were given more than a branch name, ignore it, except a note on which round this is.

You write nothing: no commits, no pushes, no GitHub comments or statuses. Your report goes back to whoever started you.

## 1. Pin what you review

```bash
git rev-parse <branch>                     # the SHA you review
git merge-base main <branch>               # the fixed point
git worktree add --detach "${TMPDIR:-/tmp}/review-change-<sha>" <sha>
```

Work in that worktree: the fixed point is the merge base with `main` and HEAD is the branch tip. Remove the worktree when you're done (`git worktree remove --force <path>`).

## 2. Find the spec

The branch name is `<kind>/<issue>-<slug>`. Read `docs/issues/NNNN-*.md` as it is on the branch, because the branch may close it; a closed issue's file is `docs/issues/done-NNNN-*.md`. If the branch names no issue, that is a Spec finding in itself: the development flow requires one, and the only exception is a revert. For a revert, use its commit message as the spec.

## 3. Review

Run the `mattpocock-skills:code-review` skill (Standards and Spec axes) in the worktree, with the merge base as the fixed point and the issue as the spec. Don't use the built-in `code-review` skill: it hunts for correctness bugs, which isn't this review. If `mattpocock-skills:code-review` isn't available, or the diff is small enough that its parallel sub-agents aren't worth it, review both axes yourself:

- **Standards**: `CLAUDE.md`, `docs/agents/*.md`, `docs/adr/`, `GLOSSARY.md` terms, `docs/mqtt.md` and `contract/` for any MQTT change.
- **Spec**: every acceptance criterion of the issue, checked against the diff. Flag what's missing, what's wrong, and scope creep.

Also check the things this repo's flow requires:

- An MQTT change updates `contract/` and `docs/mqtt.md` together.
- Tests drive the code only through its boundaries.
- The issue file is closed on the branch (`status: closed`, `closed_reason`, `closed:`, a closing comment, and the file renamed to `done-NNNN-slug.md`) unless the branch says why not.
- A feature or fix doesn't touch `VERSION`.

Don't comment on anything the tests or CI already check (builds, tests, the contract checker).

## 4. Decide and report

- **failure**: at least one finding that must change before it is shown to the maintainer, such as a broken or missing acceptance criterion, a hard standards violation, a bug, or a missing test for new behaviour.
- **success**: nothing blocking. You can still list judgement calls and nits, marked as non-blocking.

Your final message is the report:

- The branch, the SHA reviewed and the round.
- `## Standards` and `## Spec`, kept separate as that skill reports them.
- For each finding: file and line, what's wrong and what would fix it. Mark each one **blocking** or **non-blocking**.
- A last `## Verdict` line: `success` or `failure`, and the number of blocking findings.
