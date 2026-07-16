// crates/lane-governance-topology/src/types.rs

/// A single r_topology sample.
#[derive(Debug, Clone)]
pub struct RTopologySample {
    pub scenario_id: String,
    pub t_index: i32,
    pub timestamp_ms: i64,
    pub lane: String,
    pub r_metric: f32,
    pub vt: f32,
    pub hard_band: f32,
}

/// Kind of lane event.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LaneEventKind {
    Promote,
    Downgrade,
    Stay,
}

/// Lane event emitted by the engine.
#[derive(Debug, Clone)]
pub struct LaneEvent {
    pub scenario_id: String,
    pub sample_index: i32,
    pub from_lane: String,
    pub to_lane: String,
    pub kind: LaneEventKind,
    pub reason: String,
}
