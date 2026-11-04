---
number: 29
title: "Follow Releases Update Policy and Hold"
status: open
labels: [ready-for-agent]
parent: 17
blocked_by: [28, 20]
created: 2026-11-04
---

# Follow Releases Update Policy and Hold

## What to build

The Follow Releases Update Policy: the Hub watches this repo's GitHub Releases and rolls each new stable release out to the fleet, one Node at a time, stopping at the first failure. Nodes on Hold are skipped.

## Acceptance criteria

- [ ] The Update Policy is a fleet-wide Hub setting: Manual or Follow Releases
- [ ] Under Follow Releases the Hub polls the public GitHub Releases API every few hours (configurable)
- [ ] Pre-releases are skipped unless a setting allows them
- [ ] The Hub uses only the release's `manifest.json` to pick each Node's image by the board in its Status, and checks the SHA-256 after download
- [ ] Every Node on an older version and not on Hold is updated one after another through the update engine
- [ ] The rollout stops at the first failure, and that release is not retried automatically; the admin page shows why it stopped and can resume it
- [ ] Hold can be set and cleared per Node from the admin page
- [ ] Tests cover a full rollout, stop on failure, Hold, pre-release skipping and a bad SHA-256, against a fake GitHub API and fake Nodes
