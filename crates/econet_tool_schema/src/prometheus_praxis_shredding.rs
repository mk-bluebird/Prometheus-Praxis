// filename: crates/econet_tool_schema/src/prometheus_praxis_shredding.rs

use serde::Serialize;
use schemars::JsonSchema;

use crate::SchemaProvider;

/// Input type for prometheus_praxis_get_shredding_snapshot_json.
///
/// This describes a single non-actuating request for a KER-weighted snapshot
/// of a shredding machine's blast radius and lane verdict in the Prometheus-Praxis
/// EcoNet constellation.
#[derive(Debug, Clone, Serialize, JsonSchema)]
pub struct PrometheusPraxisShreddingSnapshotInput {
    /// Unique node identifier for the shredding machine inside the EcoNet spine.
    #[schemars(description = "EcoNet node identifier for the shredding machine.")]
    pub node_id: String,

    /// Optional region hint (e.g., Phoenix-AZ) for topology-aware diagnostics.
    #[schemars(description = "Region label used by EcoNet for topology (e.g., Phoenix-AZ).")]
    pub region: Option<String>,

    /// Optional time window start (ISO-8601 UTC) for workload trend aggregation.
    #[schemars(description = "Optional window start timestamp in ISO-8601 UTC.")]
    pub window_start_utc: Option<String>,

    /// Optional time window end (ISO-8601 UTC) for workload trend aggregation.
    #[schemars(description = "Optional window end timestamp in ISO-8601 UTC.")]
    pub window_end_utc: Option<String>,
}

/// Output type for prometheus_praxis_get_shredding_snapshot_json.
///
/// The cdylib function returns a non-actuating governance/diagnostic snapshot
/// combining blast-radius, workload trends, and lane verdicts.
#[derive(Debug, Clone, Serialize, JsonSchema)]
pub struct PrometheusPraxisShreddingSnapshotOutput {
    /// Node identifier for the shredding machine.
    #[schemars(description = "Node identifier for the shredding machine.")]
    pub node_id: String,

    /// Role band of the repo (e.g., DIAGNOSTIC).
    #[schemars(description = "Governance role band of the underlying repo.")]
    pub roleband: String,

    /// Lane scope for this diagnostic snapshot (e.g., RESEARCH).
    #[schemars(description = "Lane scope for the snapshot (RESEARCH, EXPPROD, PROD).")]
    pub lanescope: String,

    /// Blast-radius entries summarizing impact on nodes, regions, and materials.
    #[schemars(description = "List of blast-radius diagnostics for this node.")]
    pub blastradius_entries: Vec<BlastRadiusEntryLite>,

    /// Aggregated workload trends for the shredding machine.
    #[schemars(description = "Aggregated workload trends for this node and time window.")]
    pub workload_trends: Vec<WorkloadTrendEntryLite>,

    /// KER scores (knowledge, eco-impact, risk of harm) for the node or repo.
    #[schemars(description = "KER scores associated with this shredding machine snapshot.")]
    pub ker_scores: KerScoresLite,

    /// Lane verdict expressing whether the current configuration is monotone-safe.
    #[schemars(description = "Lane admissibility verdict for the shredding machine.")]
    pub lane_verdict: LaneVerdictLite,
}

#[derive(Debug, Clone, Serialize, JsonSchema)]
pub struct BlastRadiusEntryLite {
    pub sourcetype: String,
    pub sourceid: String,
    pub targettype: String,
    pub targetid: String,
    pub impacttype: String,
    pub impactscore: f64,
    pub vtsensitivity: f64,
}

#[derive(Debug, Clone, Serialize, JsonSchema)]
pub struct WorkloadTrendEntryLite {
    pub channel: String,
    pub totalrequestsj: f64,
    pub totalsurplusj: f64,
    pub meanvtbefore: f64,
    pub meanvtafter: f64,
    pub meanrcarbon: f64,
    pub meanrbiodiv: f64,
}

#[derive(Debug, Clone, Serialize, JsonSchema)]
pub struct KerScoresLite {
    pub k: f64,
    pub e: f64,
    pub r: f64,
}

#[derive(Debug, Clone, Serialize, JsonSchema)]
pub struct LaneVerdictLite {
    pub admissible: bool,
    pub reason: String,
}

/// SchemaProvider implementation for prometheus_praxis_get_shredding_snapshot_json.
///
/// This binds the governance metadata to the typed input/output schemas so that
/// econet_tool_schema_gen can emit a single tool schema JSON object.
pub struct PrometheusPraxisShreddingSnapshotProvider;

impl SchemaProvider for PrometheusPraxisShreddingSnapshotProvider {
    type Input = PrometheusPraxisShreddingSnapshotInput;
    type Output = PrometheusPraxisShreddingSnapshotOutput;

    fn function_id(&self) -> &str {
        "prometheus_praxis_get_shredding_snapshot_json.v1"
    }

    fn backing_symbol(&self) -> &str {
        "prometheus_praxis_get_shredding_snapshot_json"
    }

    fn summary(&self) -> &str {
        "KER-weighted blast-radius and lane verdict snapshot for a Prometheus-Praxis shredding machine, strictly non-actuating."
    }

    fn lanescope(&self) -> &str {
        "RESEARCH"
    }

    fn roleband(&self) -> &str {
        "DIAGNOSTIC"
    }

    fn blastradius_class(&self) -> &str {
        "NONACTUATING_DIAGNOSTIC"
    }

    fn ai_capability_level(&self) -> &str {
        "DIAGNOSTICONLY"
    }

    fn versiontag(&self) -> &str {
        "2026v1"
    }

    fn status(&self) -> &str {
        "ACTIVE"
    }
}
