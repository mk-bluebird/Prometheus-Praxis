// Filename: crates/aln_qpudatashard_parser/src/lib.rs
use serde::{Deserialize, Serialize};

#[derive(Clone, Debug)]
pub enum AlnParseError {
    IoError(String),
    SyntaxError(String),
    MissingField(String),
    InvalidNumber(String),
    EvidenceMismatch { expected: String, computed: String },
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct QpuDataShardRow {
    pub node_id: String,
    pub window_start_ts: String,
    pub window_end_ts: String,
    pub energy_risk: f64,
    pub hydraulics_risk: f64,
    pub biology_risk: f64,
    pub carbon_risk: f64,
    pub materials_risk: f64,
    pub dataquality_risk: f64,
    pub vt: f64,
    pub ker_k: f64,
    pub ker_e: f64,
    pub ker_r: f64,
    pub evidence_hex: String,
}

/// Compute a simple hex evidence string from row content.
/// In production, this should be replaced with a permitted hash function
/// consistent with ALNDIDBostromStamp.[file:114]
pub fn compute_row_evidence_hex(row: &QpuDataShardRow) -> String {
    let payload = format!(
        "{}|{}|{}|{:.6}|{:.6}|{:.6}|{:.6}|{:.6}|{:.6}|{:.6}|{:.6}|{:.6}|{:.6}",
        row.node_id,
        row.window_start_ts,
        row.window_end_ts,
        row.energy_risk,
        row.hydraulics_risk,
        row.biology_risk,
        row.carbon_risk,
        row.materials_risk,
        row.dataquality_risk,
        row.vt,
        row.ker_k,
        row.ker_e,
        row.ker_r,
    );
    let bytes = payload.as_bytes();

    // Deterministic, non-cryptographic hex representation.
    // Replace with approved hash in your core when wiring to ALNDID stamps.[file:114]
    let mut acc: u64 = 0;
    for &b in bytes {
        acc = acc.wrapping_mul(109);
        acc = acc.wrapping_add(b as u64);
    }
    format!("0x{:016x}", acc)
}

fn parse_f64(label: &str, s: &str) -> Result<f64, AlnParseError> {
    s.parse::<f64>()
        .map_err(|_| AlnParseError::InvalidNumber(format!("{}='{}'", label, s)))
}

/// Parse a single `node_row` line into a strongly-typed QpuDataShardRow,
/// verifying that all required fields exist and evidence_hex matches row content.
pub fn parse_node_row_line(line: &str) -> Result<QpuDataShardRow, AlnParseError> {
    if !line.starts_with("node_row") {
        return Err(AlnParseError::SyntaxError(format!("not a node_row: {}", line)));
    }

    // Strip prefix and split on '|'
    let mut parts = line.trim_start_matches("node_row").trim().split('|');

    let node_id = parts.next().ok_or_else(|| AlnParseError::MissingField("node_id".to_string()))?.to_string();
    let window_start_ts = parts.next().ok_or_else(|| AlnParseError::MissingField("window_start_ts".to_string()))?.to_string();
    let window_end_ts = parts.next().ok_or_else(|| AlnParseError::MissingField("window_end_ts".to_string()))?.to_string();

    let energy_risk = parse_f64("energy_risk", parts.next().ok_or_else(|| AlnParseError::MissingField("energy_risk".to_string()))?)?;
    let hydraulics_risk = parse_f64("hydraulics_risk", parts.next().ok_or_else(|| AlnParseError::MissingField("hydraulics_risk".to_string()))?)?;
    let biology_risk = parse_f64("biology_risk", parts.next().ok_or_else(|| AlnParseError::MissingField("biology_risk".to_string()))?)?;
    let carbon_risk = parse_f64("carbon_risk", parts.next().ok_or_else(|| AlnParseError::MissingField("carbon_risk".to_string()))?)?;
    let materials_risk = parse_f64("materials_risk", parts.next().ok_or_else(|| AlnParseError::MissingField("materials_risk".to_string()))?)?;
    let dataquality_risk = parse_f64("dataquality_risk", parts.next().ok_or_else(|| AlnParseError::MissingField("dataquality_risk".to_string()))?)?;
    let vt = parse_f64("vt", parts.next().ok_or_else(|| AlnParseError::MissingField("vt".to_string()))?)?;
    let ker_k = parse_f64("ker_k", parts.next().ok_or_else(|| AlnParseError::MissingField("ker_k".to_string()))?)?;
    let ker_e = parse_f64("ker_e", parts.next().ok_or_else(|| AlnParseError::MissingField("ker_e".to_string()))?)?;
    let ker_r = parse_f64("ker_r", parts.next().ok_or_else(|| AlnParseError::MissingField("ker_r".to_string()))?)?;

    let evidence_hex = parts.next().ok_or_else(|| AlnParseError::MissingField("evidence_hex".to_string()))?.to_string();

    let row = QpuDataShardRow {
        node_id,
        window_start_ts,
        window_end_ts,
        energy_risk,
        hydraulics_risk,
        biology_risk,
        carbon_risk,
        materials_risk,
        dataquality_risk,
        vt,
        ker_k,
        ker_e,
        ker_r,
        evidence_hex,
    };

    // Verify evidence hex matches computed hash of row content.
    let computed_hex = compute_row_evidence_hex(&row);
    if row.evidence_hex != computed_hex {
        return Err(AlnParseError::EvidenceMismatch {
            expected: row.evidence_hex.clone(),
            computed: computed_hex,
        });
    }

    Ok(row)
}
