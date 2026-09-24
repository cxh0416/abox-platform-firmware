"""Exercise configure-time component and hardware gates with synthetic users."""

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def configure(name: str, arguments: str, success: bool, diagnostic: str = "") -> None:
    with tempfile.TemporaryDirectory(prefix=f"cmake-{name}-", dir=ROOT / "build") as folder:
        source = Path(folder)
        config = source / "config"
        config.mkdir()
        (config / "abox_product_config.h").write_text(
            "/* synthetic hardware contract */\nABOX_SCHEDULER_BAREMETAL\n", encoding="utf-8")
        (source / "consumer.c").write_text("int consumer(void) { return 0; }\n", encoding="utf-8")
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.22)\n"
            "project(component_contract C)\n"
            f"add_subdirectory(\"{ROOT.as_posix()}\" platform-build)\n"
            "add_library(consumer STATIC consumer.c)\n"
            f"set(ABOX_PRODUCT_CONFIG_DIR \"{config.as_posix()}\")\n"
            f"abox_platform_attach_components(consumer {arguments})\n",
            encoding="utf-8")
        result = subprocess.run(["cmake", "-S", str(source), "-B", str(source / "build"),
                                 "-G", "Ninja", "-DBUILD_TESTING=OFF"],
                                capture_output=True, text=True)
        output = result.stdout + result.stderr
        assert (result.returncode == 0) == success, f"{name}: {output}"
        if diagnostic:
            assert diagnostic in output, f"{name}: {output}"


def main() -> None:
    (ROOT / "build").mkdir(exist_ok=True)
    configure("generic-mqtt", "SCHEDULER BAREMETAL COMPONENTS core mqtt_v4", True)
    configure("hardware", "SCHEDULER BAREMETAL HARDWARE stm32f105_ec800_v1 COMPONENTS core", True)
    configure("hardware-mismatch", "SCHEDULER FREERTOS HARDWARE stm32f105_ec800_v1 COMPONENTS core",
              False, "scheduler disagrees")
    configure("scheduler", "SCHEDULER UNKNOWN COMPONENTS core", False, "SCHEDULER")
    configure("hardware-unknown", "SCHEDULER BAREMETAL HARDWARE unknown COMPONENTS core", False, "HARDWARE")
    configure("missing-dependency", "SCHEDULER BAREMETAL COMPONENTS mqtt_runtime", False, "requires")
    configure("legacy-ota", "SCHEDULER BAREMETAL COMPONENTS ota", False, "legacy Boot/OTA")


if __name__ == "__main__":
    main()
