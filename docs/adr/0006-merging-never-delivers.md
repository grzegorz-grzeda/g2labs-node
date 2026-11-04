# Merging never delivers; a person approves every stable Release

Claude writes, reviews and merges PRs into `main` on its own once every required check is green, but nothing merged reaches the fleet until it is in a stable Release. Claude tags Release Candidates freely, since Follow Releases skips them. A stable Release is published only when the maintainer merges a release PR, which Claude prepares with a recommendation to release or to wait. Under Follow Releases a stable Release is a Rollout to every Node, so it is the one step where a person's judgement and a hardware smoke test sit between agent-written code and devices.

## Considered Options

- **Merge to `main` delivers** (continuous deployment): rejected. Every agent merge would start a Rollout with no hardware test in between.
- **Claude also cuts stable Releases**: rejected. The Rollout stops at the first failure, but that one Node still takes the hit, and nobody would have looked at the change before devices did.
- **A person approves every PR**: rejected. It turns the maintainer into the bottleneck for changes that are only integrated, not delivered.
