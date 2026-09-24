"""Reject product Profile names in public Platform implementation surfaces."""

import json
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: check_profile_boundary.py PRODUCT_REGISTRY_JSON...", file=sys.stderr)
        return 2
    root = Path(__file__).resolve().parents[1]
    names = {
        json.loads(Path(path).read_text(encoding="utf-8"))["profile"]["name"]
        for path in sys.argv[1:]
    }
    files = [root / "CMakeLists.txt", *root.glob("cmake/**/*.cmake"),
             *root.glob("components/**/*.c"), *root.glob("include/**/*.h"),
             root / "tools/validate_mqtt_v4.py"]
    failures = []
    for path in files:
        content = path.read_text(encoding="utf-8")
        for name in names:
            if name in content:
                failures.append(f"{path.relative_to(root)} contains product Profile {name}")
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print(f"checked {len(files)} public Platform files against {len(names)} product Profiles")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
