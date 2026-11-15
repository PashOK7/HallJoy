"""Compiler discovery without relying on the developer's PATH or VS edition."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import run_native_backend_checks as subject


@unittest.skipUnless(os.name == "nt", "Visual Studio discovery is Windows-only")
class CompilerDiscoveryTests(unittest.TestCase):
    def start_patch(self, patcher):
        value = patcher.start()
        self.addCleanup(patcher.stop)
        return value

    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        self.vswhere = root / "Microsoft Visual Studio/Installer/vswhere.exe"
        self.compiler = root / "Custom VS Community Сборка/VC/Tools/Llvm/x64/bin/clang++.exe"
        self.existing = {self.vswhere, self.compiler}
        self.start_patch(patch.dict(os.environ, {"CXX": "", "ProgramFiles(x86)": str(root)}))
        self.which = self.start_patch(patch.object(subject.shutil, "which", return_value=None))
        self.start_patch(patch.object(Path, "is_file", autospec=True,
                                      side_effect=lambda path: path in self.existing))
        self.discovery = self.start_patch(patch.object(subject.subprocess, "run"))
        self.discovery.return_value = subprocess.CompletedProcess(
            [], 0, stdout=str(self.compiler) + "\n", stderr="")

    def test_visual_studio_compiler_is_found_outside_path(self):
        self.assertEqual(subject.find_cxx(), str(self.compiler))
        command = self.discovery.call_args.args[0]
        self.assertEqual(command[0], str(self.vswhere))
        self.assertIn("-utf8", command)
        self.assertIn("VC/Tools/Llvm/x64/bin/clang++.exe", command)

    def test_explicit_compiler_takes_precedence(self):
        with patch.dict(os.environ, {"CXX": "chosen-compiler"}):
            self.assertEqual(subject.find_cxx(), "chosen-compiler")
        self.discovery.assert_not_called()

    def test_path_compiler_takes_precedence(self):
        self.which.side_effect = [None, "path-clang++"]
        self.assertEqual(subject.find_cxx(), "path-clang++")
        self.discovery.assert_not_called()

    def test_missing_discovery_tool_returns_none(self):
        self.existing.remove(self.vswhere)
        self.assertIsNone(subject.find_cxx())
        self.discovery.assert_not_called()

    def test_stale_candidate_is_skipped(self):
        self.discovery.return_value.stdout = "missing-clang++.exe\n" + str(self.compiler) + "\n"
        self.assertEqual(subject.find_cxx(), str(self.compiler))

    def test_no_installed_compiler_returns_none(self):
        self.discovery.return_value.stdout = "\n"
        self.assertIsNone(subject.find_cxx())

    def test_discovery_errors_return_none(self):
        for error in (OSError("cannot start"), subprocess.CalledProcessError(1, "vswhere"),
                      subprocess.TimeoutExpired("vswhere", 10)):
            with self.subTest(error=type(error).__name__):
                self.discovery.side_effect = error
                self.assertIsNone(subject.find_cxx())


if __name__ == "__main__":
    unittest.main()
