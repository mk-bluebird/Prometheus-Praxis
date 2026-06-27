// filepath: eco_restoration_shard/cybercore/prometheus_praxis/src/lyapunov/block_adapter.rs

#![forbid(unsafe_code)]

use crate::lyapunov::block_lyapunov_guard::{
    BlockLyapunovCoefficients, BlockSnapshot, CellId, CellState, Scalar,
};

/// Minimal projection of a neighborhood or canal segment cell into the
/// Lyapunov guard view. Concrete systems implement this.
pub trait LyapunovCellProjection {
    fn cell_id(&self) -> String;
    fn pfbs_concentration_ug_per_l(&self) -> f64;
    fn ecoli_cfu_per_100ml(&self) -> f64;
    fn swarm_coverage_fraction(&self) -> f64;
    fn weight(&self) -> f64;
}

/// Minimal projection of a block (neighborhood, canal reach cluster, etc.).
pub trait LyapunovBlockProjection {
    type Cell: LyapunovCellProjection;

    fn block_id(&self) -> String;
    fn timestamp_utc_ms(&self) -> i64;
    fn cells(&self) -> &[Self::Cell];
}

/// Helper to build `BlockSnapshot` from any projection.
pub fn make_block_snapshot<B: LyapunovBlockProjection>(
    block: &B,
    coeffs: BlockLyapunovCoefficients,
) -> BlockSnapshot {
    let cells: Vec<CellState> = block
        .cells()
        .iter()
        .map(|c| CellState {
            cell_id: CellId(c.cell_id()),
            pfbs_concentration: Scalar(c.pfbs_concentration_ug_per_l()),
            ecoli_count: Scalar(c.ecoli_cfu_per_100ml()),
            swarm_coverage: Scalar(c.swarm_coverage_fraction()),
            weight: Scalar(c.weight()),
        })
        .collect();

    BlockSnapshot {
        block_id: block.block_id(),
        timestamp_utc_ms: block.timestamp_utc_ms(),
        cells,
        coeffs,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lyapunov::block_lyapunov_guard::BlockLyapunovGuard;

    #[derive(Clone)]
    struct DummyCell {
        id: String,
        c: f64,
        e: f64,
        s: f64,
        w: f64,
    }

    impl LyapunovCellProjection for DummyCell {
        fn cell_id(&self) -> String {
            self.id.clone()
        }
        fn pfbs_concentration_ug_per_l(&self) -> f64 {
            self.c
        }
        fn ecoli_cfu_per_100ml(&self) -> f64 {
            self.e
        }
        fn swarm_coverage_fraction(&self) -> f64 {
            self.s
        }
        fn weight(&self) -> f64 {
            self.w
        }
    }

    struct DummyBlock {
        id: String,
        t: i64,
        cells: Vec<DummyCell>,
    }

    impl LyapunovBlockProjection for DummyBlock {
        type Cell = DummyCell;

        fn block_id(&self) -> String {
            self.id.clone()
        }

        fn timestamp_utc_ms(&self) -> i64 {
            self.t
        }

        fn cells(&self) -> &[Self::Cell] {
            &self.cells
        }
    }

    #[test]
    fn adapter_produces_consistent_snapshots() {
        let block = DummyBlock {
            id: "neigh-01".to_string(),
            t: 1_726_000_000_000,
            cells: vec![
                DummyCell {
                    id: "c0".into(),
                    c: 1.0,
                    e: 10.0,
                    s: 0.2,
                    w: 1.0,
                },
                DummyCell {
                    id: "c1".into(),
                    c: 0.5,
                    e: 5.0,
                    s: 0.4,
                    w: 0.5,
                },
            ],
        };

        let coeffs = BlockLyapunovCoefficients {
            alpha_pfbs: Scalar(1.0),
            beta_ecoli: Scalar(0.1),
            gamma_swarm: Scalar(0.5),
        };

        let snapshot_before = make_block_snapshot(&block, coeffs.clone());
        let snapshot_after = make_block_snapshot(&block, coeffs.clone());

        let guard =
            BlockLyapunovGuard::new(crate::lyapunov::block_lyapunov_guard::BlockLyapunovPolicy::default_derate_band())
                .unwrap();

        let res = guard.evaluate_step(&snapshot_before, &snapshot_after).unwrap();
        assert!(matches!(res.decision, crate::lyapunov::block_lyapunov_guard::BlockLyapunovDecision::Allow));
    }
}
