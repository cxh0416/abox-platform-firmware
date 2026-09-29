"""Failure gates for staging and replacing a frozen-Boot firmware bundle."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from publish_release import publish, validate  # noqa: E402


def artifact(name: str, data: bytes, address: str) -> dict:
    return {"name": name, "length": len(data), "load_address": address,
            "sha256": hashlib.sha256(data).hexdigest(),
            "crc32": f"{zlib.crc32(data):08x}",
            "includes_device_config": False, "includes_boot_state": False}


class ReleaseTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "product"
        self.root.mkdir()
        self.stage = Path(self.temp.name) / "stage"
        self.stage.mkdir()
        self.boot = bytearray(b"\xff" * 0x8000)
        self.app = bytearray(b"\xff" * 256)
        struct.pack_into("<II", self.boot, 0, 0x20010000, 0x08000009)
        struct.pack_into("<II", self.app, 0, 0x2000F000, 0x08008009)
        self.app[32:55] = b"FW_VERSION=product-1.0\0"
        self.contract = {"product": "product", "app_name": "App.bin",
                         "boot_name": "Boot.bin", "full_name": "Full.bin",
                         "app_max_size": 0x36000,
                         "frozen_boot_sha256": hashlib.sha256(self.boot).hexdigest(),
                         "version_marker_prefix": "FW_VERSION=",
                         "manifest_files": ["release.manifest.json"],
                         "manifest_constraints": {"configuration": "Release"}}
        self.write_stage()

    def write_stage(self) -> None:
        app, boot = bytes(self.app), bytes(self.boot)
        full = boot + app
        for name, data in (("App.bin", app), ("Boot.bin", boot), ("Full.bin", full)):
            (self.stage / name).write_bytes(data)
        manifest = {"product": "product", "version": "product-1.0",
                    "configuration": "Release", "artifacts": [
                        artifact("App.bin", app, "0x08008000"),
                        artifact("Boot.bin", boot, "0x08000000"),
                        artifact("Full.bin", full, "0x08000000")]}
        (self.stage / "release.manifest.json").write_text(json.dumps(manifest), encoding="utf-8")

    def test_valid_bundle_replaces_only_dist(self) -> None:
        old = self.root / "dist"
        old.mkdir()
        (old / "old.txt").write_text("previous", encoding="utf-8")
        publish(self.root, self.stage, self.contract)
        self.assertEqual({path.name for path in old.iterdir()},
                         {"App.bin", "Boot.bin", "Full.bin", "release.manifest.json"})
        self.assertEqual((old / "Full.bin").read_bytes(), bytes(self.boot + self.app))
        self.assertFalse(list(self.root.glob(".dist-backup-*")))
        self.assertFalse((self.root / ".dist-publish.lock").exists())

    def test_rejected_stage_preserves_existing_dist(self) -> None:
        old = self.root / "dist"
        old.mkdir()
        (old / "old.txt").write_text("previous", encoding="utf-8")
        (self.stage / "Full.bin").write_bytes(b"corrupt")
        with self.assertRaises(ValueError):
            publish(self.root, self.stage, self.contract)
        self.assertEqual((old / "old.txt").read_text(encoding="utf-8"), "previous")

    def test_failed_swap_restores_previous_dist(self) -> None:
        old = self.root / "dist"
        old.mkdir()
        (old / "old.txt").write_text("previous", encoding="utf-8")
        real_replace = os.replace

        def fail_incoming(source: Path, destination: Path) -> None:
            if source.name.startswith(".dist-incoming-"):
                raise OSError("injected swap failure")
            real_replace(source, destination)

        with patch("publish_release.os.replace", side_effect=fail_incoming):
            with self.assertRaisesRegex(OSError, "injected"):
                publish(self.root, self.stage, self.contract)
        self.assertEqual((old / "old.txt").read_text(encoding="utf-8"), "previous")
        self.assertFalse(list(self.root.glob(".dist-backup-*")))

    def test_split_manifest_schema_is_validated(self) -> None:
        artifact_manifest = json.loads((self.stage / "release.manifest.json").read_text(encoding="utf-8"))
        artifact_manifest["firmware_version"] = artifact_manifest.pop("version")
        (self.stage / "manifest.json").write_text(json.dumps(artifact_manifest), encoding="utf-8")
        release = {"product": "product", "version": "product-1.0", "file": "App.bin",
                   "sha256": hashlib.sha256(self.app).hexdigest(), "configuration": "Release"}
        (self.stage / "release.manifest.json").write_text(json.dumps(release), encoding="utf-8")
        self.contract["manifest_files"] = ["manifest.json", "release.manifest.json"]
        validate(self.stage, self.contract)
        release["sha256"] = "0" * 64
        (self.stage / "release.manifest.json").write_text(json.dumps(release), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "release App hash"):
            validate(self.stage, self.contract)

    def test_frozen_boot_and_flash_bounds_are_enforced(self) -> None:
        self.contract["frozen_boot_sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "frozen Boot"):
            validate(self.stage, self.contract)
        self.contract["frozen_boot_sha256"] = hashlib.sha256(self.boot).hexdigest()
        self.contract["app_max_size"] = 128
        with self.assertRaisesRegex(ValueError, "Flash region"):
            validate(self.stage, self.contract)

    def test_manifest_marker_and_reserved_pages_are_enforced(self) -> None:
        manifest_path = self.stage / "release.manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["artifacts"][0]["includes_boot_state"] = True
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "reserved pages"):
            validate(self.stage, self.contract)
        self.write_stage()
        self.contract["version_marker_prefix"] = "WRONG="
        with self.assertRaisesRegex(ValueError, "version marker"):
            validate(self.stage, self.contract)

    def test_stage_cannot_be_dist(self) -> None:
        publish(self.root, self.stage, self.contract)
        with self.assertRaisesRegex(ValueError, "overlap"):
            publish(self.root, self.root / "dist", self.contract)


if __name__ == "__main__":
    unittest.main()
