#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResearchObjectId {
    pub category: String; // "advection", "marl", "streaming", "bugs_life"
    pub name: String;
    pub version: String;
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ComponentResearchObject {
    pub id: ResearchObjectId;
    pub profile: SevenDimProfile;
    pub risk_residual: f32;
    pub notes: String;
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PilotResearchObject {
    pub id: ResearchObjectId;
    pub corridor_id: String;
    pub metrics: PilotMetrics;
    pub component_results: Vec<ComponentEvalResult>;
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PilotMetrics {
    pub mean_mrt_reduction: f32;
    pub p_value_heat: f32;
    pub kg_poison_avoided_per_day: f32;
    pub energy_use_per_day: f32;
}

pub struct PhoenixResearchSnapshot {
    pub system_profile: SevenDimProfile;
    pub evidence_flags: SystemEvidenceFlags;
    pub components: Vec<ComponentResearchObject>;
    pub pilots: Vec<PilotResearchObject>;
}

pub fn summarize_phase_a_state(
    components: &[ComponentEvalResult],
    pilots: &[PilotResearchObject],
    thresholds: PhoenixThresholds,
) -> PhoenixResearchSnapshot {
    // aggregate profiles & residuals; derive evidence flags
}
