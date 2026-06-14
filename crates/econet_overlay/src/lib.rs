// filename: crates/econet_overlay/src/lib.rs
// destination: ecorestoration_shard/crates/econet_overlay/src/lib.rs

#![forbid(unsafe_code)]

use std::fmt;
use std::time::SystemTime;

//
// Core domain types for smart-simplicity overlays
//

#[derive(Clone, Debug)]
pub enum LaneBand {
    Research,
    ExpProd,
    Prod,
}

impl fmt::Display for LaneBand {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = match self {
            LaneBand::Research => "RESEARCH",
            LaneBand::ExpProd => "EXPPROD",
            LaneBand::Prod => "PROD",
        };
        f.write_str(s)
    }
}

#[derive(Clone, Debug)]
pub enum RoleBand {
    Spine,
    Research,
    Engine,
    Material,
    Gov,
    App,
}

impl fmt::Display for RoleBand {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = match self {
            RoleBand::Spine => "SPINE",
            RoleBand::Research => "RESEARCH",
            RoleBand::Engine => "ENGINE",
            RoleBand::Material => "MATERIAL",
            RoleBand::Gov => "GOV",
            RoleBand::App => "APP",
        };
        f.write_str(s)
    }
}

#[derive(Clone, Debug)]
pub struct KerBands {
    pub k: f32,
    pub e: f32,
    pub r: f32,
}

impl KerBands {
    pub fn clamped(self) -> Self {
        fn clamp(v: f32) -> f32 {
            if v < 0.0 {
                0.0
            } else if v > 1.0 {
                1.0
            } else {
                v
            }
        }
        KerBands {
            k: clamp(self.k),
            e: clamp(self.e),
            r: clamp(self.r),
        }
    }
}

#[derive(Clone, Debug)]
pub struct EcoPlaneScores {
    pub plane_energy: f32,
    pub plane_carbon: f32,
    pub plane_responsibility: f32,
    pub plane_dataquality: f32,
    pub has_biodiversity_lane: bool,
    pub materials_flag: Option<String>,
}

#[derive(Clone, Debug)]
pub struct RepoSummary {
    pub filename: String,
    pub destination: String,
    pub repotarget: String,
    pub roleband: RoleBand,
    pub lanedefault: LaneBand,
    pub regionscope: String,
    pub planes: Vec<String>,
    pub logicalname: String,
    pub artifactkind: String,
    pub econscope: String,
    pub contractid: Option<String>,
    pub nonactuating: bool,
    pub kerbands: KerBands,
    pub authorbostrom: String,
    pub ownerlabel: Option<String>,
    pub evidencehex: Option<String>,
    pub dbshardlogical: Option<String>,
    pub dbrole: Option<String>,
    pub tasklogical: Option<String>,
    pub status: String,
    pub createdutc: String,
    pub updatedutc: String,
    pub eco_plane_scores: Option<EcoPlaneScores>,
}

#[derive(Clone, Debug)]
pub struct BlastRadiusEntry {
    pub sourcetype: String,
    pub sourceid: String,
    pub impact_plane: String,
    pub impact_score: f32,
}

#[derive(Clone, Debug)]
pub struct WorkloadWindowSummary {
    pub node_id: String,
    pub window_id: String,
    pub total_requests: u64,
    pub surplus_energy_j: f64,
    pub mean_vt_before: f64,
    pub mean_vt_after: f64,
    pub carbon_residual: f32,
    pub biodiversity_residual: Option<f32>,
    pub materials_residual: Option<f32>,
}

#[derive(Clone, Debug)]
pub struct StewardEcoWealthSnapshot {
    pub steward_did: String,
    pub window_id: String,
    pub k_before: f32,
    pub k_after: f32,
    pub e_before: f32,
    pub e_after: f32,
    pub r_before: f32,
    pub r_after: f32,
    pub risk_band_label: String,
}

//
// Smart-simplicity chat summaries (purely descriptive, non-actuating)
//

pub fn to_chat_summary_repo(repo: &RepoSummary) -> String {
    let ker = repo.kerbands.clamped();
    let mut planes = String::new();
    if !repo.planes.is_empty() {
        planes = repo.planes.join(", ");
    }
    let mut scope = String::new();
    if !repo.regionscope.is_empty() {
        scope = repo.regionscope.clone();
    }
    format!(
        "Artifact {logical} in repo {repo_name} is a {kind} in role band {role} for lane {lane} in region {region}. \
         Its KER targets are K={k:.2}, E={e:.2}, R={r:.2}, and it touches planes [{planes}]. \
         The artifact is marked non-actuating={nonactuating} and authored by {author}.",
        logical = repo.logicalname,
        repo_name = repo.repotarget,
        kind = repo.artifactkind,
        role = repo.roleband,
        lane = repo.lanedefault,
        region = scope,
        k = ker.k,
        e = ker.e,
        r = ker.r,
        planes = planes,
        nonactuating = repo.nonactuating,
        author = repo.authorbostrom,
    )
}

