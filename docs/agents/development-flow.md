# Development flow

How changes get from an issue to the fleet. Claude does the development, reviews and merges; the maintainer approves stable Releases. Why: ADR-0006.

Two events, never confused:

- **Integration**: a PR squash-merged into `main`. `main` is always green and shippable, but merging delivers nothing.
- **Delivery**: a stable Release (a `v*` tag without a suffix). Follow Releases rolls it out to the fleet. Release Candidates (`v*-rc.N`) are skipped by the fleet unless allowed.

## Picking up work

- Work starts when the maintainer asks for it in a session ("work the backlog", "do #20"). There is no scheduled agent yet.
- Take only open issues labelled `ready-for-agent` whose `blocked_by` issues are all closed. Never `ready-for-human`.
- Claim an issue with `assignee: claude` in its front matter, as the branch's first commit.
- Up to 3 branches at once, each in its own worktree, and only for issues that don't touch the same area.
- Every change goes through a PR, including issue, glossary and spec edits. Work without an issue gets an issue first.

## Branches

Short-lived, off `main`, one per issue: `<kind>/<issue>-<slug>`.

| Kind | For |
|---|---|
| `feat` | New behaviour in the Node, the Hub or the contract |
| `fix` | A bug fix |
| `refactor` | Behaviour unchanged |
| `ci` | Workflows and build scripts |
| `docs` | Issues, specs, glossary, ADRs, other docs |
| `chore` | Dependency bumps, Zephyr or SDK updates, housekeeping |
| `revert` | Reverting a merged PR |
| `release` | A release PR, named `release/vX.Y.Z` with no issue number |

Examples: `feat/20-release-workflow`, `fix/7-broker-failover-timer`, `docs/30-mqtt-contract`.

## Pull requests

- The PR title is the squash commit's subject and becomes a line in the release notes, so write it for a reader of the release notes: imperative, about the user-visible change.
- The body names the issue (`Closes #N`, a local issue) and says what changed and how it was tested.
- The squash commit closes the issue file, per `docs/agents/issue-tracker.md`.
- Open the PR with GitHub auto-merge turned on (squash). GitHub merges it once every required check is green; the branch is deleted on merge.
- When `main` moves, merge it into the branch; CI runs again. Run the review again only if resolving a conflict changed code.

## Required checks

A PR merges only when all of these pass:

- **CI**: every job in `.github/workflows/ci.yml`: `build-and-test` (core tests, contract check, both sysbuild builds) and `hub` (Hub build, tests and image).
- **`claude-review`**: the review below.
- **`human-gate`**: fails while the PR touches a gated path or carries `ready-for-human`, until the maintainer adds the `human-approved` label. A push after that removes the label again, so approval covers only what was looked at. The gated paths (workflows, release and signing configuration, the contract, ADRs, `VERSION` and the like) are listed only in [`.github/gated-paths`](../../.github/gated-paths), with the reason for each. The `human-gate` workflow runs on `pull_request_target` from `main`'s copy, so a PR can't change its own gate, and reports as the `human-gate` commit status.

