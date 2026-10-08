"""Create protected station bundles or render a PSK-only stunnel listener.

Never pass a secret on the command line. Input/output paths must stay outside
Git and firmware packages. Broker username/password provisioning is separate.
"""
import argparse
import json
import os
from pathlib import Path
import secrets
import tempfile


def valid_text(value, maximum, exact=False):
    return (isinstance(value, str) and (len(value) == maximum if exact else 0 < len(value) <= maximum)
            and all(0x21 <= ord(ch) <= 0x7e and ch not in '\\":' for ch in value))


def validate(item):
    if (not valid_text(item.get("identity"), 31) or not valid_text(item.get("secret"), 32, True)
            or type(item.get("generation")) is not int or not 0 < item["generation"] <= 0xffffffff):
        raise ValueError("invalid PSK identity, generation or 32 ASCII secret")
    return item


def protected_write(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    fd, name = tempfile.mkstemp(dir=path.parent)
    try:
        os.fchmod(fd, 0o600) if hasattr(os, "fchmod") else None
        with os.fdopen(fd, "w", encoding="ascii", newline="\n") as stream:
            stream.write(value)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def render(items, output, listen_port=26443, backend_port=18884):
    if not 1 <= listen_port <= 65535 or not 1 <= backend_port <= 65535:
        raise ValueError("invalid port")
    ids = set()
    lines = []
    for item in items:
        validate(item)
        if item["identity"] in ids:
            raise ValueError("duplicate PSK identity")
        ids.add(item["identity"])
        # stunnel takes HEX of the modem's literal ASCII bytes.
        lines.append(item["identity"] + ":" + item["secret"].encode("ascii").hex())
    if not lines:
        raise ValueError("empty PSK inventory")
    output = Path(output).resolve()
    if any(ch in str(output) for ch in "\r\n"):
        raise ValueError("invalid path")
    protected_write(output / "psk.secrets", "\n".join(lines) + "\n")
    protected_write(output / "stunnel.conf", f"""foreground = yes
pid =
debug = 5
[abox-mqtt-psk]
accept = 0.0.0.0:{listen_port}
connect = 127.0.0.1:{backend_port}
PSKsecrets = {output / 'psk.secrets'}
sslVersionMin = TLSv1.2
sslVersionMax = TLSv1.2
ciphers = ECDHE-PSK-CHACHA20-POLY1305
sessionResume = no
options = NO_TICKET
TIMEOUTclose = 0
""")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    actions = parser.add_subparsers(dest="action", required=True)
    new = actions.add_parser("generate")
    new.add_argument("--identity", required=True)
    new.add_argument("--generation", type=int, required=True)
    new.add_argument("--output", type=Path, required=True)
    front = actions.add_parser("render")
    front.add_argument("--inventory", type=Path, required=True)
    front.add_argument("--output", type=Path, required=True)
    front.add_argument("--listen-port", type=int, default=26443)
    front.add_argument("--backend-port", type=int, default=18884)
    args = parser.parse_args()
    os.umask(0o077)
    if args.action == "generate":
        if args.output.exists():
            parser.error("station bundle already exists; use a new generation and path")
        item = validate({"identity": args.identity, "generation": args.generation,
                         "secret": secrets.token_urlsafe(24)})
        protected_write(args.output, json.dumps(item, separators=(",", ":")) + "\n")
    else:
        items = json.loads(args.inventory.read_text(encoding="ascii"))
        render(items, args.output, args.listen_port, args.backend_port)
    print("Protected files written; no secrets printed.")


if __name__ == "__main__":
    main()
