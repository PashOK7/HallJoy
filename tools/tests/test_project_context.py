"""Context entry checks use isolated files, never the installed HallJoy."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import project_context as subject


class ContextTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "workspace"
        files = set(subject.CORE + subject.ARCHIVES + (subject.VERSION,))
        files.update(p for group in subject.TOPICS.values() for p in group)
        for relative in files:
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("# Fixture\n", encoding="utf8")
        (self.root / subject.VERSION).write_text(
            '#define HALLJOY_VERSION_STRING_FULL "1.6.13.0"\n', encoding="utf8")

    def snapshot(self):
        return {p.relative_to(self.root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in self.root.rglob("*") if p.is_file()}

    def test_extracted_workspace_needs_neither_git_nor_private_baseline(self):
        before = self.snapshot()
        result = subject.context(self.root, "layouts")
        self.assertEqual(result["errors"], [])
        self.assertFalse(result["workspace_has_git"])
        self.assertIsNone(result["publication_baseline_local_only"])
        self.assertEqual(result["source_version"], "1.6.13.0")
        self.assertEqual(result["remote_state"], "not queried")
        self.assertEqual(before, self.snapshot())

    def test_missing_required_document_is_detected(self):
        (self.root / subject.CORE[0]).unlink()
        self.assertTrue(any("Missing required" in e for e in subject.context(self.root)["errors"]))

    def test_broken_link_fails_but_external_and_anchor_are_allowed(self):
        doc = self.root / "docs/README.md"
        doc.write_text("[ok](current/OWNER_CONTEXT.md#rule) [web](https://example.com) "
                       "[local](#rule) [bad](missing.md)", encoding="utf8")
        errors = subject.context(self.root)["errors"]
        self.assertEqual(len(errors), 1)
        self.assertIn("missing.md", errors[0])

    def test_byte_budget_detects_context_growth(self):
        relative = "docs/current/OWNER_CONTEXT.md"
        (self.root / relative).write_bytes(b"x" * (subject.BUDGETS[relative] + 1))
        self.assertTrue(any("budget" in e for e in subject.context(self.root)["errors"]))

    def test_invalid_private_state_is_reported(self):
        path = self.root / ".local/release-state.json"
        path.parent.mkdir()
        path.write_text("[]", encoding="utf8")
        self.assertTrue(any("baseline" in e for e in subject.context(self.root)["errors"]))

    def test_delivery_is_hashed_only_on_request(self):
        exe = self.root / subject.EXE
        exe.parent.mkdir(parents=True)
        exe.write_bytes(b"fixture-exe")
        self.assertNotIn("sha256", subject.context(self.root)["delivery"])
        self.assertEqual(subject.context(self.root, hash_exe=True)["delivery"]["sha256"],
                         hashlib.sha256(b"fixture-exe").hexdigest())

    def test_search_reaches_owner_history_and_is_bounded(self):
        archive = self.root / subject.ARCHIVES[0]
        archive.write_text("K552 evidence\n" * 30, encoding="utf8")
        hits = subject.search(self.root, "k552", limit=3)
        self.assertEqual(len(hits), 3)
        self.assertEqual(hits[0]["path"], subject.ARCHIVES[0])
        self.assertEqual(subject.search(self.root, " "), [])


if __name__ == "__main__":
    unittest.main()
