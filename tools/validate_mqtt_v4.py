#!/usr/bin/env python3
"""Read-only validator for the frozen MQTT V4 canonicalization vectors."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re
import sys


SAFE_INTEGER = 9007199254740991
SNAKE_CASE = re.compile(r"^[a-z][a-z0-9]*(?:_[a-z0-9]+)*$")
PROFILE_VERSION = re.compile(r"^(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})$")


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


def validate_registry(registry) -> list[str]:
    failures = []
    names = set()
    matchers = set()

    def register_operation(kind: str, operation) -> None:
        name = operation.get("name")
        matcher = operation.get("operationMatcher", {})
        matcher_key = (matcher.get("field"), matcher.get("equals"))
        if not isinstance(name, str) or len(name) > 64 or not SNAKE_CASE.fullmatch(name):
            failures.append(f"{kind}: invalid operation name {name!r}")
        elif name in names:
            failures.append(f"{kind}: duplicate operation name {name}")
        else:
            names.add(name)
        if matcher.get("equals") != name:
            failures.append(f"{kind} {name}: matcher value must equal the operation name")
        if matcher_key in matchers:
            failures.append(f"{kind} {name}: conflicting operationMatcher {matcher_key}")
        else:
            matchers.add(matcher_key)

    if registry.get("protocolVersion") != "4.0":
        failures.append("registry protocolVersion must be 4.0")

    for command in registry.get("core", {}).get("commands", []):
        register_operation("core command", command)

    profile_names = set()
    for profile in registry.get("profiles", []):
        name = profile.get("name")
        version = profile.get("currentVersion")
        majors = profile.get("supportedMajors", [])
        if not isinstance(name, str) or len(name) > 64 or not SNAKE_CASE.fullmatch(name):
            failures.append(f"invalid profile name {name!r}")
        elif name in profile_names:
            failures.append(f"duplicate profile name {name}")
        else:
            profile_names.add(name)
        match = PROFILE_VERSION.fullmatch(version or "")
        if not match:
            failures.append(f"profile {name}: invalid currentVersion {version!r}")
        elif int(match.group(1)) not in majors:
            failures.append(f"profile {name}: current major is not supported")
        if not profile.get("contract"):
            failures.append(f"profile {name}: contract reference is required")
        for command in profile.get("commands", []):
            register_operation(f"profile {name} command", command)
        for report in profile.get("reports", []):
            register_operation(f"profile {name} report", report)

    if not registry.get("profiles"):
        failures.append("registry must contain at least one frozen profile")
    return failures


def main() -> int:
    repo = Path(__file__).resolve().parents[1]
    vector_path = repo / "docs" / "protocols" / "v4" / "test-vectors.json"
    registry_path = repo / "docs" / "protocols" / "v4" / "registry.json"
    vectors = load_contract_json(vector_path.read_text(encoding="utf-8"))
    registry = load_contract_json(registry_path.read_text(encoding="utf-8"))

    failures = validate_registry(registry)
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

    print(
        f"validated {len(vectors['valid'])} valid and "
        f"{len(vectors['invalidJson'])} invalid MQTT V4 vectors; "
        f"registry contains {len(registry['profiles'])} profile"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
