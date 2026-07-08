# ALN Parser Guide

This guide documents the ALN (Artifact Linkage Notation) parser API and usage patterns for developers and AI-chat integration.

## Overview

ALN is a line-based notation for expressing governance contracts, safety rules, and deployment kernels in the EcoNet ecosystem. The `alncore` crate provides a Rust implementation of the ALN parser with validation and evaluation capabilities.

## Core API

### Parsing

```rust
use alncore::{parse_aln_str, AlnDocument, AlnParseError};

// Parse an ALN string into a document
let content = std::fs::read_to_string("contract.aln")?;
let doc: Result<AlnDocument, AlnParseError> = parse_aln_str(&content);

match doc {
    Ok(document) => {
        println!("Parsed document: {}", document.doc_id);
    }
    Err(e) => {
        eprintln!("Parse error at line {}: {}", e.line, e.message);
    }
}
```

### Evaluation Functions

#### eval_safestep

Checks if a Lyapunov step satisfies the SafeStepRule constraint:

```rust
use alncore::{eval_safestep, SafeStepRule};

let rule = SafeStepRule::new(
    "rule-001",
    "Standard safety step",
    Some(1.0),  // vt_ceiling
    0.1,        // epsilon
    "governance" // lyap_channel
);

let v_t = 0.5;   // Previous value
let v_t1 = 0.55; // New value

if eval_safestep(v_t, v_t1, &rule) {
    println!("Step is admissible");
} else {
    println!("Step violates safety constraint");
}
```

**Invariant INV-SAFESTEP-2:** `V_{t+1} <= V_t + epsilon`

#### eval_deploy

Checks if KER values satisfy a DeployDecisionKernel:

```rust
use alncore::{eval_deploy, DeployDecisionKernel};

let kernel = DeployDecisionKernel::new(
    "kernel-prod-001",
    "Production deployment kernel",
    0.7,  // k_min
    0.6,  // e_min
    0.3,  // r_max
    "PROD" // lane_scope
);

let k = 0.75; // Knowledge
let e = 0.65; // Eco-impact
let r = 0.25; // Risk

if eval_deploy(k, e, r, &kernel) {
    println!("Deployment is admissible");
} else {
    println!("Deployment rejected - thresholds not met");
}
```

#### check_move

Composite gate that enforces both Lyapunov non-expansion and KER thresholds with lane-scope awareness:

```rust
use alncore::{check_move, KerSnapshot, KerCompleteness, Lane, DeployDecision};

let snapshot = KerSnapshot::new(
    0.75,              // k
    0.60,              // e
    0.30,              // r
    0.05,              // vt
    Lane::Research,    // lane
    KerCompleteness::Measured,
    false              // is_speculative
);

let previous_vt = 0.04;
let decision = check_move(&snapshot, previous_vt, &rule, &kernel);

match decision {
    DeployDecision::Admissible => {
        println!("Move is admissible");
    }
    DeployDecision::Rejected { reason } => {
        println!("Move rejected: {}", reason);
    }
}
```

### explain_deploy

Returns human-readable explanation of which thresholds were violated or satisfied:

```rust
use alncore::explain_deploy;

let explanation = explain_deploy(&snapshot, &kernel);
println!("{}", explanation);
```

Example output:
```
DeployDecisionKernel 'kernel-prod-001': ADMISSIBLE
  ✓ K=0.750 meets minimum 0.700
  ✓ E=0.600 meets minimum 0.600
  ✓ R=0.300 within maximum 0.300
  ✓ Lane Research matches kernel scope 'ALL'
```

## Reading .aln Files

### Basic Pattern

```rust
use alncore::{parse_aln_str, to_canonical_json};

fn load_aln_file(path: &str) -> Result<(), Box<dyn std::error::Error>> {
    let content = std::fs::read_to_string(path)?;
    let doc = parse_aln_str(&content)?;
    
    println!("Document loaded successfully:");
    println!("  ID: {}", doc.doc_id);
    println!("  Schema: {}", doc.schema_name);
    println!("  SafeStepRules: {}", doc.safesteprules.len());
    println!("  DeployKernels: {}", doc.deploy_kernels.len());
    
    // Get canonical JSON for cross-language comparison
    let json = to_canonical_json(&doc);
    println!("Canonical JSON: {}", json);
    
    Ok(())
}
```

### Why a Move Was Rejected

To explain why a proposed move was rejected:

```rust
use alncore::{parse_aln_str, check_move, explain_deploy, DeployDecision, KerSnapshot};

fn explain_rejection(aln_path: &str, snapshot: &KerSnapshot, prev_vt: f32) {
    let content = std::fs::read_to_string(aln_path).unwrap();
    let doc = parse_aln_str(&content).unwrap();
    
    for kernel in &doc.deploy_kernels {
        // Check lane scope first
        let explanation = explain_deploy(snapshot, kernel);
        println!("{}", explanation);
        
        // Check full move decision
        if let Some(rule) = doc.safesteprules.first() {
            match check_move(snapshot, prev_vt, rule, kernel) {
                DeployDecision::Admissible => {
                    println!("=> Move ADMISSIBLE under this kernel\n");
                }
                DeployDecision::Rejected { reason } => {
                    println!("=> Move REJECTED: {}\n", reason);
                }
            }
        }
    }
}
```

## CLI Tool: alnctl

The `alnctl` binary provides command-line access to ALN validation and explanation:

```bash
# Validate an ALN file
alnctl validate path/to/contract.aln

# Get human-readable explanation of rules and kernels
alnctl explain path/to/contract.aln

# Output canonical JSON representation
alnctl json path/to/contract.aln
```

## Integration Patterns

### Ecosafety Crate Integration

```rust
// In cyboquatic-ecosafety crate
use alncore::{parse_aln_str, DeployDecisionKernel};

pub fn load_ecosafety_contract(path: &str) -> alncore::AlnDocument {
    let content = std::fs::read_to_string(path)
        .expect("Failed to read ecosafety contract");
    parse_aln_str(&content)
        .expect("Invalid ecosafety contract")
}

pub fn get_deploy_thresholds(contract: &alncore::AlnDocument, lane: &str) -> (f32, f32, f32) {
    contract.deploy_kernels
        .iter()
        .find(|k| k.lane_scope == lane || k.lane_scope == "ALL")
        .map(|k| (k.k_min, k.e_min, k.r_max))
        .unwrap_or((0.5, 0.5, 0.5)) // defaults
}
```

### Lane Promotion Integration

```rust
// In cyboquatic-core lane promotion
use alncore::{KerSnapshot, DeployDecisionKernel};

pub fn should_promote_lane(
    snapshot: &KerSnapshot,
    kernels: &[DeployDecisionKernel],
) -> bool {
    kernels.iter().any(|k| {
        // Check if kernel applies to this lane
        lanes_match(&snapshot.lane, &k.lane_scope) &&
        // Check KER thresholds
        snapshot.k_clamped() >= k.k_min &&
        snapshot.e_clamped() >= k.e_min &&
        snapshot.r_clamped() <= k.r_max
    })
}
```

## Error Handling

The parser returns `AlnParseError` with line number information:

```rust
use alncore::{parse_aln_str, AlnParseError};

fn parse_with_diagnostics(content: &str) {
    match parse_aln_str(content) {
        Ok(doc) => { /* success */ }
        Err(AlnParseError { line, message }) => {
            eprintln!("Error at line {}: {}", line, message);
            
            // Show context
            let lines: Vec<&str> = content.lines().collect();
            if line > 0 && line <= lines.len() {
                eprintln!("Context: {}", lines[line - 1]);
            }
        }
    }
}
```

## Validation Invariants

The parser automatically validates these invariants:

| Invariant | Description |
|-----------|-------------|
| INV-ALNDOC-1 | evidence_hex/signing_hex non-empty for non-RESEARCH roleband |
| INV-REPO-1 | ecorestorationshard must have roleband=RESEARCH and non_actuating_only=true |
| INV-REPO-2 | owner_did must be in allowed Bostrom DID set |
| INV-SAFESTEP-1 | epsilon >= 0 |
| INV-KER-1/2/3 | k_min, e_min, r_max in [0, 1] |
| INV-OVERRIDE-1 | forbid_safety_loosen must be true |

## Cross-Language Conformance

Use `to_canonical_json()` to generate the ground-truth representation for comparing parsers across languages:

```rust
use alncore::{parse_aln_str, to_canonical_json};

let doc = parse_aln_str(&content)?;
let canonical = to_canonical_json(&doc);

// Compare with expected golden file
let expected = std::fs::read_to_string("golden.json")?;
assert_eq!(canonical, expected);
```

## See Also

- `schemas/alncoregrammar.md` - Complete grammar specification
- `crates/alncore/examples/` - Working example programs
- `.github/workflows/ci-alncore.yml` - CI configuration for parser testing
