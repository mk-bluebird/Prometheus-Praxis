#!/usr/bin/env python3
"""
Example: Load and compute KER scores from ALN scenario files.

This demonstrates how KER (Knowledge/Eco-impact/Risk) scores are computed
from ALN spec definitions using only native Python (no external packages).

Usage:
    python examples/ker/compute_ker_scores.py
"""

import re
import sys
from pathlib import Path
from typing import Dict, List, Optional


def parse_aln_file(filepath: str) -> Dict:
    """
    Parse a simple ALN file and extract records, rows, and corridors.
    
    This is a minimal parser for demonstration purposes.
    """
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    result = {
        'records': [],
        'rows': [],
        'corridors': []
    }
    
    # Parse record definitions
    for match in re.finditer(r'record\s+(\w+)\s*\{([^}]+)\}', content, re.DOTALL):
        name = match.group(1)
        fields_str = match.group(2)
        fields = {}
        for field_match in re.finditer(r'(\w+)\s*=\s*(\w+)', fields_str):
            fields[field_match.group(1)] = field_match.group(2)
        result['records'].append({'name': name, 'fields': fields})
    
    # Parse row instances
    for match in re.finditer(r'row\s+(\w+)\s*\{([^}]+)\}', content, re.DOTALL):
        record_type = match.group(1)
        fields_str = match.group(2)
        instance = {'type': record_type}
        
        # Extract name if present
        name_match = re.search(r'name\s*=\s*"([^"]+)"', fields_str)
        if name_match:
            instance['name'] = name_match.group(1)
        
        # Extract numeric fields
        for field_match in re.finditer(r'(\w+)\s*=\s*([\d.]+)', fields_str):
            instance[field_match.group(1)] = float(field_match.group(2))
        
        result['rows'].append(instance)
    
    # Parse corridor definitions
    for match in re.finditer(r'corridor\s+(\w+)\s*\{([^}]+)\}', content, re.DOTALL):
        name = match.group(1)
        bounds_str = match.group(2)
        bounds = {'name': name}
        for bound_match in re.finditer(r'(\w+)\s*=\s*([\d.]+)', bounds_str):
            field_name = bound_match.group(1)
            field_value = float(bound_match.group(2))
            bounds[field_name] = field_value
        result['corridors'].append(bounds)
    
    return result


def compute_ker_composite(knowledge: float, eco_impact: float, risk: float) -> float:
    """
    Compute composite KER score.
    
    Formula: (knowledge * 0.4 + eco_impact * 0.4) - (risk * 0.2)
    
    Higher knowledge and eco-impact increase the score.
    Higher risk decreases the score.
    """
    return (knowledge * 0.4 + eco_impact * 0.4) - (risk * 0.2)


def check_corridor_bounds(composite: float, risk: float, corridor: Dict) -> bool:
    """Check if KER scores fall within acceptable corridor bounds."""
    min_score = corridor.get('min_composite_score', 0.0)
    max_risk = corridor.get('max_risk_threshold', 1.0)
    
    return composite >= min_score and risk <= max_risk


def main():
    """Load example ALN and compute derived KER metrics."""
    example_file = Path(__file__).parent / 'ker_composition_example.aln'
    
    if not example_file.exists():
        print(f"ERROR: Example file not found: {example_file}")
        return 1
    
    print("=" * 60)
    print("KER Score Computation Example")
    print("=" * 60)
    print()
    
    # Parse the ALN file
    data = parse_aln_file(str(example_file))
    
    print(f"Parsed {len(data['records'])} record definition(s)")
    print(f"Parsed {len(data['rows'])} row instance(s)")
    print(f"Parsed {len(data['corridors'])} corridor definition(s)")
    print()
    
    # Find corridor bounds
    corridor = None
    for c in data['corridors']:
        if c.get('name') == 'KerDeploymentCorridor':
            corridor = c
            break
    
    # Compute scores for each row instance
    print("-" * 60)
    print("Computed KER Scores:")
    print("-" * 60)
    
    passed_count = 0
    failed_count = 0
    
    for row in data['rows']:
        if 'knowledge_factor' not in row:
            continue
        
        name = row.get('name', 'unnamed')
        knowledge = row.get('knowledge_factor', 0.0)
        eco_impact = row.get('eco_impact_value', 0.0)
        risk = row.get('risk_of_harm_score', 0.0)
        
        composite = compute_ker_composite(knowledge, eco_impact, risk)
        
        # Check against corridor
        passes = False
        if corridor:
            passes = check_corridor_bounds(composite, risk, corridor)
        
        status = "✓ PASS" if passes else "✗ FAIL"
        
        print(f"\n{name}:")
        print(f"  Knowledge:     {knowledge:.2f}")
        print(f"  Eco-Impact:    {eco_impact:.2f}")
        print(f"  Risk-of-Harm:  {risk:.2f}")
        print(f"  Composite KER: {composite:.2f}")
        print(f"  Status:        {status}")
        
        if passes:
            passed_count += 1
        else:
            failed_count += 1
    
    print()
    print("-" * 60)
    print(f"Summary: {passed_count} passed, {failed_count} failed")
    print("=" * 60)
    
    return 0 if failed_count == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