The hardware smoke test (#16) needs real boards and is not a merge check; it gates stable Releases instead.

## Review

- Every PR is reviewed by a separate session or subagent with fresh context, not by the one that wrote it, using the project skill `review-pr` (`.claude/skills/review-pr/`). It runs the `mattpocock-skills:code-review` skill (not the built-in bug-finding `code-review`) against the PR's head: Standards (CLAUDE.md, ADRs, glossary) and Spec (the linked issue).
- **How the author invokes it**: after opening the PR (and again after each push that addresses findings), start a reviewer that doesn't share your context. Use a subagent with the Agent tool (a general-purpose agent, never a fork) or a new session, with only this prompt: `Use the review-pr skill on PR #<N>.` Don't add your own summary of the change: the reviewer works from the PR, the issue and the repo.
- The reviewer posts its findings as a PR comment headed with the round number and sets the `claude-review` commit status to `success` or `failure` on the head commit it reviewed. Commit statuses belong to one commit, so a new push leaves the new head without `claude-review`; since it is a required check on `main`, the PR waits ("Expected") until the reviewer runs again.
- On failure the author fixes the findings and the reviewer runs again on the new head. After 3 failed rounds the reviewer labels the PR `ready-for-human` and it stops.

## Versioning

- `VERSION` on `main` is always the next version with `EXTRAVERSION = dev`. Feature and fix PRs never touch it.
- Semver. While at 0.x: minor for new features, patch for fixes only.
- Every version is a [SemVer 2.0.0](https://semver.org/spec/v2.0.0.html) version without build metadata, of one of three shapes:

  | Version | What | `VERSION` | Tag |
  |---|---|---|---|
  | `X.Y.Z-dev` | `main` between Releases | `EXTRAVERSION = dev` | none |
  | `X.Y.Z-rc.N` | Release Candidate `N`, from 1 | `X.Y.Z-dev`, the commit it's tagged on | `vX.Y.Z-rc.N` |
  | `X.Y.Z` | stable Release | `EXTRAVERSION` empty, set by the release PR `release/vX.Y.Z` | `vX.Y.Z` |

  The pre-release identifier is `rc.N`, with a dot, so that `rc.10` sorts after `rc.9`. `VERSION_TWEAK` stays 0: it only reaches MCUboot's image header, never the version.
- Versions are compared by SemVer 2.0.0 precedence, never as strings: `0.3.0-dev` < `0.3.0-rc.1` < `0.3.0-rc.2` < `0.3.0` < `0.4.0-dev`. That's what "newer" means wherever the Hub decides about an update (#28, #29).
- A Node reports its version as `fw_version` in its Status (`APP_VERSION_STRING`); for a Release Candidate or a Release that is exactly its tag without the `v`. A Release Candidate's images are built with `EXTRAVERSION = rc.N` (#20), so they report `X.Y.Z-rc.N`.
- CI checks the rule with `.github/scripts/check_version.py`: `VERSION`, the branch of a release PR, and every `v*` tag.
- A release PR sets `VERSION` to the release (no `-dev`). The first PR after the Release bumps to the next `-dev`.
- One version and one tag ship both the Node firmware and the Hub image.

## Releases

Each release has a tracking issue, "Release vX.Y.Z", holding its Release Candidates, smoke-test results and Claude's recommendation.

1. **Recommend.** After each merge that changes the firmware, the Hub or the contract, Claude looks at what is unreleased since the last Release and proposes a Release Candidate when any of these holds: a fix for a bug in released firmware has landed; a parent issue's last child has closed; about two weeks of merged work is unreleased. Otherwise it says "not yet".
2. **Release Candidate.** Claude pushes `vX.Y.Z-rc.N` on a commit of `main` whose `VERSION` is `X.Y.Z-dev`, counting `N` from 1. It publishes a pre-release with the per-board images and the Hub image tagged with that version only, never `latest`.
3. **Smoke test.** The maintainer runs the hardware smoke test on both boards with the RC and records the result on the tracking issue.
4. **Release PR.** Claude opens `release/vX.Y.Z`: it sets `VERSION`, carries release notes generated from the squash-commit titles since the last Release, links the tracking issue and states its recommendation. It is never auto-merged (`human-gate`).
5. **Publish.** The maintainer's merge of the release PR publishes the stable Release (tag, per-board images, manifest, Hub image as that version and `latest`). Closing it unmerged means "not now".

## When something breaks

- **`main` goes red** after a merge, or **a Release Candidate fails** its smoke test: Claude first opens a revert PR (`revert/<issue>-<slug>`, normal checks, auto-merged), then opens an issue for the real fix. The next attempt at the same version is the next `rc.N`.
- **A Rollout stops** at a failed Node: the Hub halts it (#29). The fix goes through the same flow as a patch Release.
