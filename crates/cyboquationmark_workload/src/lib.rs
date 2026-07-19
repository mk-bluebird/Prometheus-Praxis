// Filename: crates/cyboquationmark_workload/src/lib.rs
// rust-version = "1.85"
// edition = "2024"

use rusqlite::{params, Connection, OpenFlags, Result as SqlResult};
use serde::{Deserialize, Serialize};
use std::path::Path;

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub enum MediaClass {
    Water,
    Air,
    Mixed,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub enum HydraulicImpact {
    Neutral,
    LocalPerturbation,
    GlobalPerturbation,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub enum WorkloadKind {
    ValveMove,
    Analytics,
    NanoswarmActuation,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DailyProgressRow {
    pub id: i64,
    pub ts_utc: String,
    pub node_id: String,
    pub delta_vt: f64,
    pub energyreqj: f64,
    pub vt_residual: f64,
    pub lane: String,
    pub roh: f64,
    pub ker_k: f64,
    pub ker_e: f64,
    pub ker_r: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkloadEnergyViolation {
    pub ts_utc: String,
    pub delta_vt: f64,
    pub energyreqj_prev: f64,
    pub energyreqj_next: f64,
    pub vt_prev: f64,
    pub vt_next: f64,
    pub roh: f64,
    pub reason: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkloadEnergyContractiveReport {
    pub node_id: String,
    pub lane: String,
    pub k_window: f64,
    pub e_window: f64,
    pub r_window: f64,
    pub roh_max: f64,
    pub vt_max: f64,
    pub vt_min: f64,
    pub contractive_ok: bool,
    pub violations: Vec<WorkloadEnergyViolation>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkloadEnergyConfig {
    pub k: f64,
    pub c: f64,
    pub roh_ceiling: f64,
    pub vt_ceiling: f64,
}

impl WorkloadEnergyConfig {
    pub fn default_prod() -> Self {
        WorkloadEnergyConfig {
            k: 0.85,
            c: 0.25,
            roh_ceiling: 0.13,
            vt_ceiling: 0.13,
        }
    }
}

pub fn open_daily_progress_db(path: &Path) -> SqlResult<Connection> {
    Connection::open_with_flags(
        path,
        OpenFlags::SQLITE_OPEN_READ_ONLY | OpenFlags::SQLITE_OPEN_NO_MUTEX,
    )
}

pub fn load_node_window(
    conn: &Connection,
    node_id: &str,
    lane: &str,
) -> SqlResult<Vec<DailyProgressRow>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            id,
            ts_utc,
            node_id,
            daily_deltaVt,
            energyreqJ,
            vt_residual,
            lane,
            roh,
            ker_k,
            ker_e,
            ker_r
        FROM db_cyboquatic_daily_progress
        WHERE node_id = ?1 AND lane = ?2
        ORDER BY ts_utc ASC
        "#,
    )?;

    let rows = stmt
        .query_map(params![node_id, lane], |row| {
            Ok(DailyProgressRow {
                id: row.get(0)?,
                ts_utc: row.get(1)?,
                node_id: row.get(2)?,
                delta_vt: row.get(3)?,
                energyreqj: row.get(4)?,
                vt_residual: row.get(5)?,
                lane: row.get(6)?,
                roh: row.get(7)?,
                ker_k: row.get(8)?,
                ker_e: row.get(9)?,
                ker_r: row.get(10)?,
            })
        })?
        .collect::<SqlResult<Vec<_>>>()?;

    Ok(rows)
}

#[inline]
fn expected_energy_next(cfg: &WorkloadEnergyConfig, energy_prev: f64, delta_vt: f64) -> f64 {
    let forcing = cfg.c * delta_vt * delta_vt;
    cfg.k * energy_prev + forcing
}

pub fn check_workload_energy_contractive(
    rows: &[DailyProgressRow],
    cfg: &WorkloadEnergyConfig,
) -> WorkloadEnergyContractiveReport {
    let mut violations = Vec::new();

    if rows.is_empty() {
        return WorkloadEnergyContractiveReport {
            node_id: String::new(),
            lane: String::new(),
            k_window: 0.0,
            e_window: 0.0,
            r_window: 0.0,
            roh_max: 0.0,
            vt_max: 0.0,
            vt_min: 0.0,
            contractive_ok: true,
            violations,
        };
    }

    let node_id = rows[0].node_id.clone();
    let lane = rows[0].lane.clone();

    let mut vt_max = f64::NEG_INFINITY;
    let mut vt_min = f64::INFINITY;
    let mut roh_max = f64::NEG_INFINITY;

    let mut k_sum = 0.0;
    let mut e_sum = 0.0;
    let mut r_sum = 0.0;
    let mut ker_count = 0usize;

    for w in rows {
        vt_max = vt_max.max(w.vt_residual);
        vt_min = vt_min.min(w.vt_residual);
        roh_max = roh_max.max(w.roh);
        k_sum += w.ker_k;
        e_sum += w.ker_e;
        r_sum += w.ker_r;
        ker_count += 1;
    }

    let k_window = if ker_count > 0 {
        k_sum / ker_count as f64
    } else {
        0.0
    };
    let e_window = if ker_count > 0 {
        e_sum / ker_count as f64
    } else {
        0.0
    };
    let r_window = if ker_count > 0 {
        r_sum / ker_count as f64
    } else {
        0.0
    };

    for pair in rows.windows(2) {
        let prev = &pair[0];
        let next = &pair[1];

        let e_prev = prev.energyreqj;
        let e_next = next.energyreqj;
        let vt_prev = prev.vt_residual;
        let vt_next = next.vt_residual;
        let delta_vt = next.delta_vt;

        let e_expected_max = expected_energy_next(cfg, e_prev, delta_vt);

        if e_next > e_expected_max + 1e-9 {
            violations.push(WorkloadEnergyViolation {
                ts_utc: next.ts_utc.clone(),
                delta_vt,
                energyreqj_prev: e_prev,
                energyreqj_next: e_next,
                vt_prev,
                vt_next,
                roh: next.roh,
                reason: format!(
                    "energyreqJ exceeded contractive bound: E_next={} > E_expected_max={}",
                    e_next, e_expected_max
                ),
            });
        }

        if vt_next > vt_prev + 1e-6 && vt_prev > cfg.vt_ceiling {
            violations.push(WorkloadEnergyViolation {
                ts_utc: next.ts_utc.clone(),
                delta_vt,
                energyreqj_prev: e_prev,
                energyreqj_next: e_next,
                vt_prev,
                vt_next,
                roh: next.roh,
                reason: format!(
                    "Lyapunov residual increased outside interior: V_prev={} V_next={}",
                    vt_prev, vt_next
                ),
            });
        }

        if next.roh > cfg.roh_ceiling + 1e-9 {
            violations.push(WorkloadEnergyViolation {
                ts_utc: next.ts_utc.clone(),
                delta_vt,
                energyreqj_prev: e_prev,
                energyreqj_next: e_next,
                vt_prev,
                vt_next,
                roh: next.roh,
                reason: format!(
                    "RoH ceiling breached: roh={} > roh_ceiling={}",
                    next.roh, cfg.roh_ceiling
                ),
            });
        }

        if vt_next > cfg.vt_ceiling + 1e-9 {
            violations.push(WorkloadEnergyViolation {
                ts_utc: next.ts_utc.clone(),
                delta_vt,
                energyreqj_prev: e_prev,
                energyreqj_next: e_next,
                vt_prev,
                vt_next,
                roh: next.roh,
                reason: format!(
                    "Vt ceiling breached: V_next={} > vt_ceiling={}",
                    vt_next, cfg.vt_ceiling
                ),
            });
        }
    }

    let contractive_ok = violations.is_empty();

    WorkloadEnergyContractiveReport {
        node_id,
        lane,
        k_window,
        e_window,
        r_window,
        roh_max,
        vt_max,
        vt_min,
        contractive_ok,
        violations,
    }
}

pub fn check_node_from_db<P: AsRef<Path>>(
    db_path: P,
    node_id: &str,
    lane: &str,
    cfg: &WorkloadEnergyConfig,
) -> SqlResult<WorkloadEnergyContractiveReport> {
    let conn = open_daily_progress_db(db_path.as_ref())?;
    let rows = load_node_window(&conn, node_id, lane)?;
    Ok(check_workload_energy_contractive(&rows, cfg))
}

#[no_mangle]
pub extern "C" fn cyboquatic_check_workload_energy_contractive_json(
    db_path_utf8: *const libc::c_char,
    node_id_utf8: *const libc::c_char,
    lane_utf8: *const libc::c_char,
    cfg_json_utf8: *const libc::c_char,
) -> *mut libc::c_char {
    use std::ffi::{CStr, CString};

    if db_path_utf8.is_null()
        || node_id_utf8.is_null()
        || lane_utf8.is_null()
        || cfg_json_utf8.is_null()
    {
        return std::ptr::null_mut();
    }

    let db_path_c = unsafe { CStr::from_ptr(db_path_utf8) };
    let node_id_c = unsafe { CStr::from_ptr(node_id_utf8) };
    let lane_c = unsafe { CStr::from_ptr(lane_utf8) };
    let cfg_c = unsafe { CStr::from_ptr(cfg_json_utf8) };

    let db_path_str = match db_path_c.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    let node_id_str = match node_id_c.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    let lane_str = match lane_c.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    let cfg_str = match cfg_c.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let cfg: WorkloadEnergyConfig = match serde_json::from_str(cfg_str) {
        Ok(c) => c,
        Err(_) => WorkloadEnergyConfig::default_prod(),
    };

    let report_res = check_node_from_db(db_path_str, node_id_str, lane_str, &cfg);
    let json = match report_res {
        Ok(report) => serde_json::to_string(&report).unwrap_or_else(|_| String::new()),
        Err(_) => String::new(),
    };

    match CString::new(json) {
        Ok(cstring) => cstring.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn cyboquatic_free_json(ptr: *mut libc::c_char) {
    if ptr.is_null() {
        return;
    }
    unsafe {
        let _ = std::ffi::CString::from_raw(ptr);
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ValveMovePayload {
    pub valve_id: String,
    pub open_fraction: f64,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnalyticsPayload {
    pub job_id: String,
    pub input_shard_id: String,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct NanoswarmPayload {
    pub swarm_id: String,
    pub target_node_id: String,
    pub duration_s: f64,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum CyboquationmarkPayload {
    ValveMove(ValveMovePayload),
    Analytics(AnalyticsPayload),
    NanoswarmActuation(NanoswarmPayload),
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CyboquationmarkVariant {
    pub kind: WorkloadKind,
    pub payload: CyboquationmarkPayload,
    pub media_class: MediaClass,
    pub hydraulic_impact: HydraulicImpact,
    pub safety_factor: f64,
    pub energy_req_j: f64,
    pub expected_delta_vt: f64,
}

pub trait CyboquaticWorkload {
    fn energyreq_j(&self) -> f64;
    fn safetyfactor(&self) -> f64;
    fn media_class(&self) -> MediaClass;
    fn hydraulicimpact(&self) -> HydraulicImpact;
    fn expected_delta_vt(&self) -> f64;
}

impl CyboquaticWorkload for CyboquationmarkVariant {
    fn energyreq_j(&self) -> f64 {
        self.energy_req_j
    }

    fn safetyfactor(&self) -> f64 {
        self.safety_factor
    }

    fn media_class(&self) -> MediaClass {
        self.media_class
    }

    fn hydraulicimpact(&self) -> HydraulicImpact {
        self.hydraulic_impact
    }

    fn expected_delta_vt(&self) -> f64 {
        self.expected_delta_vt
    }
}

pub fn to_json(w: &CyboquationmarkVariant) -> serde_json::Result<String> {
    serde_json::to_string(w)
}

pub fn from_json(s: &str) -> serde_json::Result<CyboquationmarkVariant> {
    serde_json::from_str(s)
}