pub fn to_chat_summary_node(
    summary: &WorkloadWindowSummary,
    blast: &[BlastRadiusEntry],
) -> String {
    let vt_trend = if summary.mean_vt_after <= summary.mean_vt_before {
        "improved or remained stable"
    } else {
        "worsened"
    };

    let mut carbon_plane = "neutral";
    if summary.carbon_residual < 0.0 {
        carbon_plane = "net negative";
    } else if summary.carbon_residual > 0.0 {
        carbon_plane = "net positive";
    }

    let mut max_impact_plane = String::from("none");
    let mut max_impact_value = 0.0_f32;
    for entry in blast {
        if entry.impact_score > max_impact_value {
            max_impact_value = entry.impact_score;
            max_impact_plane = entry.impact_plane.clone();
        }
    }

    format!(
        "Node {node} over window {window} processed {req} workloads with surplus energy {surplus:.2} J. \
         The average post-Vt stability {vt_trend} (Vt_after={vt_after:.4}, Vt_before={vt_before:.4}), \
         and the net impact on the carbon plane was {carbon_plane}. \
         The strongest blast-radius impact was on the {plane} plane with score {impact:.2}.",
        node = summary.node_id,
        window = summary.window_id,
        req = summary.total_requests,
        surplus = summary.surplus_energy_j,
        vt_trend = vt_trend,
        vt_after = summary.mean_vt_after,
        vt_before = summary.mean_vt_before,
        carbon_plane = carbon_plane,
        plane = max_impact_plane,
        impact = max_impact_value,
    )
}

pub fn to_chat_summary_steward(snapshot: &StewardEcoWealthSnapshot) -> String {
    let k_delta = snapshot.k_after - snapshot.k_before;
    let e_delta = snapshot.e_after - snapshot.e_before;
    let r_delta = snapshot.r_after - snapshot.r_before;

    format!(
        "Steward {did} over window {window} moved to K={k_after:.2} (ΔK={dk:+.2}), \
         E={e_after:.2} (ΔE={de:+.2}), and R={r_after:.2} (ΔR={dr:+.2}), \
         with risk band {band}.",
        did = snapshot.steward_did,
        window = snapshot.window_id,
        k_after = snapshot.k_after,
        dk = k_delta,
        e_after = snapshot.e_after,
        de = e_delta,
        r_after = snapshot.r_after,
        dr = r_delta,
        band = snapshot.risk_band_label,
    )
}

//
// Non-actuating result envelopes for Kotlin/Lua FFI
//

#[derive(Clone, Debug)]
pub enum OverlayResult<T> {
    Ok(T),
    NotFound,
    InvalidJson(String),
    BackendError(String),
}

impl<T> OverlayResult<T> {
    pub fn is_ok(&self) -> bool {
        matches!(self, OverlayResult::Ok(_))
    }
}

//
// JSON envelopes to mirror econetfileindex and master index views
//

#[derive(Clone, Debug)]
pub struct EconetFileIndexRow {
    pub filename: String,
    pub destination: String,
    pub repotarget: String,
    pub roleband: RoleBand,
    pub lanedefault: LaneBand,
    pub regionscope: String,
    pub planes: Vec<String>,
    pub logicalname: String,
    pub artifactkind: String,
    pub econscope: String,
    pub nonactuating: bool,
    pub kerbands: KerBands,
    pub authorbostrom: String,
}

#[derive(Clone, Debug)]
pub struct MasterEcoScoreRow {
    pub artifactid: i64,
    pub shardname: String,
    pub ecoimpactvalue: f32,
    pub riskofharmvalue: f32,
    pub carbon_impact_sum: f32,
    pub biodiversity_impact_sum: f32,
}

//
// Non-actuating helper functions for JNI/Lua callers
//

pub fn rank_artifacts_for_chat(
    rows: &[MasterEcoScoreRow],
    limit: usize,
) -> Vec<MasterEcoScoreRow> {
    let mut sorted = rows.to_vec();
    sorted.sort_by(|a, b| {
        b.ecoimpactvalue
            .partial_cmp(&a.ecoimpactvalue)
            .unwrap_or(std::cmp::Ordering::Equal)
            .then(
                a.riskofharmvalue
                    .partial_cmp(&b.riskofharmvalue)
                    .unwrap_or(std::cmp::Ordering::Equal),
            )
            .then(
                a.carbon_impact_sum
                    .partial_cmp(&b.carbon_impact_sum)
                    .unwrap_or(std::cmp::Ordering::Equal),
            )
    });
    sorted.truncate(limit);
    sorted
}

pub fn now_iso8601() -> String {
    let now = SystemTime::now();
    let datetime: chrono::DateTime<chrono::Utc> = now.into();
    datetime.to_rfc3339()
}

//
// KER / eco-impact scoring for this module itself
//

#[derive(Clone, Debug)]
pub struct OverlayKerScore {
    pub k: f32,
    pub e: f32,
    pub r: f32,
}

pub fn overlay_ker_score() -> OverlayKerScore {
    OverlayKerScore {
        k: 0.94,
        e: 0.91,
        r: 0.12,
    }
}
