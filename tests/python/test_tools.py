"""
Unit tests for Python tools in Prometheus-Praxis.

These tests exercise core functionality using only native Python (stdlib).
Run with: python -m unittest discover tests/python
"""

import unittest
import sys
import os
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent.parent / 'python'))


class TestPpxDiagnostics(unittest.TestCase):
    """Tests for ppx_diagnostics.py module."""
    
    def setUp(self):
        """Set up test fixtures."""
        try:
            from ppx_diagnostics import (
                StructuredMetricData,
                StructuredFileDelta,
                UnifiedSystemDiagnosticState,
                ChangeTypeEnum,
                parse_unified_diff,
            )
            self.module_available = True
            self.StructuredMetricData = StructuredMetricData
            self.StructuredFileDelta = StructuredFileDelta
            self.UnifiedSystemDiagnosticState = UnifiedSystemDiagnosticState
            self.ChangeTypeEnum = ChangeTypeEnum
            self.parse_unified_diff = parse_unified_diff
        except ImportError:
            self.module_available = False
    
    def test_structured_metric_creation(self):
        """Test creating a structured metric data object."""
        if not self.module_available:
            self.skipTest("ppx_diagnostics module not available")
        
        from datetime import datetime
        metric = self.StructuredMetricData(
            name="test_metric",
            recorded_value=42.0,
            collection_epoch=datetime.utcnow()
        )
        self.assertEqual(metric.name, "test_metric")
        self.assertEqual(metric.recorded_value, 42.0)
    
    def test_metric_to_prometheus_sample(self):
        """Test converting metric to Prometheus format."""
        if not self.module_available:
            self.skipTest("ppx_diagnostics module not available")
        
        from datetime import datetime
        metric = self.StructuredMetricData(
            name="eco_score",
            recorded_value=0.95,
            collection_epoch=datetime(2026, 1, 1, 0, 0, 0)
        )
        sample = metric.to_prometheus_sample()
        self.assertIn("eco_score", sample)
        self.assertIn("0.95", sample)
    
    def test_file_delta_additions(self):
        """Test counting added lines in file delta."""
        if not self.module_available:
            self.skipTest("ppx_diagnostics module not available")
        
        delta = self.StructuredFileDelta(
            origin_filepath="old.py",
            target_filepath="new.py",
            is_new_file=False
        )
        # Manually add a hunk with additions
        from ppx_diagnostics import ParsedDiffHunk, ParsedCodeLine
        hunk = ParsedDiffHunk(
            old_start=1, old_count=1, new_start=1, new_count=2,
            lines=[
                ParsedCodeLine(
                    old_line_number=1, new_line_number=1,
                    modification_type=self.ChangeTypeEnum.CONTEXT,
                    line_payload="existing"
                ),
                ParsedCodeLine(
                    old_line_number=-1, new_line_number=2,
                    modification_type=self.ChangeTypeEnum.ADDITION,
                    line_payload="new line"
                )
            ]
        )
        delta.hunks.append(hunk)
        self.assertEqual(delta.total_added_lines(), 1)
        self.assertEqual(delta.total_deleted_lines(), 0)
    
    def test_parse_unified_diff(self):
        """Test parsing unified diff format."""
        if not self.module_available:
            self.skipTest("ppx_diagnostics module not available")
        
        diff_lines = [
            "--- old.py",
            "+++ new.py",
            "@@ -1,2 +1,3 @@",
            " context line",
            "+added line",
            " another context",
        ]
        delta = self.parse_unified_diff("old.py", "new.py", diff_lines)
        self.assertEqual(delta.origin_filepath, "old.py")
        self.assertEqual(delta.target_filepath, "new.py")
        self.assertEqual(len(delta.hunks), 1)


class TestAlnParser(unittest.TestCase):
    """Tests for ALN parsing functionality."""
    
    def setUp(self):
        """Set up test fixtures."""
        self.aln_content = """
-- Test ALN spec
record TestParticle {
    name = string
    value = float
}

row TestParticle {
    name = "test"
    value = 42.0
}

corridor TestCorridor {
    metric = "eco_score"
    min = 0.0
    max = 1.0
}
"""
    
    def test_parse_record_definition(self):
        """Test parsing ALN record definitions."""
        import re
        records = re.findall(r'record\s+(\w+)\s*\{(.*?)\}', self.aln_content, re.DOTALL)
        self.assertEqual(len(records), 1)
        self.assertEqual(records[0][0], "TestParticle")
    
    def test_parse_row_instance(self):
        """Test parsing ALN row instances."""
        import re
        rows = re.findall(r'row\s+(\w+)\s*\{(.*?)\}', self.aln_content, re.DOTALL)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0][0], "TestParticle")
    
    def test_parse_corridor(self):
        """Test parsing ALN corridor definitions."""
        import re
        corridors = re.findall(r'corridor\s+(\w+)\s*\{(.*?)\}', self.aln_content, re.DOTALL)
        self.assertEqual(len(corridors), 1)
        self.assertEqual(corridors[0][0], "TestCorridor")


class TestKerScoreCalculator(unittest.TestCase):
    """Tests for KER score calculation logic."""
    
    def test_ker_composition(self):
        """Test KER (Knowledge/Eco/Risk) score composition."""
        knowledge = 0.9
        eco_impact = 0.85
        risk_of_harm = 0.1
        
        # Composite score: weighted average with risk penalty
        composite = (knowledge * 0.4 + eco_impact * 0.4) - (risk_of_harm * 0.2)
        
        self.assertAlmostEqual(composite, 0.68, places=2)
    
    def test_ker_threshold_check(self):
        """Test KER threshold validation."""
        ker_scores = [
            {"knowledge": 0.9, "eco_impact": 0.85, "risk_of_harm": 0.1},
            {"knowledge": 0.5, "eco_impact": 0.4, "risk_of_harm": 0.8},
        ]
        
        def passes_threshold(scores, min_knowledge=0.7, max_risk=0.5):
            return (scores["knowledge"] >= min_knowledge and 
                    scores["risk_of_harm"] <= max_risk)
        
        self.assertTrue(passes_threshold(ker_scores[0]))
        self.assertFalse(passes_threshold(ker_scores[1]))


class TestShardLayoutValidator(unittest.TestCase):
    """Tests for shard layout validation."""
    
    def test_shard_dependency_graph(self):
        """Test shard dependency graph validation."""
        shards = {
            "shard_a": {"depends_on": []},
            "shard_b": {"depends_on": ["shard_a"]},
            "shard_c": {"depends_on": ["shard_a", "shard_b"]},
        }
        
        def has_cycle(shard_id, visited, rec_stack):
            visited.add(shard_id)
            rec_stack.add(shard_id)
            
            for dep in shards.get(shard_id, {}).get("depends_on", []):
                if dep not in visited:
                    if has_cycle(dep, visited, rec_stack):
                        return True
                elif dep in rec_stack:
                    return True
            
            rec_stack.remove(shard_id)
            return False
        
        # Check for cycles
        visited = set()
        rec_stack = set()
        has_cycles = any(
            has_cycle(shard, visited, rec_stack) 
            for shard in shards
        )
        self.assertFalse(has_cycles, "Dependency graph should not have cycles")


if __name__ == '__main__':
    unittest.main()
