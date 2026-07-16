// crates/lane-governance-topology/src/lib.rs

pub mod types;
pub mod engine;

pub use types::{LaneEvent, LaneEventKind, RTopologySample};

/// Process a sliding window of r_topology samples and emit lane events.
///
/// This is a minimal stub that delegates to a non-SIMD Rust implementation
/// until the C++ engine and FFI bindings are fully wired.
pub fn process_r_topology_window<F>(
    samples: &[RTopologySample],
    window_ms: i64,
    mut emit: F,
) where
    F: FnMut(LaneEvent),
{
    let _ = window_ms;
    for (idx, sample) in samples.iter().enumerate() {
        let event = LaneEvent {
            scenario_id: sample.scenario_id.clone(),
            sample_index: sample.t_index,
            from_lane: sample.lane.clone(),
            to_lane: sample.lane.clone(),
            kind: LaneEventKind::Stay,
            reason: format!("stub_event_index_{}", idx),
        };
        emit(event);
    }
}
