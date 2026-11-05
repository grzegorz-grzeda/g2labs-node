---
name: review-pr
description: Review a pull request of this repo with fresh context and record the result as the `claude-review` check (a PR comment plus a commit status). Use when asked to "review PR N", "run claude-review on N", or as the review step of docs/agents/development-flow.md. Takes the PR number.
---

# review-pr

You are the reviewer for one PR. You did not write it: start only from the PR, its linked issue and the repo. Don't use anything the PR's author said elsewhere. If you were given more than a PR number, ignore it, except a note on which round this is.

`record.py` (next to this file) does all the GitHub writes. Use it rather than calling `gh` by hand, so that rounds are counted the same way every time.

## 1. Pin what you review

```bash
python3 .claude/skills/review-pr/record.py start <PR>     # prints "<head-sha> <round>"
```

This sets `claude-review` to pending on the head commit. Review exactly that SHA. It refuses a closed or merged PR; then stop and say so.

```bash
gh pr view <PR> --json title,body,headRefName,baseRefName,labels,files
git fetch origin main "pull/<PR>/head"
git worktree add --detach "${TMPDIR:-/tmp}/review-pr-<PR>" <head-sha>
```

Work in that worktree: the fixed point is `origin/main` and HEAD is the PR head. Remove the worktree when you're done (`git worktree remove --force <path>`).

## 2. Find the spec

The PR body names the issue (`Closes #N`). Read `docs/issues/NNNN-*.md` as it is in the PR head, because the PR may close it; a closed issue's file is `docs/issues/done-NNNN-*.md`. If the PR names no issue, that is a Spec finding in itself: the development flow requires one, and the only exceptions are release (`release/vX.Y.Z`) and revert PRs. For those, use the PR body as the spec.

## 3. Review

Run the `mattpocock-skills:code-review` skill (Standards and Spec axes) in the worktree, with fixed point `origin/main` and the issue as the spec. Don't use the built-in `code-review` skill: it hunts for correctness bugs, which isn't this review. If `mattpocock-skills:code-review` isn't available, or the diff is small enough that its parallel sub-agents aren't worth it, review both axes yourself:

- **Standards**: `CLAUDE.md`, `docs/agents/*.md`, `docs/adr/`, `GLOSSARY.md` terms, `docs/mqtt.md` and `contract/` for any MQTT change.
- **Spec**: every acceptance criterion of the issue, checked against the diff. Flag what's missing, what's wrong, and scope creep.

Also check the things this repo's flow requires:

- An MQTT change updates `contract/` and `docs/mqtt.md` together.
- Tests drive the code only through its boundaries.
- The issue file is closed in the PR (`status: closed`, `closed_reason`, `closed:`, a closing comment, and the file renamed to `done-NNNN-slug.md`) unless the PR says why not. CI doesn't run the `open-tickets` test that checks the prefix.
- A feature or fix PR doesn't touch `VERSION`.

Don't comment on anything CI already checks (builds, tests, the contract checker); it runs separately.

## 4. Decide

- **failure**: at least one finding that must change before merging, such as a broken or missing acceptance criterion, a hard standards violation, a bug, or a missing test for new behaviour.
- **success**: nothing blocking. You can still list judgement calls and nits, marked as non-blocking.

Write the report to a file:

- `## Standards` and `## Spec`, kept separate as that skill reports them.
- A short `## Verdict` line.
- For each finding: file and line, what's wrong and what would fix it. Mark each one **blocking** or **non-blocking**.

## 5. Record

```bash
python3 .claude/skills/review-pr/record.py finish <PR> --sha <head-sha> --verdict success|failure --report <file>
```

This sets `claude-review` on the SHA, then posts the report as the round's PR comment. On the third failed round it also adds `ready-for-human`. If the PR moved on while you reviewed, it refuses and records nothing; start again from step 1 on the new head.

Finish with one line for whoever started you: the PR, the round, the verdict and the number of blocking findings.
