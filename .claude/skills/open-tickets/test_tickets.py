import tempfile
import unittest
from pathlib import Path

from tickets import load, parse_issue, render, tickets


def issue(number, status="open", parent="", blocked_by="[]", labels="[ready-for-agent]", extra="", body=""):
    return (
        f'---\nnumber: {number}\ntitle: "Ticket {number}"\nstatus: {status}\nlabels: {labels}\n'
        f"parent: {parent}\nblocked_by: {blocked_by}\n{extra}created: 2026-11-04\n---\n\n# Ticket {number}\n\n{body}"
    )


class Fixture(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def write(self, number, text):
        (self.dir / f"{number:04d}-ticket-{number}.md").write_text(text)

    def rows(self):
        return {r["number"]: r for r in tickets(load(self.dir))}


class Tickets(Fixture):
    def test_blocked_until_every_blocker_closes(self):
        self.write(1, issue(1, status="closed"))
        self.write(2, issue(2))
        self.write(3, issue(3, blocked_by="[1, 2]"))
        self.write(4, issue(4, blocked_by="[1]"))
        rows = self.rows()
        self.assertEqual(rows[3]["open_blockers"], [2])
        self.assertEqual(rows[4]["open_blockers"], [])
        self.assertEqual(rows[2]["blocks"], [3])
        self.assertNotIn(1, rows)

    def test_unknown_blocker_counts_as_open(self):
        self.write(2, issue(2, blocked_by="[99]"))
        self.assertEqual(self.rows()[2]["open_blockers"], [99])

    def test_open_parents_are_specs(self):
        self.write(1, issue(1))
        self.write(2, issue(2, parent="1", extra="assignee: claude\n"))
        rows = self.rows()
        self.assertEqual(rows[1]["kind"], "spec")
        self.assertEqual(rows[2]["kind"], "ticket")
        self.assertEqual(rows[2]["parent"], 1)
        self.assertEqual(rows[2]["assignee"], "claude")

    def test_summary_is_the_first_paragraph_of_what_to_build(self):
        body = "## What to build\n\nDo the\nthing well.\n\nMore detail.\n\n## Acceptance criteria\n\n- [ ] x\n"
        self.assertEqual(parse_issue(issue(5, body=body))["summary"], "Do the thing well.")
        self.assertEqual(parse_issue(issue(5, body="no section"))["summary"], "")

    def test_render_hides_blocked_unless_asked(self):
        self.write(1, issue(1))
        self.write(2, issue(2, blocked_by="[1]"))
        rows = tickets(load(self.dir))
        self.assertIn("Unblocked tickets (1):", render(rows, False))
        self.assertIn("1 blocked ticket(s) not shown", render(rows, False))
        self.assertIn("blocked by #1", render(rows, True))


class RealIssues(unittest.TestCase):
    def test_every_issue_file_parses(self):
        issues_dir = Path(__file__).resolve().parents[3] / "docs" / "issues"
        files = list(issues_dir.glob("[0-9][0-9][0-9][0-9]-*.md"))
        self.assertEqual(len(load(issues_dir)), len(files))


if __name__ == "__main__":
    unittest.main()
