"""从本仓库的诊断应用与 Boot 生成烧录镜像和 MCU OTA 包。"""
from pathlib import Path
import argparse
import hashlib
import struct
import zlib

BASE_ADDRESS = 0x08000000
APP_OFFSET = 0x2000
ACTIVE_OFFSET = 0xE000
APP_VERSION = 0x000F0600
FILE_PREFIX = "feeder_0.15.6-public-bench"


def hex_record(address, kind, payload):
    data = struct.pack(">BHB", len(payload), address, kind) + payload
    return ":" + (data + bytes((-sum(data) & 255,))).hex().upper() + "\n"


def factory_hex(data):
    yield hex_record(0, 4, struct.pack(">H", BASE_ADDRESS >> 16))
    for offset in range(0, len(data), 16):
        yield hex_record(offset, 0, data[offset:offset + 16])
    yield hex_record(0, 1, b"")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    boot = (args.build / "feeder_ota_boot_1.0.0.bin").read_bytes()
    app = (args.build / "feeder_cycle_net_diag_0.15.6.bin").read_bytes()
    if not 192 < len(boot) <= APP_OFFSET or not 192 < len(app) <= 24448:
        raise SystemExit("Boot 或应用镜像越界")
    prefix = struct.pack(
        "<7I", 0x3141544F, 1, 0x46303330, APP_VERSION,
        len(app), zlib.crc32(app), BASE_ADDRESS + APP_OFFSET,
    )
    header = (prefix + struct.pack("<I", zlib.crc32(prefix))).ljust(128, b"\xff")
    factory = bytearray(b"\xff" * 65536)
    factory[:len(boot)] = boot
    factory[APP_OFFSET:APP_OFFSET + len(app)] = app
    factory[ACTIVE_OFFSET:ACTIVE_OFFSET + len(header)] = header
    outputs = {
        ".factory.hex": "".join(factory_hex(factory)).encode("ascii"),
        ".app.bin": app,
        ".ota.bin": header + app,
    }
    args.output.mkdir(parents=True, exist_ok=True)
    sums = []
    for suffix, contents in outputs.items():
        path = args.output / (FILE_PREFIX + suffix)
        path.write_bytes(contents)
        sums.append(f"{hashlib.sha256(contents).hexdigest()}  {path.name}\n")
    (args.output / "SHA256SUMS.txt").write_text("".join(sums), encoding="ascii")


if __name__ == "__main__":
    main()
