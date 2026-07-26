# eco-ledger-particles

This crate defines the ledger particle types for recording eco-restoration actions in the Prometheus-Praxis ecosystem.

## EcoCredit integration

This crate uses `prometheus-praxis_spine::EcoCredit` to compute eco-restoration credits from residuals.

Credits are recorded for reporting and policy metrics only and are never fed back into control logic or plane weights.

### Usage

```rust
use eco_ledger_particles::{RegionalEcoLedgerParticle, compute_eco_credit_for_ledger};

// Compute eco-credit when minting a ledger particle
let delta_v = -0.15;  // Lyapunov improvement (negative delta)
let r_carbon = 0.05;  // carbon risk factor
let jw = 10.0;        // workload energy
let compliant = true; // non-offsettable planes in compliance

if let Some(credit) = compute_eco_credit_for_ledger(delta_v, r_carbon, jw, compliant) {
    println!("Minted eco-credit: {}", credit.value);
}
```

## Output-only constraint

**Important:** The `EcoCredit` type is output-only. It MUST NOT be used to:
- Alter `LyapunovWeights` or `RiskCoord` values
- Influence controller decisions
- Change plane weight configurations

This preserves the "credits cannot offset physical risk" constraint central to the Prometheus-Praxis design.
