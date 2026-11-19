"""只测电脑端协议边界，不导入pyOCD、不枚举探针、不接板。"""
import importlib.util
from pathlib import Path
import unittest

path = Path(__file__).resolve().parents[1] / "tools/motor_link.py"
spec = importlib.util.spec_from_file_location("motor_link", path)
link = importlib.util.module_from_spec(spec)
spec.loader.exec_module(link)


class FakeCore:
    def __init__(self):
        self.calls = []
        self.words = [link.MAGIC, link.ABI, link.FW_ID, 1001, 1, 0, 0, 0, 0, 0]

    def write_memory_block32(self, address, words):
        self.calls.append(("parameters", address, words))

    def write32(self, address, value):
        self.calls.append(("command", address, value))

    def flush(self):
        self.calls.append(("flush",))

    def read_memory_block32(self, address, count):
        return self.words[:count]


class ProtocolTests(unittest.TestCase):
    def test_parameters_flushed_before_single_start_command(self):
        core = FakeCore()
        link.request_jog(core, 0x20000000, "A", 15, 50)
        self.assertEqual(core.calls, [
            ("parameters", 0x2000001C, [15, 50]), ("flush",),
            ("command", 0x20000018, link.CMD_A), ("flush",)])

    def test_electrical_polarity_b_uses_different_explicit_command(self):
        core = FakeCore()
        link.request_jog(core, 0x20000000, "B", 15, 50)
        self.assertEqual(core.calls[-2], ("command", 0x20000018, link.CMD_B))

    def test_wrong_magic_abi_or_version_refuses_mailbox(self):
        for index in range(3):
            core = FakeCore()
            core.words[index] ^= 1
            with self.assertRaises(RuntimeError):
                link.read_mailbox(core, 0x20000000)
            self.assertEqual(core.calls, [])

    def test_status_format_is_fixed_ten_words(self):
        core = FakeCore()
        data = link.read_mailbox(core, 0x20000000)
        self.assertEqual(len(data), 10)
        self.assertEqual(data["state"], 1)
        self.assertEqual(data["uptime_ms"], 1001)


if __name__ == "__main__":
    unittest.main(verbosity=2)
