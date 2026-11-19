---
status: accepted
---

# The maintainer approves in the conversation; Claude pushes to main

The maintainer already reads and approves every change while working with Claude in a session, so the PR, `human-gate` label and release-PR steps only repeated that approval. Now `main` changes only when the maintainer says "ok" in the session to a diff Claude showed there, and Claude then pushes it to `main` directly as one squash commit per issue. A stable Release is published the same way: the maintainer says to release vX.Y.Z in the session, after the hardware smoke test passed, and Claude pushes the version commit and the tag. Subagents and side sessions never push `main`; they hand their work back to the session the maintainer is talking to. A session with no maintainer in it pushes only its own branch, which waits for approval in a later conversation.

This replaces how ADR-0006 is carried out, not what it decided: merging still never delivers, and a person still approves every stable Release.

## Considered Options

- **Keep PRs, let Claude add `human-approved` after approval in the session**: rejected. Same approval, plus a PR, a label and a wait for checks on top.
- **Drop only `human-gate`**: rejected. The release PR and the per-PR review rounds stay the slow part.

## Consequences

- CI runs after the push, not before, so Claude runs the affected tests locally before showing the diff. A red `main` is reverted at once without asking (the revert only restores the last green state); the fix then needs approval like any change.
- The fresh-context review still runs, from a subagent on the local diff, for code changes (core, adapters, Hub, scripts), before Claude shows the diff; docs and issue edits skip it.
- An approval covers only the diff that was shown. Anything changed after it needs a new one.
