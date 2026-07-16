use eco_repo_index::{EcoRepoIndex, discover_aln_particles};
use std::path::Path;
use std::time::SystemTime;

fn now_utc_iso8601() -> String {
    let now = SystemTime::now();
    let datetime: chrono::DateTime<chrono::Utc> = now.into();
    datetime.to_rfc3339_opts(chrono::SecondsFormat::Secs, true)
}

fn main() {
    let aln_root = Path::new("aln");
    let entries = discover_aln_particles(aln_root);

    let index = EcoRepoIndex {
        version: "2026.1".to_string(),
        generated_at_utc: now_utc_iso8601(),
        entries,
    };

    let json = serde_json::to_string_pretty(&index).expect("failed to serialize index");
    std::fs::write("ecorepoindex.json", json).expect("failed to write ecorepoindex.json");
}
