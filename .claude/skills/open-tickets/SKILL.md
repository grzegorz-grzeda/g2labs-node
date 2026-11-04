---
name: open-tickets
description: List and summarize the open, unblocked tickets in docs/issues/, the backlog that can be worked on now. Use when asked "what's next", "what can be worked on", "list open tickets", "show the backlog", or before picking up work under docs/agents/development-flow.md.
---

# open-tickets

Issues are local Markdown files in `docs/issues/` (see `docs/agents/issue-tracker.md`). A ticket is unblocked when every issue in its `blocked_by` is closed.

## 1. List them

```bash
python3 .claude/skills/open-tickets/tickets.py          # unblocked tickets, plus open specs
python3 .claude/skills/open-tickets/tickets.py --all    # also blocked ones, with their open blockers
python3 .claude/skills/open-tickets/tickets.py --json   # the same, as data
```

Run it from the checkout you're answering about. In a worktree that is behind `main`, say so, or run `git fetch` and read `origin/main`'s issues (`--issues <dir>` points it at another directory).

Each entry shows the number, title, labels, parent, assignee, the open tickets it blocks, and the first paragraph of its "What to build" section. Open specs (issues that other issues name as `parent`) are listed apart: they close when their children do.

## 2. Summarize

Answer from the script's output; read an issue file only when its summary line isn't enough. Give:

- **The unblocked tickets, grouped by parent spec** (#1 Node firmware, #17 Hub, #30 contract, or none). Use one line each: the number, the title and what it delivers in plain words.
- **Who can take each one**: only `ready-for-agent` tickets without an `assignee` are free for Claude. Call out `ready-for-human`, `needs-info` and `needs-triage` tickets, and tickets someone has already claimed (`assignee`).
- **What unblocks the most**: name the tickets whose "blocks" list is longest, as candidates to do first.
- **Specs**: just a count and names.

Keep it short: a table or a list, no prose per ticket beyond one line. Recommend a next ticket only if asked.
