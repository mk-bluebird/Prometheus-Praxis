// crates/lane-governance-topology/src/bin/cli.rs

use lane_governance_topology::{process_r_topology_window, LaneEventKind, RTopologySample};

fn main() {
    let samples = vec![
        RTopologySample {
            scenario_id: "SAMPLE_SCENARIO".to_string(),
            t_index: 0,
            timestamp_ms: 0,
            lane: "RESEARCH".to_string(),
            r_metric: 0.2,
            vt: 0.1,
            hard_band: 0.8,
        },
    ];

    process_r_topology_window(&samples, 600_000, |event| {
        println!(
            "Lane event: scenario_id={}, sample_index={}, from_lane={}, to_lane={}, kind={:?}, reason={}",
            event.scenario_id,
            event.sample_index,
            event.from_lane,
            event.to_lane,
            event.kind,
            event.reason
        );
    });
}
