"""Compare a product's JSON Registry to its compiled production C descriptor."""

import json
import subprocess
import sys
from pathlib import Path


CLASSIFICATIONS = {
    "CORE": 0,
    "QUERY": 1,
    "DISCRETE_ACTION": 2,
    "CONTINUOUS_SESSION": 3,
    "SAFETY_STOP": 4,
}


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: verify_profile_descriptor.py REGISTRY_JSON TEST_EXECUTABLE", file=sys.stderr)
        return 2
    registry = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))["profile"]
    output = subprocess.run([sys.argv[2], "--describe"], check=True,
                            capture_output=True, text=True).stdout
    actual = [tuple(line.split("\t")) for line in output.splitlines()]
    expected = [("profile", registry["name"], registry["currentVersion"])]
    expected += [("command", command["name"],
                  str(CLASSIFICATIONS[command["classification"]]))
                 for command in registry["commands"]]
    expected += [("report", report["name"], report["reportType"])
                 for report in registry["reports"]]
    if len(actual) != len(expected) or actual[0] != expected[0] or sorted(actual[1:]) != sorted(expected[1:]):
        print(f"JSON Registry differs from compiled descriptor:\nexpected: {expected}\nactual: {actual}",
              file=sys.stderr)
        return 1
    print(f"verified C descriptor against {sys.argv[1]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
