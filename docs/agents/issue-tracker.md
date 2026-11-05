# Issue tracker: local Markdown files

Issues and specs for this repo live as Markdown files in `docs/issues/`, one file per issue, versioned with the code. There is no external tracker. Edit them with ordinary file tools and commit them like any other change. Issues #1–#29 were migrated from GitHub Issues on 2026-11-04 and kept their numbers.

## File format

`docs/issues/NNNN-slug.md`, where `NNNN` is the issue number zero-padded to 4 digits and `slug` is the title in lowercase kebab-case, cut to about 60 characters. A closed issue's file is `docs/issues/done-NNNN-slug.md` instead, so the file list alone tells finished issues from live ones:

```md
---
number: 30
title: "Short imperative title"
status: open
labels: [ready-for-agent]
parent: 1
blocked_by: [18, 19]
created: 2026-11-05
---

# Short imperative title

## What to build

…

## Acceptance criteria

- [ ] …

## Comments

### 2026-11-06 — author

…
```

- `status`: `open` or `closed`. A closed issue also has `closed_reason: completed` or `closed_reason: not_planned`, and `closed: <date>`.
- `labels`: values from `docs/agents/triage-labels.md`, plus `wayfinder:*` labels.
- `parent`: the parent issue's number, or empty. Specs (#1 the Node, #17 the Hub) are parents.
- `blocked_by`: numbers of issues that must be closed first. An issue is unblocked when every one of them is `closed`.
- The title is repeated as the first heading so the file reads well on its own. The front matter `title` is the authoritative one.
- Refer to issues as `#N` everywhere (commits, ADRs, other issues). It means the local issue, not GitHub's.

## Conventions

- **Create an issue**: take the next number (one more than the highest `NNNN` in `docs/issues/`, `done-` files included) and write the file with `status: open` and `created: <today>`.
- **Read an issue**: read `docs/issues/NNNN-*.md`, or `docs/issues/done-NNNN-*.md` once it's closed (`ls docs/issues/*NNNN-*` finds either).
- **List the workable backlog**: the `open-tickets` skill, or `python3 .claude/skills/open-tickets/tickets.py` (`--all` adds blocked tickets).
- **List issues**: the open ones are the files without the `done-` prefix (`ls docs/issues/[0-9]*.md`), or `grep -l '^status: open' docs/issues/*.md`. Filter by label with `grep -l 'labels: .*ready-for-agent' …` or by parent with `grep -l '^parent: 17$' …`.
- **Make an issue a sub-issue of a parent**: set `parent:` in the child's front matter.
- **Comment on an issue**: append `### <date> — <author>` and the text under `## Comments` (create that section at the end of the file if it's missing).
- **Apply / remove labels**: edit the `labels:` list.
- **Close**: set `status: closed`, `closed_reason:` and `closed:`, append a comment saying why, and rename the file with the `done-` prefix (`git mv docs/issues/NNNN-slug.md docs/issues/done-NNNN-slug.md`), whatever the `closed_reason`. Close an issue in the same commit as the work that finishes it. Reopening one drops the prefix again. The `open-tickets` tests check that the prefix and `status` agree.

## Pull requests as a triage surface

**PRs as a request surface: no.** _(Set to `yes` if this repo treats external PRs as feature requests; `/triage` reads this flag.)_

## When a skill says "publish to the issue tracker"

Create an issue file in `docs/issues/`.

## When a skill says "fetch the relevant ticket"

Read it as in **Read an issue** above.

## Wayfinding operations

Used by `/wayfinder`. The **map** is a single issue with **child** issues as tickets.

- **Map**: a single issue labelled `wayfinder:map`, holding the Notes / Decisions-so-far / Fog body.
- **Child ticket**: an issue whose `parent:` is the map. Labels: `wayfinder:<type>` (`research`/`prototype`/`grilling`/`task`). Once claimed, add `assignee: <name>` to its front matter.
- **Blocking**: the `blocked_by:` list. A ticket is unblocked when every issue in it is `closed`.
- **Frontier query**: list the map's open children (`grep -l '^parent: <map>$'`, then keep `status: open`), drop any with an open blocker or an `assignee`; lowest number wins.
- **Claim**: add `assignee:` to the ticket, the session's first write.
- **Resolve**: append the answer as a comment, close the ticket, then append a context pointer (gist + `#N`) to the map's Decisions-so-far.
