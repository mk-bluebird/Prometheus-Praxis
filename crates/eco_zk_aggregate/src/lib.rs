// filename: crates/eco_zk_aggregate/src/lib.rs
// destination: ecorestorationshard/crates/eco_zk_aggregate/src/lib.rs

use halo2_proofs::{
    circuit::{Layouter, SimpleFloorPlanner},
    dev::MockProver,
    plonk::{Circuit, ConstraintSystem, Error},
    pasta::Fp,
};

/// Structure of a single oracle report used in the zk circuit.
#[derive(Clone)]
pub struct OracleReport {
    pub value: Fp,
    pub signed: bool,
}

/// Public inputs:
/// - median_delta_r: claimed median Δr.
/// - max_deviation: maximum allowed deviation.
/// - num_reports: number of reports.
#[derive(Clone)]
pub struct EcoAggregateConfig {
    pub max_reports: usize,
}

#[derive(Clone)]
pub struct EcoAggregateCircuit {
    pub reports: Vec<OracleReport>,
    pub claimed_median_delta_r: Fp,
    pub max_deviation: Fp,
}

impl Circuit<Fp> for EcoAggregateCircuit {
    type Config = EcoAggregateConfig;
    type FloorPlanner = SimpleFloorPlanner;

    fn without_witnesses(&self) -> Self {
        Self {
            reports: vec![],
            claimed_median_delta_r: Fp::zero(),
            max_deviation: Fp::zero(),
        }
    }

    fn configure(_cs: &mut ConstraintSystem<Fp>) -> Self::Config {
        EcoAggregateConfig { max_reports: 16 }
    }

    fn synthesize(
        &self,
        _config: Self::Config,
        _layouter: impl Layouter<Fp>,
    ) -> Result<(), Error> {
        // High-level zk conditions to enforce:
        //
        // 1. All reports are within max_deviation of claimed_median_delta_r.
        // 2. claimed_median_delta_r is equal to the median of the sorted reports.
        //
        // A full implementation would:
        // - Allocate advice columns for report values.
        // - Implement a sorting network inside the circuit.
        // - Constrain the middle element to equal claimed_median_delta_r.
        // - Constrain |report_i - claimed_median_delta_r| <= max_deviation for all i.
        //
        // This placeholder leaves the constraints to be filled according to
        // your chosen Halo2 patterns.

        let _ = self.reports.clone();
        let _ = self.claimed_median_delta_r;
        let _ = self.max_deviation;

        Ok(())
    }
}

/// Example helper to test the circuit off-chain.
pub fn test_eco_aggregate_circuit(
    reports: Vec<u64>,
    claimed_median: u64,
    max_dev: u64,
) -> Result<(), String> {
    let reports_fp = reports
        .into_iter()
        .map(|v| OracleReport {
            value: Fp::from(v),
            signed: true,
        })
        .collect::<Vec<_>>();

    let circuit = EcoAggregateCircuit {
        reports: reports_fp,
        claimed_median_delta_r: Fp::from(claimed_median),
        max_deviation: Fp::from(max_dev),
    };

    let public_inputs: Vec<Fp> = vec![
        circuit.claimed_median_delta_r,
        circuit.max_deviation,
        Fp::from(circuit.reports.len() as u64),
    ];

    let k = 8;
    let prover = MockProver::run(k, &circuit, vec![public_inputs])
        .map_err(|e| e.to_string())?;
    prover.verify().map_err(|e| format!("{:?}", e))
}
