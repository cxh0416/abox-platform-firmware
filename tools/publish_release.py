"""Validate a product-declared Boot V2 bundle and replace dist atomically.

Products keep their existing manifest schema and build script. The contract is
explicit data; no product identity or Flash end address is inferred from paths.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import struct
import uuid
import zlib


def _inside(root: Path, path: Path) -> Path:
    root = Path(os.path.abspath(root))
    path = Path(os.path.abspath(path))
    if path == root or not path.is_relative_to(root):
        raise ValueError(f"path outside product root: {path}")
    for item in (path, *path.parents):
        if not item.exists() and not item.is_symlink():
            continue
        info = item.lstat()
        if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
            raise ValueError(f"reparse point in managed path: {item}")
        if item == root:
            break
    return path


def _remove(root: Path, path: Path) -> None:
    path = _inside(root, path)
    if path.is_dir():
        for child in path.rglob("*"):
            _inside(root, child)
        shutil.rmtree(path)
    elif path.exists():
        path.unlink()


def _check_tree(root: Path, path: Path) -> None:
    _inside(root, path)
    if path.is_dir():
        for child in path.rglob("*"):
            _inside(root, child)


def _image_vectors(image: bytes, base: int) -> None:
    if len(image) < 8:
        raise ValueError("image is too short for vectors")
    sp, reset = struct.unpack_from("<II", image)
    if not (0x20000000 <= sp <= 0x20010000 and sp % 8 == 0 and
            reset & 1 and base <= (reset & ~1) < base + len(image)):
        raise ValueError(f"invalid vectors at 0x{base:08X}")


def _artifact_matches(item: dict, path: Path, address: int) -> None:
    data = path.read_bytes()
    if item.get("name") != path.name or item.get("length", item.get("size")) != len(data):
        raise ValueError(f"artifact name/length mismatch: {path.name}")
    if str(item.get("sha256", "")).lower() != hashlib.sha256(data).hexdigest():
        raise ValueError(f"artifact hash mismatch: {path.name}")
    if "crc32" in item and str(item["crc32"]).lower() != f"{zlib.crc32(data):08x}":
        raise ValueError(f"artifact CRC mismatch: {path.name}")
    if "load_address" in item and str(item["load_address"]).lower() != f"0x{address:08x}":
        raise ValueError(f"artifact address mismatch: {path.name}")
    if item.get("includes_device_config") is True or item.get("includes_boot_state") is True:
        raise ValueError(f"artifact includes reserved pages: {path.name}")


def validate(stage: Path, contract: dict) -> None:
    stage = stage.absolute()
    if not stage.is_dir() or stage.is_symlink():
        raise ValueError("stage is not a regular directory")
    required = {contract[key] for key in ("app_name", "boot_name", "full_name")}
    required.update(contract.get("manifest_files", ["release.manifest.json"]))
    if {path.name for path in stage.iterdir()} != required or any(
            not path.is_file() or path.is_symlink() for path in stage.iterdir()):
        raise ValueError("unexpected release file set")
    release = json.loads((stage / "release.manifest.json").read_text(encoding="utf-8-sig"))
    version = release.get("version")
    if release.get("product") != contract["product"] or not isinstance(version, str) or not version:
        raise ValueError("release identity mismatch")
    for key, value in contract.get("manifest_constraints", {}).items():
        if release.get(key) != value:
            raise ValueError(f"release manifest constraint failed: {key}")
    app_path = stage / contract["app_name"]
    boot_path = stage / contract["boot_name"]
    full_path = stage / contract["full_name"]
    app, boot, full = app_path.read_bytes(), boot_path.read_bytes(), full_path.read_bytes()
    offset = int(contract.get("app_offset", 0x8000))
    app_start = int(contract.get("app_start", 0x08008000))
    if offset != app_start - 0x08000000 or len(boot) != offset:
        raise ValueError("Boot/App offset mismatch")
    if not 8 <= len(app) <= int(contract["app_max_size"]):
        raise ValueError("App exceeds declared Flash region")
    if hashlib.sha256(boot).hexdigest().lower() != contract["frozen_boot_sha256"].lower():
        raise ValueError("frozen Boot mismatch")
    _image_vectors(boot, 0x08000000)
    _image_vectors(app, app_start)
    if full != boot + app:
        raise ValueError("Full image differs from exact Boot + App")
    marker = (contract["version_marker_prefix"] + version).encode("ascii") + b"\0"
    if marker not in app:
        raise ValueError("App version marker mismatch")
    artifact_manifest = release
    if not isinstance(release.get("artifacts"), list):
        artifact_manifest = json.loads((stage / "manifest.json").read_text(encoding="utf-8-sig"))
        if artifact_manifest.get("product") != contract["product"] or artifact_manifest.get("firmware_version") != version:
            raise ValueError("artifact manifest identity mismatch")
    if artifact_manifest.get("configuration") != "Release":
        raise ValueError("formal bundle is not a Release build")
    artifacts = artifact_manifest.get("artifacts")
    if not isinstance(artifacts, list) or {item.get("name") for item in artifacts} != {
            contract["app_name"], contract["boot_name"], contract["full_name"]}:
        raise ValueError("artifact manifest set mismatch")
    for item in artifacts:
        _artifact_matches(item, stage / item["name"],
                          app_start if item["name"] == contract["app_name"] else 0x08000000)
    if "file" in release and release["file"] != contract["app_name"]:
        raise ValueError("release App filename mismatch")
    if "sha256" in release and str(release["sha256"]).lower() != hashlib.sha256(app).hexdigest():
        raise ValueError("release App hash mismatch")
    if "size" in release and release["size"] != len(app):
        raise ValueError("release App size mismatch")
    if "crc32" in release and str(release["crc32"]).lower() != f"{zlib.crc32(app):08x}":
        raise ValueError("release App CRC mismatch")


def publish(root: Path, stage: Path, contract: dict) -> None:
    root = root.absolute()
    if not root.is_dir() or root.is_symlink():
        raise ValueError("invalid product root")
    target = _inside(root, root / "dist")
    stage = stage.absolute()
    if stage == target or stage.is_relative_to(target) or target.is_relative_to(stage):
        raise ValueError("stage and dist overlap")
    validate(stage, contract)
    token = uuid.uuid4().hex
    incoming = _inside(root, root / f".dist-incoming-{token}")
    backup = _inside(root, root / f".dist-backup-{token}")
    lock = _inside(root, root / ".dist-publish.lock")
    handle = lock.open("x")
    try:
        try:
            shutil.copytree(stage, incoming)
            validate(incoming, contract)
            if target.exists():
                _check_tree(root, target)
                os.replace(target, backup)
            try:
                os.replace(incoming, target)
            except BaseException:
                if backup.exists():
                    os.replace(backup, target)
                raise
            if backup.exists():
                _remove(root, backup)
        finally:
            if incoming.exists():
                _remove(root, incoming)
            if backup.exists() and not target.exists():
                os.replace(backup, target)
    finally:
        handle.close()
        lock.unlink()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--stage", type=Path, required=True)
    parser.add_argument("--product-root", type=Path, required=True)
    parser.add_argument("--validate-only", action="store_true")
    args = parser.parse_args()
    contract = json.loads(args.contract.read_text(encoding="utf-8"))
    if args.validate_only:
        validate(args.stage, contract)
    else:
        publish(args.product_root, args.stage, contract)


if __name__ == "__main__":
    main()
