"""Inject a protected station JSON through local SWD into an ABOX_PSK_STATION build.
Start a loopback J-Link GDB server for the intended board first. No secret CLI arguments.
Success means candidate accepted, NOT handshake, persistence or reboot verification.
"""
import argparse
import json
from pathlib import Path
import re
import secrets
import struct
import subprocess
import tempfile
import time
from mqtt_psk_files import validate


def encode(item):
    validate(item)
    def text(name, capacity):
        value = item.get(name, "")
        if not isinstance(value, str) or len(value) >= capacity or any(ord(c) < 33 or ord(c) > 126 or c in '\\"' for c in value):
            raise ValueError("invalid station field: " + name)
        return value.encode("ascii").ljust(capacity, b"\0")
    port = item.get("port")
    if type(port) is not int or not 1 <= port <= 65535:
        raise ValueError("invalid station port")
    return (struct.pack("<I", item["generation"]) + item["identity"].encode().ljust(32, b"\0") +
            item["secret"].encode().ljust(33, b"\0") + b"\1\0\0" +
            text("host", 64) + text("username", 32) + text("password", 64) + struct.pack("<HH", port, 0))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, required=True)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--gdb", required=True)
    parser.add_argument("--port", type=int, default=2331)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("invalid local GDB port")
    raw = encode(json.loads(args.bundle.read_text(encoding="ascii")))
    def run(commands):
        argv = [args.gdb, "-batch", str(args.elf), "-ex", "set pagination off",
                "-ex", f"target remote 127.0.0.1:{args.port}"]
        for command in commands: argv += ["-ex", command]
        argv += ["-ex", "detach"]
        result = subprocess.run(argv, capture_output=True, text=True, timeout=15)
        # Do not dump GDB diagnostics: they can contain station memory.
        if result.returncode or "Error" in result.stderr or "No symbol" in result.stderr:
            raise RuntimeError("station GDB operation failed; inspect locally without logging secret memory")
        return result.stdout
    address = run(["p/x (void*)&ABoxPskStation_Input"])
    match = re.search(r"= (0x[0-9a-fA-F]+)", address)
    if not match: raise RuntimeError("station symbols missing; use the matching maintenance ELF")
    request = secrets.randbelow(0xfffffffe) + 1
    with tempfile.TemporaryDirectory(prefix="abox-private-station-") as directory:
        path = Path(directory) / "input.bin"
        path.write_bytes(raw)
        run([f'restore "{path.as_posix()}" binary {match[1]}',
             f"set {{unsigned}} &ABoxPskStation_Request={request}"])
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            result = run(["x/1uw &ABoxPskStation_Completed", "x/1uw &ABoxPskStation_Result"])
            completed = re.search(r"<ABoxPskStation_Completed>:\s*(\d+)", result)
            accepted = re.search(r"<ABoxPskStation_Result>:\s*(\d+)", result)
            if completed and int(completed[1]) == request:
                if not accepted or int(accepted[1]) != 1: raise RuntimeError("candidate rejected")
                print("Candidate accepted. Verify Broker, saved configuration and ordinary reboot before retiring any identity.")
                return
            time.sleep(.2)
        raise RuntimeError("station result unknown; read current state before retrying")


if __name__ == "__main__":
    main()
