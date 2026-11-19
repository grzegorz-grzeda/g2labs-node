# Development flow

How changes get from an issue to the fleet. Claude does the development and pushes; the maintainer approves every change and every stable Release in the conversation with Claude. Why: ADR-0006, ADR-0007.

Two events, never confused:

- **Integration**: a commit pushed to `main`. `main` is always green and shippable, but pushing delivers nothing.
- **Delivery**: a stable Release (a `v*` tag without a suffix). Follow Releases rolls it out to the fleet. Release Candidates (`v*-rc.N`) are skipped by the fleet unless allowed.

## Approval

- `main` changes only when the maintainer says "ok" (or equivalent) in the session to a diff Claude showed there. The approval covers exactly that diff; anything changed afterwards needs a new one.
- Asking for work ("work the backlog", "do #20") is not approval to push it.
- Subagents and side sessions never push `main`. They hand their work back to the session the maintainer is talking to, which shows the diff and pushes.
- A session with no maintainer in it pushes only its own branch, never `main`. That branch waits until the maintainer approves it in a conversation.

## Picking up work

- Work starts when the maintainer asks for it in a session. There is no scheduled agent yet.
- Take only open issues labelled `ready-for-agent` whose `blocked_by` issues are all closed. Never `ready-for-human`.
- Claim an issue with `assignee: claude` in its front matter, as the branch's first commit.
- Up to 3 branches at once, each in its own worktree, and only for issues that don't touch the same area.
- Every change, including issue, glossary and spec edits, has an issue. Work without one gets an issue first.

## From a design conversation to tickets

When a conversation settles a design (a grilling, `/grill-with-docs`, or any "let's work out how X should be"), the maintainer's confirmation of the shared understanding is not approval to implement. The next step is the tickets:

1. **Spec.** Write the outcome as an issue: what to build and why, what is out of scope, and the decisions taken, each in a line. A design that splits into more than one ticket gets a parent spec issue; a single-ticket design is just that ticket.
2. **Tickets.** Split the spec into child issues (`parent:` the spec), each small enough for one squash commit, with acceptance criteria, `blocked_by` where order matters, and `ready-for-agent`, or `ready-for-human` for what needs the maintainer or hardware. Follow-ups noticed along the way get their own `needs-triage` issue.
3. **Approve.** Show the maintainer the spec and the tickets, as text, and wait for their "ok". Their approval pushes the issue files to `main` as one `docs` commit.
4. **Implement.** Only then pick the tickets up as above, each with its own diff and approval.

## Branches and commits

Work happens on a short-lived local branch off `main`, one per issue: `<kind>/<issue>-<slug>`.

| Kind | For |
|---|---|
| `feat` | New behaviour in the Node, the Hub or the contract |
| `fix` | A bug fix |
| `refactor` | Behaviour unchanged |
| `ci` | Workflows and build scripts |
| `docs` | Issues, specs, glossary, ADRs, other docs |
| `chore` | Dependency bumps, Zephyr or SDK updates, housekeeping |
| `revert` | Reverting a commit on `main` |

Examples: `feat/20-release-workflow`, `fix/7-broker-failover-timer`, `docs/30-mqtt-contract`.

- Each issue lands on `main` as one squash commit. Its subject becomes a line in the release notes, so write it for their reader: imperative, about the user-visible change. The body names the issue (`Closes #N`) and says how it was tested.
- The squash commit closes the issue file, per `docs/agents/issue-tracker.md`.
- After the push the branch is deleted.

## Before showing a diff

1. Rebase the branch on the current `main`.
2. Run locally what the change touches: core tests (`west twister -T tests -p native_sim`) and the contract check, `npm test` in `service/`, both sysbuild builds for firmware changes, the unit tests of `.github/scripts/` and `.claude/skills/` for script changes.
3. For code changes (core, adapters, Hub, scripts, workflows), run the fresh-context review (below) and fix its blocking findings. Docs and issue edits skip it.
4. Show the maintainer the diff, the test results and the review's verdict, and wait for approval.

## Review

