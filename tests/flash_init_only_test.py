"""烧录事务的离线故障注入；所有主控与探针都是替身，不连接USB。"""
import contextlib
import builtins
import importlib.util
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import MagicMock, patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
spec = importlib.util.spec_from_file_location("flash_init", ROOT / "tools/flash_init_only.py")
flash = importlib.util.module_from_spec(spec)
spec.loader.exec_module(flash)


class CoreDouble:
    def __init__(self):
        self.memory = bytearray(b"\xff" * flash.FLASH_BYTES)
        struct.pack_into("<II", self.memory, 0, 0x20002000, 0x08000101)
        self.option = 0xAA
        self.halted = True
        self.read_count = 0
        self.unstable = False
        self.events = []

    def is_halted(self):
        return self.halted

    def halt(self):
        self.halted = True
        self.events.append("halt")

    def read32(self, address):
        if address != flash.OPTR:
            raise AssertionError("Unexpected register read in program transaction")
        return self.option

    def read_memory_block8(self, address, length):
        self.events.append(f"read:{length}")
        self.read_count += 1
        data = list(self.memory[address - flash.FLASH_START:address - flash.FLASH_START + length])
        if self.unstable and self.read_count == 2:
            data[-1] ^= 1
        return data


class FlashTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.core = CoreDouble()
        self.image = struct.pack("<II", 0x20002000, 0x08000011) + b"INIT-ONLY-DATA"
        self.uid = "offline-probe-double"
        self.output = io.StringIO()
        self.silence = contextlib.redirect_stdout(self.output)
        self.silence.__enter__()

    def tearDown(self):
        self.silence.__exit__(None, None, None)
        self.temp.cleanup()

    def program(self):
        # 写入之前必须已保存完整、双读一致的旧主Flash及元数据。
        backups = list(self.root.glob(".tools/backups/*/original-flash-64k.bin"))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_bytes(), bytes(self.core.memory))
        record = json.loads(backups[0].with_name("backup.json").read_text(encoding="utf-8"))
        self.assertEqual(record["sha256"], flash.digest(bytes(self.core.memory)))
        self.assertEqual(record["option_register"], self.core.option)
        self.assertFalse(record["restore_tested"])
        self.core.events.append("program")
        self.core.memory[:len(self.image)] = self.image

    def transaction(self, program=None):
        flash.program_and_verify(self.core, self.uid, self.root, self.image, program or self.program)

    def test_success_backups_before_program_and_checks_readback(self):
        self.transaction()
        self.assertEqual(self.core.events, ["read:65536", "read:65536", "program", "halt",
                                            f"read:{len(self.image)}"])
        self.assertIn("WRITE VERIFY PASS", self.output.getvalue())

    def test_running_core_refuses_write(self):
        self.core.halted = False
        with self.assertRaisesRegex(RuntimeError, "未暂停"):
            self.transaction()
        self.assertEqual(self.core.events, [])

    def test_preparation_only_after_backup_and_before_program(self):
        def prepare():
            self.assertEqual(len(list(self.root.glob(".tools/backups/*/backup.json"))), 1)
            self.core.events.append("prepare")
        flash.program_and_verify(self.core, self.uid, self.root, self.image, self.program, prepare=prepare)
        self.assertLess(self.core.events.index("prepare"), self.core.events.index("program"))
        self.assertEqual(self.core.events[:2], ["read:65536", "read:65536"])

    def test_preparation_failure_never_erases_or_retries(self):
        prepare = MagicMock(side_effect=RuntimeError("preparation failed"))
        program = MagicMock()
        with self.assertRaises(RuntimeError):
            flash.program_and_verify(self.core, self.uid, self.root, self.image, program, prepare=prepare)
        prepare.assert_called_once_with(); program.assert_not_called()

    def test_algorithm_preparation_checks_interrupt_quiescence(self):
        core = MagicMock(); core.is_halted.return_value = True
        core.read_core_register.side_effect = lambda name: {"ipsr": 0, "primask": 1}[name]
        core.read32.return_value = 0
        flash.prepare_flash_algorithm(core, "explicit-software-reset")
        core.reset_and_halt.assert_called_once_with("explicit-software-reset")
        core.write_core_register.assert_called_once_with("primask", 1)
        core.read32.side_effect = lambda register: 4 if register == 0xE000E010 else 0
        flash.prepare_flash_algorithm(core, "explicit-software-reset")
        for address in (0xE000E010, 0xE000E100):
            core.read32.side_effect = lambda register, bad=address: 7 if register == bad else 0
            with self.assertRaises(RuntimeError): flash.prepare_flash_algorithm(core, "explicit-software-reset")
        core.read32.return_value = 0; core.read32.side_effect = None
        core.read_core_register.side_effect = lambda name: 3 if name == "ipsr" else 1
        with self.assertRaises(RuntimeError): flash.prepare_flash_algorithm(core, "explicit-software-reset")

    def test_protected_device_never_unlocks_or_programs(self):
        self.core.option = 0xBB
        with self.assertRaisesRegex(RuntimeError, "不解锁"):
            self.transaction()
        self.assertEqual(self.core.events, [])

    def test_inconsistent_backup_prevents_programming(self):
        self.core.unstable = True
        with self.assertRaisesRegex(RuntimeError, "双读不一致"):
            self.transaction()
        self.assertNotIn("program", self.core.events)
        self.assertEqual(list(self.root.glob(".tools/backups/*")), [])

    def test_backup_disk_failure_prevents_programming(self):
        with patch.object(Path, "write_bytes", side_effect=OSError("offline disk failure")):
            with self.assertRaises(OSError):
                self.transaction()
        self.assertNotIn("program", self.core.events)

    def test_programming_failure_is_not_retried(self):
        attempts = []

        def failing_program():
            attempts.append(1)
            raise RuntimeError("offline transport failure")

        with self.assertRaisesRegex(RuntimeError, "transport"):
            self.transaction(failing_program)
        self.assertEqual(attempts, [1])
        self.assertNotIn("WRITE VERIFY PASS", self.output.getvalue())

    def test_readback_mismatch_never_reports_success(self):
        def corrupt_program():
            self.program()
            self.core.memory[12] ^= 1

        with self.assertRaisesRegex(RuntimeError, "读回不一致"):
            self.transaction(corrupt_program)
        self.assertNotIn("WRITE VERIFY PASS", self.output.getvalue())

    def test_option_change_never_reports_success(self):
        def changed_option_program():
            self.program()
            self.core.option ^= 0x100

        with self.assertRaisesRegex(RuntimeError, "选项状态变化"):
            self.transaction(changed_option_program)
        self.assertNotIn("WRITE VERIFY PASS", self.output.getvalue())

    def test_isolation_confirmation_must_be_explicit(self):
        for value in ("", "取消", "true", "机构无人"):
            with self.subTest(value=value):
                with self.assertRaisesRegex(RuntimeError, "未连接硬件"):
                    flash.require_isolation(value)
        flash.require_isolation(flash.POWER_ISOLATED)

    def test_terminal_confirmation_is_explicit_and_cancellable(self):
        with patch.object(builtins, "input", return_value="1"):
            self.assertEqual(flash.confirm_isolation(), flash.POWER_ISOLATED)
        for value in ("", "0", "取消", "yes"):
            with self.subTest(value=value), patch.object(builtins, "input", return_value=value):
                with self.assertRaisesRegex(RuntimeError, "未连接硬件"):
                    flash.confirm_isolation()
        with patch.object(builtins, "input", side_effect=EOFError):
            with self.assertRaisesRegex(RuntimeError, "未连接硬件"):
                flash.confirm_isolation()

    def test_check_only_and_cancel_never_import_debug_tools(self):
        path = self.root / "offline.hex"
        path.write_bytes(b"offline-package-placeholder")
        image = ({"hex": path}, self.image, self.root / "offline.pack", 0x20000000)
        original_import = builtins.__import__

        def forbid_debug_import(name, *args, **kwargs):
            if name == "pyocd" or name.startswith("pyocd."):
                self.fail("Offline/cancel path imported hardware tools")
            return original_import(name, *args, **kwargs)

        with patch.object(flash, "checked_image", return_value=image), \
                patch.object(builtins, "__import__", side_effect=forbid_debug_import):
            self.assertEqual(flash.main(["--check-only"]), 0)
            with self.assertRaisesRegex(RuntimeError, "未连接硬件"):
                flash.main(["--motor-power-state", "取消"])
            with patch.object(builtins, "input", return_value=""):
                with self.assertRaisesRegex(RuntimeError, "未连接硬件"):
                    flash.main(["--confirm-motor-power"])

    def test_user_requested_auto_download_never_prompts_or_fakes_isolation(self):
        path = self.root / "offline.hex"
        path.write_bytes(b"offline-package-placeholder")
        image = ({"hex": path}, self.image, self.root / "offline.pack", 0x20000000)
        with patch.object(flash, "checked_image", return_value=image), \
                patch.object(flash, "connect_and_program") as connect, \
                patch.object(flash, "confirm_isolation", side_effect=AssertionError("must not prompt")), \
                patch.object(flash, "require_isolation", side_effect=AssertionError("must not fake isolation")):
            self.assertEqual(flash.main(["--auto-download"]), 0)
            connect.assert_called_once_with(*image, None)
            connect.reset_mock()
            self.assertEqual(flash.main(["--check-only", "--auto-download"]), 0)
            connect.assert_not_called()

    def test_probe_selection_never_picks_between_multiple_devices(self):
        a = SimpleNamespace(unique_id="A", description="ARM CMSIS-DAP")
        b = SimpleNamespace(unique_id="B", description="ARM CMSIS-DAP")
        self.assertIs(flash.select_probe([a]), a)
        self.assertIs(flash.select_probe([a, b], "B"), b)
        for probes, uid in (([], None), ([a, b], None), ([a], "B")):
            with self.assertRaises(RuntimeError):
                flash.select_probe(probes, uid)
        with self.assertRaisesRegex(RuntimeError, "不是CMSIS-DAP"):
            flash.select_probe([SimpleNamespace(unique_id="C", description="Different probe")])

    def test_download_preserves_vendor_init_without_unlock_or_unwired_reset(self):
        pack = self.root / "vendor.pack"
        options = flash.download_options(pack)
        self.assertEqual(options["target_override"], "py32f030x8")
        self.assertEqual(options["pack"], str(pack))
        self.assertEqual(options["dap_protocol"], "swd")
        self.assertEqual(options["frequency"], 100000)
        self.assertEqual(options["connect_mode"], "halt")
        self.assertTrue(options["pack.debug_sequences.enable"])
        self.assertFalse(options["auto_unlock"])
        self.assertFalse(options["resume_on_disconnect"])


class BootTests(unittest.TestCase):
    def make_core(self, version=0x00020100, tick_delta=100, mode=0x00410000, odr=0):
        words = [0x46494E49, version, 100, 8000000, 1, 0, 0, mode, odr, 0]
        first = words.copy()
        second = words.copy()
        second[2] += tick_delta
        values = iter((first, second))
        return SimpleNamespace(read_memory_block32=lambda _a, _n: next(values),
                               read32=lambda address: mode if address == 0x50000000 else odr)

    def test_boot_checks_matching_version_tick_and_actual_registers(self):
        with contextlib.redirect_stdout(io.StringIO()), patch.object(flash.time, "sleep"):
            flash.check_boot(self.make_core(), 0x20000000)

    def test_bad_boot_never_reports_success(self):
        for core in (self.make_core(version=0), self.make_core(tick_delta=0),
                     self.make_core(mode=0), self.make_core(odr=0x100)):
            with self.subTest(core=core), patch.object(flash.time, "sleep"):
                with self.assertRaises(RuntimeError):
                    flash.check_boot(core, 0x20000000)


if __name__ == "__main__":
    unittest.main(verbosity=2)
