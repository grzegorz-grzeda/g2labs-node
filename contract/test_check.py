# SPDX-License-Identifier: MIT
"""Tests of the contract checker: python -m unittest discover -s contract"""

import json
import tempfile
import unittest
from pathlib import Path

import check

STATUS = {
    "node_id": "404cca0ab1f2",
    "fw_version": "0.1.0",
    "uptime_s": 1,
    "network": "home",
    "rssi": None,
    "free_heap": 100,
}


def publish(topic, payload, retained):
    if not isinstance(payload, str):
        payload = json.dumps(payload)
    return {"test": "suite.test_x", "topic": topic, "payload": payload, "retained": retained}


class CheckPublish(unittest.TestCase):
    def setUp(self):
        self.topics = check.load_index(check.CONTRACT_DIR)

    def errors(self, record, publisher="node"):
        return check.check_publish(self.topics, record, publisher)

    def test_valid_publishes_pass(self):
        self.assertEqual(self.errors(publish("g2labs/404cca0ab1f2/availability", "online", True)), [])
        self.assertEqual(self.errors(publish("g2labs/404cca0ab1f2/status", STATUS, False)), [])

    def test_unknown_topic_fails(self):
        self.assertIn("topic is not in contract/topics.json",
                      self.errors(publish("g2labs/404cca0ab1f2/debug", "x", False)))

    def test_topic_with_a_malformed_node_id_fails(self):
        self.assertTrue(self.errors(publish("g2labs/404CCA0AB1F2/availability", "online", True)))

    def test_wrong_retained_flag_fails(self):
        self.assertIn("retained must be true",
                      self.errors(publish("g2labs/404cca0ab1f2/availability", "online", False)))

    def test_text_value_not_allowed_fails(self):
        self.assertTrue(self.errors(publish("g2labs/404cca0ab1f2/availability", "up", True)))

    def test_extra_field_fails(self):
        errors = self.errors(publish("g2labs/404cca0ab1f2/status", {**STATUS, "extra": 1}, False))
        self.assertTrue(any("extra" in e for e in errors), errors)

    def test_schema_error_fails(self):
        errors = self.errors(publish("g2labs/404cca0ab1f2/status", {**STATUS, "uptime_s": "1"}, False))
        self.assertTrue(any(e.startswith("uptime_s:") for e in errors), errors)

    def test_payload_that_is_not_json_fails(self):
        self.assertTrue(self.errors(publish("g2labs/404cca0ab1f2/status", "{", False)))

    def test_wrong_publisher_fails(self):
        self.assertTrue(self.errors(publish("g2labs/404cca0ab1f2/availability", "online", True),
                                    publisher="hub"))


class CheckDumps(unittest.TestCase):
    def test_failure_names_test_topic_payload_and_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            dump = Path(tmp) / check.DUMP_NAME
            dump.write_text(json.dumps(publish("g2labs/404cca0ab1f2/status",
                                               {**STATUS, "extra": 1}, False)) + "\n")
            count, failures = check.check_dumps(check.load_index(check.CONTRACT_DIR),
                                                check.dump_files([Path(tmp)]), "node")
        self.assertEqual(count, 1)
        self.assertEqual(len(failures), 1)
        for part in ("suite.test_x", "g2labs/404cca0ab1f2/status", '"extra": 1', "error:"):
            self.assertIn(part, failures[0])


class CheckContract(unittest.TestCase):
    def test_the_contract_in_the_repo_passes(self):
        topics = check.load_index(check.CONTRACT_DIR)
        docs = (check.REPO_DIR / "docs" / "mqtt.md").read_text()
        self.assertEqual(check.check_contract(topics, docs), [])

    def test_a_topic_missing_from_the_docs_fails(self):
        topics = check.load_index(check.CONTRACT_DIR)
        failures = check.check_contract(topics, "")
        self.assertTrue(any("docs/mqtt.md" in f for f in failures), failures)

    def test_schema_problems_are_found(self):
        schema = {"type": "object", "properties": {"a": {"type": "string", "format": "date"},
                                                   "b": {"type": "object"}}}
        problems = check.schema_problems(schema, "#")
        self.assertEqual(len(problems), 3, problems)


if __name__ == "__main__":
    unittest.main()
