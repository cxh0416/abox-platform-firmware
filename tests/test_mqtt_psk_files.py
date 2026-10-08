import importlib.util
import json
from pathlib import Path
import tempfile
import sys
import struct
import unittest

spec = importlib.util.spec_from_file_location("psk_files", Path(__file__).parents[1] / "tools/mqtt_psk_files.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
sys.path.insert(0, str(Path(__file__).parents[1] / "tools"))
import mqtt_psk_station


class FilesTest(unittest.TestCase):
    def test_station_binary_layout_uses_literal_ascii(self):
        value = {"identity": "fixture.g1", "generation": 7, "secret": "abcdefghijklmnopqrstuvwxyz012345",
                 "host": "bench.invalid", "port": 26443, "username": "fixture", "password": "fixture"}
        raw = mqtt_psk_station.encode(value)
        self.assertEqual(len(raw), 236)
        self.assertEqual(raw[36:68], value["secret"].encode("ascii"))
        self.assertEqual(raw[69], 1)
        self.assertEqual(struct.unpack_from("<H", raw, 232)[0], 26443)

    def test_literal_ascii_encoding_and_only_forward_secret_suite(self):
        # Deterministic public fixture, not an actual station secret.
        item = {"identity": "fixture.g1", "generation": 1, "secret": "abcdefghijklmnopqrstuvwxyz012345"}
        with tempfile.TemporaryDirectory() as temporary:
            module.render([item], temporary)
            root = Path(temporary)
            self.assertEqual((root / "psk.secrets").read_text(), item["identity"] + ":" + item["secret"].encode().hex() + "\n")
            config = (root / "stunnel.conf").read_text()
            self.assertIn("ciphers = ECDHE-PSK-CHACHA20-POLY1305\n", config)
            self.assertIn("connect = 127.0.0.1:18884\n", config)
            self.assertIn("sslVersionMin = TLSv1.2\nsslVersionMax = TLSv1.2", config)
            self.assertIn("sessionResume = no", config)
            self.assertNotIn(item["secret"], config)
            with self.assertRaises(ValueError):
                module.render([item, item], temporary)
        for secret in ("x" * 31, "x" * 64, '"' + "x" * 31, "\n" + "x" * 31):
            with self.assertRaises(ValueError):
                module.validate({**item, "secret": secret})


if __name__ == "__main__":
    unittest.main()
