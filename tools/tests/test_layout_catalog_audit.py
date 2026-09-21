import importlib.util
import tempfile
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location("layout_audit", Path(__file__).parents[1] / "audit_layout_catalog.py")
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)

class CatalogAuditTests(unittest.TestCase):
    def run_audit(self, rows):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "inventory.tsv"
            path.write_text("\n".join("\t".join(map(str, row)) for row in rows), encoding="utf-8")
            return audit.audit(path)

    def row(self, brand, name, display, x=0):
        return [brand, name, display, 41, x, 0, 42, 42, 0, 0, "Esc"]

    def test_equal_cross_brand_layouts_stay_separate(self):
        result = self.run_audit([self.row("A", "A One", "A One"), self.row("B", "B One", "B One")])
        self.assertEqual(result["visibleVariants"], 2)
        self.assertEqual(len(result["crossBrandGeometryKeptSeparate"]), 1)

    def test_cross_brand_merge_is_rejected(self):
        with self.assertRaisesRegex(AssertionError, "cross-brand merge"):
            self.run_audit([self.row("A", "A One", "Shared"), self.row("B", "B One", "Shared")])

    def test_different_geometry_merge_is_rejected(self):
        with self.assertRaisesRegex(AssertionError, "unequal merged geometry"):
            self.run_audit([self.row("A", "A One", "Shared"), self.row("A", "A Two", "Shared", 1)])

    def test_unmerged_exact_same_brand_duplicate_is_rejected(self):
        with self.assertRaisesRegex(AssertionError, "unmerged identical"):
            self.run_audit([self.row("A", "A One", "A One"), self.row("A", "A Two", "A Two")])

if __name__ == "__main__":
    unittest.main()
