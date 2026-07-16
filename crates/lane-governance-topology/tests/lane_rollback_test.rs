use lane_governance_topology::{process_r_topology_window, LaneEvent, LaneEventKind};

#[test]
fn lane_rollback_fires_within_two_windows_for_hard_band_breaches() {
    // Scenario S1 from LaneRollbackTest2026v1.aln.
    let samples = vec![
        sample("S1", 0, 0,         "PROD", 0.20, 0.10, 0.80),
        sample("S1", 1, 300_000,   "PROD", 0.25, 0.12, 0.80),
        sample("S1", 2, 600_000,   "PROD", 0.85, 0.90, 0.80),
        sample("S1", 3, 900_000,   "PROD", 0.90, 0.95, 0.80),
        sample("S1", 4, 1_200_000, "PILOT",0.60, 0.50, 0.80),
    ];

    let mut events = Vec::new();
    process_r_topology_window(&samples, 600_000, |ev: LaneEvent| {
        events.push(ev);
    });

    // Find first HARD-band breach.
    let first_hard = samples.iter().position(|s| s.r_metric >= s.hard_band)
        .expect("no HARD-band breach in scenario");

    // Find first downgrade event.
    let downgrade_index = events.iter().find_map(|ev| {
        if matches!(ev.kind, LaneEventKind::Downgrade) {
            Some(ev.sample_index)
        } else {
            None
        }
    }).expect("no downgrade event fired");

    assert!(
        downgrade_index as i32 - first_hard as i32 <= 2,
        "Downgrade did not fire within two windows: first_hard={}, downgrade_index={}",
        first_hard,
        downgrade_index
    );
}

fn sample(
    scenario_id: &str,
    t_index: i32,
    timestamp_ms: i64,
    lane: &str,
    r_metric: f32,
    vt: f32,
    hard_band: f32,
) -> lane_governance_topology::RTopologySample {
    lane_governance_topology::RTopologySample {
        scenario_id: scenario_id.to_string(),
        t_index,
        timestamp_ms,
        lane: lane.to_string(),
        r_metric,
        vt,
        hard_band,
    }
}
