import unittest

from record import failed_rounds, next_round, past_rounds, render_comment, should_escalate

SHA = "0123456789abcdef0123456789abcdef01234567"


class Rounds(unittest.TestCase):
    def test_first_round(self):
        self.assertEqual(next_round(past_rounds(["LGTM", "unrelated"])), 1)

    def test_rounds_come_from_earlier_comments(self):
        bodies = [
            render_comment(1, SHA, "failure", "x", 1),
            "a human comment",
            render_comment(2, SHA, "failure", "y", 2),
        ]
        rounds = past_rounds(bodies)
        self.assertEqual(rounds, [(1, SHA, "failure"), (2, SHA, "failure")])
        self.assertEqual(next_round(rounds), 3)
        self.assertEqual(failed_rounds(rounds), 2)

    def test_passed_rounds_are_not_failures(self):
        rounds = past_rounds([render_comment(1, SHA, "failure", "x", 1), render_comment(2, SHA, "success", "ok", 1)])
        self.assertEqual(failed_rounds(rounds), 1)


class Escalation(unittest.TestCase):
    def test_third_failure_escalates(self):
        self.assertFalse(should_escalate("failure", 2))
        self.assertTrue(should_escalate("failure", 3))
        self.assertTrue(should_escalate("failure", 4))

    def test_a_pass_never_escalates(self):
        self.assertFalse(should_escalate("success", 3))


class Comment(unittest.TestCase):
    def test_shows_round_and_verdict(self):
        body = render_comment(2, SHA, "success", "## Standards\nfine", 1)
        self.assertIn("round 2: ✅ passed", body)
        self.assertIn("`0123456789ab`", body)
        self.assertIn("## Standards\nfine", body)
        self.assertNotIn("ready-for-human", body)

    def test_third_failure_says_it_stops(self):
        self.assertIn("ready-for-human", render_comment(3, SHA, "failure", "bad", 3))
        self.assertNotIn("ready-for-human", render_comment(2, SHA, "failure", "bad", 2))


if __name__ == "__main__":
    unittest.main()
