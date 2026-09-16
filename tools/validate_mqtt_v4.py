#!/usr/bin/env python3
"""Read-only validator for the frozen MQTT V4 canonicalization vectors."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys


SAFE_INTEGER = 9007199254740991


class ContractError(ValueError):
    pass


def reject_duplicate_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ContractError(f"duplicate key: {key}")
        result[key] = value
    return result


def parse_integer(token: str) -> int:
    if token == "-0":
        raise ContractError("negative zero is forbidden")
    value = int(token, 10)
    if abs(value) > SAFE_INTEGER:
        raise ContractError("integer exceeds the safe range")
    return value


def reject_non_integer(token: str):
    raise ContractError(f"non-integer JSON number is forbidden: {token}")


def load_contract_json(text: str):
    return json.loads(
        text,
        object_pairs_hook=reject_duplicate_keys,
        parse_int=parse_integer,
        parse_float=reject_non_integer,
        parse_constant=reject_non_integer,
    )


def canonicalize(value) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def main() -> int:
    repo = Path(__file__).resolve().parents[1]
    vector_path = repo / "docs" / "protocols" / "v4" / "test-vectors.json"
    vectors = load_contract_json(vector_path.read_text(encoding="utf-8"))

    failures = []
    for vector in vectors["valid"]:
        actual = canonicalize(vector["value"])
        digest = hashlib.sha256(actual.encode("utf-8")).hexdigest()
        if actual != vector["canonical"]:
            failures.append(f"{vector['name']}: canonical bytes mismatch")
        if digest != vector["sha256"]:
            failures.append(f"{vector['name']}: SHA-256 mismatch")

    for vector in vectors["invalidJson"]:
        try:
            load_contract_json(vector["json"])
        except (ContractError, json.JSONDecodeError):
            continue
        failures.append(f"{vector['name']}: invalid JSON was accepted")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1

    print(f"validated {len(vectors['valid'])} valid and {len(vectors['invalidJson'])} invalid MQTT V4 vectors")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
