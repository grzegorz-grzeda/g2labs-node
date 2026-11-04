#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check the MQTT contract in contract/, and publishes against it.

Always checks the contract itself:
  - every schema is valid JSON Schema 2020-12, uses only core keywords and
    forbids additional properties on every object;
  - every topic has examples, and every example matches its topic;
  - docs/mqtt.md mentions every topic pattern in the index.

With dumps (files, or directories searched for contract_publishes.jsonl), it
also checks every recorded publish: its topic must be in the index (published
by --publisher, if given), its retained flag must match, and its payload must
be one of the allowed values or validate against the schema.

Run from anywhere:
  contract/check.py                                  # the contract only
  contract/check.py --publisher node twister-out     # plus the core tests' publishes
"""

import argparse
import json
import re
import sys
from pathlib import Path

from jsonschema import Draft202012Validator

CONTRACT_DIR = Path(__file__).resolve().parent
REPO_DIR = CONTRACT_DIR.parent
DUMP_NAME = "contract_publishes.jsonl"

# Keywords both the Python validator and ajv (in the Hub) treat the same way.
ALLOWED_KEYWORDS = {
    "$schema", "$id", "title", "description",
    "type", "properties", "required", "additionalProperties", "items",
    "enum", "minimum", "maximum", "pattern",
}


class Topic:
    def __init__(self, entry, placeholders, contract_dir):
        self.pattern = entry["pattern"]
        self.publisher = entry["publisher"]
        self.retained = entry["retained"]
        self.encoding = entry["encoding"]
        self.values = entry.get("values")
        self.examples = contract_dir / entry["examples"]
        self.schema_path = contract_dir / entry["schema"] if "schema" in entry else None
        self.schema = None
        if self.schema_path is not None:
            self.schema = json.loads(self.schema_path.read_text())

        regex = ""
        for part in re.split(r"(<[a-z-]+>)", self.pattern):
            if part.startswith("<"):
                regex += f"(?:{placeholders[part[1:-1]]})"
            else:
                regex += re.escape(part)
        self.regex = re.compile(regex)

    def matches(self, topic):
        return self.regex.fullmatch(topic) is not None

    def payload_errors(self, payload):
        """What's wrong with a payload (a str) on this topic; empty if nothing."""
        if self.encoding == "text":
            if payload not in self.values:
                return [f"payload must be one of {self.values}"]
            return []
        try:
            message = json.loads(payload)
        except json.JSONDecodeError as e:
            return [f"payload is not JSON: {e}"]
        validator = Draft202012Validator(self.schema)
        return [
            f"{'/'.join(str(p) for p in error.absolute_path) or '(root)'}: {error.message}"
            for error in sorted(validator.iter_errors(message), key=lambda e: list(e.path))
        ]


def load_index(contract_dir):
    index = json.loads((contract_dir / "topics.json").read_text())
    return [Topic(entry, index["placeholders"], contract_dir) for entry in index["topics"]]


def schema_problems(schema, where):
    """Keywords outside the core set, and objects that allow additional properties."""
    problems = []
    if not isinstance(schema, dict):
        return problems
    for keyword in schema:
        if keyword not in ALLOWED_KEYWORDS:
            problems.append(f"{where}: keyword '{keyword}' is not one of the core keywords")
    types = schema.get("type")
    types = types if isinstance(types, list) else [types]
    if "object" in types and schema.get("additionalProperties") is not False:
        problems.append(f"{where}: an object must set additionalProperties to false")
    for name, sub in schema.get("properties", {}).items():
        problems += schema_problems(sub, f"{where}/properties/{name}")
    if "items" in schema:
        problems += schema_problems(schema["items"], f"{where}/items")
    return problems


def check_contract(topics, docs):
    failures = []
    for topic in topics:
        if topic.schema is not None:
            name = topic.schema_path.relative_to(REPO_DIR)
            try:
                Draft202012Validator.check_schema(topic.schema)
            except Exception as e:  # jsonschema's SchemaError
                failures.append(f"{name}: not a valid JSON Schema 2020-12: {e}")
            if topic.schema.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
                failures.append(f"{name}: $schema must be JSON Schema 2020-12")
            failures += [f"{name}: {p}" for p in schema_problems(topic.schema, "#")]

        examples = sorted(topic.examples.glob("*")) if topic.examples.is_dir() else []
        if not examples:
            failures.append(f"{topic.pattern}: no examples in {topic.examples.relative_to(REPO_DIR)}")
        for example in examples:
            for error in topic.payload_errors(example.read_text()):
                failures.append(f"{example.relative_to(REPO_DIR)}: {error}")

        if f"`{topic.pattern}`" not in docs:
            failures.append(f"{topic.pattern}: not described in docs/mqtt.md")
    return failures


def dump_files(paths):
    files = []
    for path in paths:
        files += sorted(path.rglob(DUMP_NAME)) if path.is_dir() else [path]
    return files


def check_publish(topics, record, publisher):
    """What's wrong with one recorded publish; empty if nothing."""
    topic = next((t for t in topics if t.matches(record["topic"])), None)
    if topic is None:
        return ["topic is not in contract/topics.json"]
    errors = []
    if publisher is not None and topic.publisher != publisher:
        errors.append(f"topic is published by {topic.publisher}, not {publisher}")
    if record["retained"] != topic.retained:
        errors.append(f"retained must be {str(topic.retained).lower()}")
    return errors + topic.payload_errors(record["payload"])


def check_dumps(topics, files, publisher):
    failures = []
    count = 0
    for file in files:
        for line_no, line in enumerate(file.read_text().splitlines(), 1):
            if not line.strip():
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError as e:
                failures.append(f"{file}:{line_no}: not a JSON line: {e}")
                continue
            count += 1
            errors = check_publish(topics, record, publisher)
            if errors:
                failures.append(
                    f"{record['test']}: publish to {record['topic']}"
                    f" (retained {str(record['retained']).lower()})\n"
                    f"    payload: {record['payload']}\n"
                    + "".join(f"    error: {error}\n" for error in errors).rstrip("\n")
                )
    return count, failures


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("dumps", nargs="*", type=Path,
                        help=f"publish dumps, or directories to search for {DUMP_NAME}")
    parser.add_argument("--publisher", choices=["node", "hub"],
                        help="who made the publishes in the dumps")
    args = parser.parse_args(argv)

    topics = load_index(CONTRACT_DIR)
    docs = (REPO_DIR / "docs" / "mqtt.md").read_text()
    failures = check_contract(topics, docs)
    summary = f"{len(topics)} topics"

    if args.dumps:
        files = dump_files(args.dumps)
        if not files:
            failures.append(f"no {DUMP_NAME} found in {', '.join(map(str, args.dumps))}")
        count, dump_failures = check_dumps(topics, files, args.publisher)
        if files and count == 0:
            failures.append(f"no publishes recorded in {', '.join(map(str, files))}")
        failures += dump_failures
        summary += f", {count} publishes from {len(files)} dump(s)"

    for failure in failures:
        print(f"FAIL {failure}", file=sys.stderr)
    if failures:
        print(f"contract check failed: {len(failures)} problem(s) ({summary})", file=sys.stderr)
        return 1
    print(f"contract check passed ({summary})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
