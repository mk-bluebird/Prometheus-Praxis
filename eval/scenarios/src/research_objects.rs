
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResearchObjectId {
    pub category: String, // "component", "experiment", "safety_case", "sovereignty", "energy", "explainability"
    pub name: String,
    pub version: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ComponentResearchObject {
    pub id: ResearchObjectId,
    pub profile: SevenDimProfile,
    pub notes: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ExperimentResearchObject {
    pub id: ResearchObjectId,
    pub corridor_id: String,
    pub metrics: ExperimentMetrics,
    pub gate_impact: SystemEvidenceFlags,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ExperimentMetrics {
    pub mean_mrt_reduction: f32,
    pub p_value_heat: f32,
    pub biodiversity_loss_index: f32,
    pub water_use_per_day: f32,
    pub energy_use_per_day: f32,
}