- A subagent with fresh context reviews the branch with the project skill `review-change` (`.claude/skills/review-change/`). It runs the `mattpocock-skills:code-review` skill (not the built-in bug-finding `code-review`) against the branch: Standards (CLAUDE.md, ADRs, glossary) and Spec (the issue).
- **How to invoke it**: a general-purpose subagent with the Agent tool (never a fork), with only this prompt: `Use the review-change skill on branch <branch>.` Don't add a summary of the change: the reviewer works from the branch, the issue and the repo.
- The reviewer returns its report and a verdict (`success` or `failure`) to the caller; it writes nothing to GitHub. On failure, fix the findings and review again. After 3 failed rounds, stop and bring it to the maintainer.

## After the push

CI (`.github/workflows/ci.yml`) runs on every push to `main`. Claude checks its result. The hardware smoke test (#16) needs real boards and gates stable Releases instead.

## Versioning

- `VERSION` on `main` is the next version with `EXTRAVERSION = dev`, except for the release commit. Feature and fix commits never touch it.
- Semver. While at 0.x: minor for new features, patch for fixes only.
- Every version is a [SemVer 2.0.0](https://semver.org/spec/v2.0.0.html) version without build metadata, of one of three shapes:

  | Version | What | `VERSION` | Tag |
  |---|---|---|---|
  | `X.Y.Z-dev` | `main` between Releases | `EXTRAVERSION = dev` | none |
  | `X.Y.Z-rc.N` | Release Candidate `N`, from 1 | `X.Y.Z-dev`, the commit it's tagged on | `vX.Y.Z-rc.N` |
  | `X.Y.Z` | stable Release | `EXTRAVERSION` empty, set by the release commit | `vX.Y.Z` |

  The pre-release identifier is `rc.N`, with a dot, so that `rc.10` sorts after `rc.9`. `VERSION_TWEAK` stays 0: it only reaches MCUboot's image header, never the version.
- Versions are compared by SemVer 2.0.0 precedence, never as strings: `0.3.0-dev` < `0.3.0-rc.1` < `0.3.0-rc.2` < `0.3.0` < `0.4.0-dev`. That's what "newer" means wherever the Hub decides about an update (#28, #29).
- A Node reports its version as `fw_version` in its Status (`APP_VERSION_STRING`); for a Release Candidate or a Release that is exactly its tag without the `v`. A Release Candidate's images are built with `EXTRAVERSION = rc.N` set in the build's own copy of `VERSION`, never committed (#20), so they report `X.Y.Z-rc.N`.
- CI checks the rule with `.github/scripts/check_version.py`: the committed `VERSION` and every tag (a tag that isn't `v` + an allowed version fails).
- The release commit sets `VERSION` to the release (no `-dev`). The next commit bumps to the next `-dev`.
- One version and one tag ship both the Node firmware and the Hub image.

## Releases

Each release has a tracking issue, "Release vX.Y.Z", holding its Release Candidates, smoke-test results and Claude's recommendation.

1. **Recommend.** After each push that changes the firmware, the Hub or the contract, Claude looks at what is unreleased since the last Release and proposes a Release Candidate when any of these holds: a fix for a bug in released firmware has landed; a parent issue's last child has closed; about two weeks of pushed work is unreleased. Otherwise it says "not yet".
2. **Release Candidate.** Claude pushes `vX.Y.Z-rc.N` on a commit of `main` whose `VERSION` is `X.Y.Z-dev`, counting `N` from 1. It publishes a pre-release with the per-board images and the Hub image tagged with that version only, never `latest`.
3. **Smoke test.** The maintainer runs the hardware smoke test on both boards with the RC and records the result on the tracking issue.
4. **Approve.** Claude shows the release notes (generated from the squash-commit subjects since the last Release), the smoke-test result from the tracking issue and its recommendation. Only when the maintainer says to release vX.Y.Z in the conversation, and the smoke test passed for the last RC of that version, does it go on.
5. **Publish.** Claude pushes the release commit (`VERSION` set to `X.Y.Z`) and the tag `vX.Y.Z` on it, which publishes the stable Release (per-board images, manifest, Hub image as that version and `latest`), then the commit bumping `VERSION` to the next `-dev`.

## When something breaks

- **`main` goes red** after a push, or **a Release Candidate fails** its smoke test: Claude reverts the commit on `main` at once (`revert/<issue>-<slug>`), without waiting for approval, since a revert only restores the last green state, and tells the maintainer. Then it opens an issue for the real fix, which needs approval like any change. The next attempt at the same version is the next `rc.N`.
- **A Rollout stops** at a failed Node: the Hub halts it (#29). The fix goes through the same flow as a patch Release.
