"""下载厂商原始依赖，不运行下载文件，也不连接硬件。"""
from pathlib import Path
import hashlib
import json
import subprocess
import zipfile
from urllib.parse import quote

ROOT = Path(__file__).resolve().parents[1]
DEPS = ROOT / ".deps"


def main():
    DEPS.mkdir(parents=True, exist_ok=True)
    lock = json.loads((ROOT / "dependencies.lock.json").read_text(encoding="utf-8"))
    for key in ("sdk", "pack"):
        spec = lock[key]
        path = DEPS / spec["file"]
        if not path.exists():
            # 先写临时文件；下载中断不会把残缺压缩包当成完整依赖。
            partial = path.with_suffix(path.suffix + ".partial")
            subprocess.run([
                "curl.exe", "--fail", "--location", "--silent", "--show-error",
                "--connect-timeout", "15", "--max-time", "180", "--retry", "1",
                quote(spec["url"], safe=":/"), "--output", str(partial),
            ], check=True)
            if hashlib.sha256(partial.read_bytes()).hexdigest() != spec["sha256"]:
                raise RuntimeError(f"Downloaded checksum mismatch: {partial}")
            partial.replace(path)
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != spec["sha256"]:
            raise RuntimeError(f"Dependency checksum mismatch; keep file for diagnosis: {path}")
        print(json.dumps({"file": path.name, "bytes": path.stat().st_size, "sha256": digest}))

    sdk = DEPS / "PY32F0xx_Firmware_V1.5.0"
    marker = sdk / ".feeder-extracted.json"
    if not marker.exists():
        with zipfile.ZipFile(DEPS / lock["sdk"]["file"]) as archive:
            # 下载包属于外部输入，解压前检查所有目标都位于依赖目录内。
            for item in archive.infolist():
                target = (DEPS / item.filename).resolve()
                if DEPS.resolve() not in target.parents:
                    raise RuntimeError(f"Unsafe archive path: {item.filename}")
            archive.extractall(DEPS)
        marker.write_text(json.dumps({"sha256": lock["sdk"]["sha256"]}), encoding="utf-8")
    print(f"SDK ready: {sdk}")


if __name__ == "__main__":
    main()
