// Filename: crates/fog_router/src/lib.rs
// License: MIT OR Apache-2.0
// Rust edition: 2024
// rust-version = "1.85"

#![cfg_attr(not(test), no_std)]
#![forbid(unsafe_code)]

extern crate alloc;

use alloc::string::{String, ToString};
use alloc::vec::Vec;
use core::marker::PhantomData;

use mlua::{Function, Lua, Result as LuaResult, Table, Value};
use rusqlite::{params, Connection, Result as SqlResult};
use serde::{Deserialize, Serialize};

use cyboquatic_ecosafety::{
    CyboLane,
    CyboNodeEcosafetyEnvelope,
    FogGuardConfig,
    FogGuardVerdict,
    KERWindow,
    LyapunovResidual,
    LyapunovWeights,
    RiskCoord,
    RiskVector,
    safestep,
};

use blacklist_trie::{BlacklistTrie, PatternCategory};

fn route_prompt(trie: &BlacklistTrie, prompt: &str) -> RouterDecision {
    let matches = trie.matches_all(prompt);
    if !matches.is_empty() {
        let m = &matches[0];
        match m.category {
            PatternCategory::Hydraulic => RouterDecision::Reject {
                reason: "hydraulic actuator blacklist".into(),
            },
            PatternCategory::Energy => RouterDecision::Reject {
                reason: "energy actuator blacklist".into(),
            },
            PatternCategory::Biology => RouterDecision::Reject {
                reason: "biology actuator blacklist".into(),
            },
            PatternCategory::Governance => RouterDecision::Reject {
                reason: "governance/policy blacklist".into(),
            },
            PatternCategory::Generic => RouterDecision::Reject {
                reason: "generic actuator blacklist".into(),
            },
        }
    } else {
        RouterDecision::Accept {
            node_id: "none".to_string(),
            projected_shard: QpuDataShard {
                node_id: "none".to_string(),
                vt_prev: 0.0,
                vt_next_est: 0.0,
                tailwind_valid: true,
                biosurface_ok: true,
                hydraulic_ok: true,
                lyapunov_ok: true,
            },
        }
    }
}

/// Minimal qpudatashard view for routing decisions.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct QpuDataShard {
    pub node_id: String,
    pub vt_prev: f64,
    pub vt_next_est: f64,
    pub tailwind_valid: bool,
    pub biosurface_ok: bool,
    pub hydraulic_ok: bool,
    pub lyapunov_ok: bool,
}

/// High-level route decision for a FOG node window.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum FogRouteDecision {
    AllowRoute,
    BlockRoute,
}

/// Minimal snapshot of FOG node state needed for ecosafety routing.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct FogNodeSnapshot {
    pub lane: CyboLane,
    pub risk: RiskVector,
    pub ker_window: KERWindow,
    pub prev_residual: LyapunovResidual,
    pub evidencehex: String,
    pub did: String,
    pub corridor_present: bool,
}

fn envelope_from_snapshot(snapshot: &FogNodeSnapshot) -> CyboNodeEcosafetyEnvelope {
    let weights = LyapunovWeights::equal();
    CyboNodeEcosafetyEnvelope::new(
        snapshot.lane,
        snapshot.risk,
        weights,
        snapshot.prev_residual,
        snapshot.ker_window,
        snapshot.evidencehex.clone(),
        snapshot.did.clone(),
    )
}

pub fn decide_route(snapshot: &FogNodeSnapshot, cfg: Option<FogGuardConfig>) -> FogRouteDecision {
    let envelope = envelope_from_snapshot(snapshot);
    let verdict = safestep(&envelope, snapshot.corridor_present, cfg);
    match verdict {
        FogGuardVerdict::Allow => FogRouteDecision::AllowRoute,
        FogGuardVerdict::Stop => FogRouteDecision::BlockRoute,
    }
}

/// Shared risk coordinates stored in SQLite.
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct RiskCoords {
    pub timestamp_utc: String,
    pub rhydraulic: f32,
    pub renergy: f32,
    pub rbio: f32,
    pub rtox: f32,
    pub rmicro: f32,
    pub rmaterials: f32,
    pub rcarbon: f32,
    pub rcalib: f32,
    pub rsigma: f32,
}

/// Tree-of-Life weights aligned to governance ALN.
#[derive(Debug, Clone, Copy)]
pub struct RiskWeights {
    pub whydraulic: f32,
    pub wenergy: f32,
    pub wbio: f32,
    pub wtox: f32,
    pub wmicro: f32,
    pub wmaterials: f32,
    pub wcarbon: f32,
    pub wcalib: f32,
    pub wsigma: f32,
}

pub const TREE_OF_LIFE_WEIGHTS_PHX_V1: RiskWeights = RiskWeights {
    whydraulic: 1.0,
    wenergy: 0.8,
    wbio: 1.5,
    wtox: 1.5,
    wmicro: 1.2,
    wmaterials: 1.0,
    wcarbon: 1.0,
    wcalib: 0.6,
    wsigma: 0.6,
};

pub fn lyapunov_residual(coords: &RiskCoords, weights: RiskWeights) -> f32 {
    let mut v = 0.0_f32;
    v += weights.whydraulic * coords.rhydraulic * coords.rhydraulic;
    v += weights.wenergy * coords.renergy * coords.renergy;
    v += weights.wbio * coords.rbio * coords.rbio;
    v += weights.wtox * coords.rtox * coords.rtox;
    v += weights.wmicro * coords.rmicro * coords.rmicro;
    v += weights.wmaterials * coords.rmaterials * coords.rmaterials;
    v += weights.wcarbon * coords.rcarbon * coords.rcarbon;
    v += weights.wcalib * coords.rcalib * coords.rcalib;
    v += weights.wsigma * coords.rsigma * coords.rsigma;
    v
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum RoutingVerdict {
    Allow,
    SuggestOnly,
    Deny,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RoutingDecision {
    pub timestamp_utc: String,
    pub node_id: String,
    pub previous_v: f32,
    pub current_v: f32,
    pub verdict: RoutingVerdict,
    pub diagnostic_only: bool,
    pub evidence_hex: String,
}

pub fn build_evidence_hex(
    node_id: &str,
    timestamp_utc: &str,
    previous_v: f32,
    current_v: f32,
) -> String {
    use core::fmt::Write;
    let mut s = String::new();
    let payload = format!("{node_id}|{timestamp_utc}|{previous_v:.6}|{current_v:.6}");
    for b in payload.as_bytes() {
        write!(&mut s, "{:02x}", b).expect("hex write should not fail");
    }
    s
}

#[derive(Debug, Clone, Copy)]
pub struct HysteresisConfig {
    pub delta_v_min: f32,
}

pub fn lyapunov_ok(previous_v: f32, current_v: f32) -> bool {
    current_v <= previous_v
}

pub fn evaluate_routing_decision(
    node_id: &str,
    previous: &RiskCoords,
    current: &RiskCoords,
    weights: RiskWeights,
    hyst: HysteresisConfig,
    diagnostic_only: bool,
) -> RoutingDecision {
    let previous_v = lyapunov_residual(previous, weights);
    let current_v = lyapunov_residual(current, weights);

    let delta_v = (current_v - previous_v).abs();
    let verdict = if delta_v < hyst.delta_v_min {
        RoutingVerdict::SuggestOnly
    } else if lyapunov_ok(previous_v, current_v) {
        if diagnostic_only {
            RoutingVerdict::SuggestOnly
        } else {
            RoutingVerdict::Allow
        }
    } else {
        RoutingVerdict::Deny
    };

    let evidence_hex = build_evidence_hex(node_id, &current.timestamp_utc, previous_v, current_v);

    RoutingDecision {
        timestamp_utc: current.timestamp_utc.clone(),
        node_id: node_id.to_owned(),
        previous_v,
        current_v,
        verdict,
        diagnostic_only,
        evidence_hex,
    }
}

pub fn log_routing_decision(conn: &Connection, decision: &RoutingDecision) -> SqlResult<()> {
    conn.execute(
        "INSERT INTO fog_routing_decisions (
            timestamp_utc,
            node_id,
            previous_v,
            current_v,
            verdict,
            diagnostic_only,
            evidence_hex
        ) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)",
        params![
            decision.timestamp_utc,
            decision.node_id,
            decision.previous_v,
            decision.current_v,
            format!("{:?}", decision.verdict),
            if decision.diagnostic_only { 1_i64 } else { 0_i64 },
            decision.evidence_hex
        ],
    )?;
    Ok(())
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct WorkloadDescriptor {
    pub workload_id: String,
    pub energy_req_j: f64,
    pub media_class: String,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum RouterDecision {
    Accept {
        node_id: String,
        projected_shard: QpuDataShard,
    },
    Reject {
        reason: String,
    },
    Reroute {
        suggested_node_id: String,
        reason: String,
    },
}

fn all_predicates_hold(shard: &QpuDataShard) -> bool {
    shard.tailwind_valid
        && shard.biosurface_ok
        && shard.hydraulic_ok
        && shard.lyapunov_ok
        && shard.vt_next_est <= shard.vt_prev
}

pub fn evaluate_workload(
    shard: &QpuDataShard,
    _workload: &WorkloadDescriptor,
    reroute_node_id: Option<String>,
) -> RouterDecision {
    if !all_predicates_hold(shard) {
        if let Some(node) = reroute_node_id {
            RouterDecision::Reroute {
                suggested_node_id: node,
                reason: "predicate failure, rerouting workload".to_string(),
            }
        } else {
            RouterDecision::Reject {
                reason: "predicate failure, reject workload".to_string(),
            }
        }
    } else {
        let projected = QpuDataShard {
            node_id: shard.node_id.clone(),
            vt_prev: shard.vt_prev,
            vt_next_est: shard.vt_next_est,
            tailwind_valid: shard.tailwind_valid,
            biosurface_ok: shard.biosurface_ok,
            hydraulic_ok: shard.hydraulic_ok,
            lyapunov_ok: shard.lyapunov_ok,
        };

        RouterDecision::Accept {
            node_id: shard.node_id.clone(),
            projected_shard: projected,
        }
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum FogRoutingDecision {
    RouteOk,
    NeedsDesiccation,
    Block,
}

#[derive(Debug, Copy, Clone)]
pub struct FogShardView {
    pub node_id: &'static str,
    pub medium: &'static str,
    pub r_fog: f32,
    pub r_energy: f32,
    pub r_hydraulics: f32,
}

pub struct FogLuaSandbox<'lua> {
    lua: Lua,
    _marker: PhantomData<&'lua ()>,
}

impl<'lua> FogLuaSandbox<'lua> {
    pub fn new() -> LuaResult<FogLuaSandbox<'lua>> {
        let lua = Lua::new();

        let globals = lua.globals();
        let cain_table = lua.create_table()?;

        let classify_risk_fn =
            lua.create_function(|_, (r_fog, r_energy, r_hydraulics): (f32, f32, f32)| {
                let max_r = r_fog.max(r_energy).max(r_hydraulics);
                let level = if max_r < 0.33 {
                    "low"
                } else if max_r < 0.66 {
                    "medium"
                } else {
                    "high"
                };
                Ok(level.to_string())
            })?;
        cain_table.set("classify_risk", classify_risk_fn)?;

        let corridor_gate_fn =
            lua.create_function(|_, (r_fog, r_energy, r_hydraulics): (f32, f32, f32)| {
                let max_r = r_fog.max(r_energy).max(r_hydraulics);
                let ok = max_r <= 0.90;
                Ok(ok)
            })?;
        cain_table.set("corridor_ok", corridor_gate_fn)?;

        globals.set("cain", cain_table)?;

        Ok(FogLuaSandbox {
            lua,
            _marker: PhantomData,
        })
    }

    pub fn eval_predicate(
        &self,
        lua_source: &str,
        shard: FogShardView,
    ) -> LuaResult<FogRoutingDecision> {
        self.lua.load(lua_source).exec()?;

        let globals = self.lua.globals();
        let decide_fn: Function = globals.get("decide")?;

        let shard_table: Table = self.lua.create_table()?;
        shard_table.set("node_id", shard.node_id)?;
        shard_table.set("medium", shard.medium)?;
        shard_table.set("r_fog", shard.r_fog)?;
        shard_table.set("r_energy", shard.r_energy)?;
        shard_table.set("r_hydraulics", shard.r_hydraulics)?;

        let decision_value: Value = decide_fn.call(shard_table)?;
        let decision_str = match decision_value {
            Value::String(s) => s.to_str()?.to_owned(),
            _ => {
                return Err(mlua::Error::RuntimeError(
                    "decide() must return a string".into(),
                ))
            }
        };

        let decision = match decision_str.as_str() {
            "route_ok" => FogRoutingDecision::RouteOk,
            "needs_desiccation" => FogRoutingDecision::NeedsDesiccation,
            "block" => FogRoutingDecision::Block,
            _ => {
                return Err(mlua::Error::RuntimeError(
                    "decide() must return one of 'route_ok', 'needs_desiccation', 'block'".into(),
                ))
            }
        };

        Ok(decision)
    }
}

#[derive(Copy, Clone)]
pub struct CanalState {
    pub q_m3_s: f64,
    pub t_c: f64,
    pub h_m: f64,
    pub theta_mm: f64,
}

#[derive(Copy, Clone)]
pub struct CanalObs {
    pub et_mm_day: f64,
    pub q_m3_s: f64,
    pub t_c: f64,
}

#[derive(Copy, Clone)]
pub struct CanalRiskCoords {
    pub r_thermal: f64,
    pub r_hydraulic: f64,
    pub r_recharge: f64,
}

#[derive(Copy, Clone)]
pub struct CanalLyapunovWeights {
    pub w_thermal: f64,
    pub w_hydraulic: f64,
    pub w_recharge: f64,
}

#[derive(Copy, Clone)]
pub struct EnsembleMember {
    pub state: CanalState,
}

fn clamp01(x: f64) -> f64 {
    if x < 0.0 {
        0.0
    } else if x > 1.0 {
        1.0
    } else {
        x
    }
}

pub fn state_to_risk(
    state: &CanalState,
    t_gold_c: f64,
    t_hard_c: f64,
    hlr_gold: f64,
    hlr_hard: f64,
    recharge_gold: f64,
    recharge_hard: f64,
) -> CanalRiskCoords {
    let r_thermal = if state.t_c <= t_gold_c {
        0.0
    } else if state.t_c >= t_hard_c {
        1.0
    } else {
        clamp01((state.t_c - t_gold_c) / (t_hard_c - t_gold_c))
    };

    let r_hydraulic = if state.h_m <= hlr_gold {
        0.0
    } else if state.h_m >= hlr_hard {
        1.0
    } else {
        clamp01((state.h_m - hlr_gold) / (hlr_hard - hlr_gold))
    };

    let r_recharge = if state.theta_mm <= recharge_gold {
        0.0
    } else if state.theta_mm >= recharge_hard {
        1.0
    } else {
        clamp01((state.theta_mm - recharge_gold) / (recharge_hard - recharge_gold))
    };

    CanalRiskCoords {
        r_thermal,
        r_hydraulic,
        r_recharge,
    }
}

pub fn compute_vt(risk: &CanalRiskCoords, w: &CanalLyapunovWeights) -> f64 {
    let v = w.w_thermal * risk.r_thermal * risk.r_thermal
        + w.w_hydraulic * risk.r_hydraulic * risk.r_hydraulic
        + w.w_recharge * risk.r_recharge * risk.r_recharge;
    if v < 0.0 {
        0.0
    } else {
        v
    }
}

pub fn forecast_state(
    prev: &CanalState,
    dq_ops_m3_s: f64,
    dq_loss_m3_s: f64,
    dt_s: f64,
    solar_heat_w_m2: f64,
    exchange_coeff_w_m2_c: f64,
) -> CanalState {
    let q_next = (prev.q_m3_s + dq_ops_m3_s - dq_loss_m3_s).max(0.0);

    let rho_cp: f64 = 4.18e6;
    let depth_m: f64 = 2.0;
    let area_m2: f64 = depth_m;
    let heat_net = solar_heat_w_m2 * area_m2 - exchange_coeff_w_m2_c * (prev.t_c - 25.0);
    let denom = rho_cp * depth_m * q_next.max(0.1);
    let d_t = (heat_net * dt_s) / denom;
    let t_next = prev.t_c + d_t;

    let h_next = prev.h_m;
    let theta_next = prev.theta_mm;

    CanalState {
        q_m3_s: q_next,
        t_c: t_next,
        h_m: h_next,
        theta_mm: theta_next,
    }
}

pub fn observe_state(state: &CanalState) -> CanalObs {
    let et_mm_day = (state.theta_mm * 0.05 + (state.t_c - 15.0) * 0.2).max(0.0);
    CanalObs {
        et_mm_day,
        q_m3_s: state.q_m3_s,
        t_c: state.t_c,
    }
}

pub fn enkf_update(
    ensemble: &mut [EnsembleMember],
    obs: CanalObs,
    prev_vt: f64,
    weights: &CanalLyapunovWeights,
    corridors: (f64, f64, f64, f64, f64, f64),
    max_delta_vt: f64,
) {
    let n = ensemble.len() as f64;
    if n <= 1.0 {
        return;
    }

    let mut mean_et = 0.0;
    let mut mean_q = 0.0;
    let mut mean_t = 0.0;

    let mut pred_obs: Vec<CanalObs> = Vec::new();

    for m in ensemble.iter_mut() {
        let forecast = forecast_state(&m.state, 0.0, 0.0, 900.0, 200.0, 40.0);
        m.state = forecast;
        let o = observe_state(&forecast);
        pred_obs.push(o);
        mean_et += o.et_mm_day;
        mean_q += o.q_m3_s;
        mean_t += o.t_c;
    }

    mean_et /= n;
    mean_q /= n;
    mean_t /= n;

    let mut var_et = 0.0;
    let mut var_q = 0.0;
    let mut var_t = 0.0;

    for o in pred_obs.iter() {
        var_et += (o.et_mm_day - mean_et).powi(2);
        var_q += (o.q_m3_s - mean_q).powi(2);
        var_t += (o.t_c - mean_t).powi(2);
    }

    var_et /= n - 1.0;
    var_q /= n - 1.0;
    var_t /= n - 1.0;

    let r_et: f64 = 1.0;
    let r_q: f64 = 0.1;
    let r_t: f64 = 0.5;

    let k_et = var_et / (var_et + r_et);
    let k_q = var_q / (var_q + r_q);
    let k_t = var_t / (var_t + r_t);

    let (t_gold, t_hard, hlr_gold, hlr_hard, rech_gold, rech_hard) = corridors;

    for (idx, m) in ensemble.iter_mut().enumerate() {
        let o_pred = pred_obs[idx];
        let de = obs.et_mm_day - o_pred.et_mm_day;
        let dq = obs.q_m3_s - o_pred.q_m3_s;
        let dt_obs = obs.t_c - o_pred.t_c;

        let mut updated = m.state;
        updated.theta_mm += k_et * de;
        updated.q_m3_s += k_q * dq;
        updated.t_c += k_t * dt_obs;

        let risk =
            state_to_risk(&updated, t_gold, t_hard, hlr_gold, hlr_hard, rech_gold, rech_hard);
        let vt_new = compute_vt(&risk, weights);

        if vt_new <= prev_vt + max_delta_vt {
            m.state = updated;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_all_predicates_hold() {
        let shard = QpuDataShard {
            node_id: "NODE-01".to_string(),
            vt_prev: 0.10,
            vt_next_est: 0.08,
            tailwind_valid: true,
            biosurface_ok: true,
            hydraulic_ok: true,
            lyapunov_ok: true,
        };
        assert!(all_predicates_hold(&shard));
    }

    #[test]
    fn test_reject_on_predicate_failure() {
        let shard = QpuDataShard {
            node_id: "NODE-02".to_string(),
            vt_prev: 0.10,
            vt_next_est: 0.12,
            tailwind_valid: true,
            biosurface_ok: false,
            hydraulic_ok: true,
            lyapunov_ok: true,
        };
        let workload = WorkloadDescriptor {
            workload_id: "WL-01".to_string(),
            energy_req_j: 1000.0,
            media_class: "FOG".to_string(),
        };
        let decision = evaluate_workload(&shard, &workload, None);
        match decision {
            RouterDecision::Reject { .. } => {}
            _ => panic!("expected Reject"),
        }
    }

    #[test]
    fn test_reroute_on_predicate_failure() {
        let shard = QpuDataShard {
            node_id: "NODE-03".to_string(),
            vt_prev: 0.10,
            vt_next_est: 0.15,
            tailwind_valid: false,
            biosurface_ok: true,
            hydraulic_ok: true,
            lyapunov_ok: true,
        };
        let workload = WorkloadDescriptor {
            workload_id: "WL-02".to_string(),
            energy_req_j: 500.0,
            media_class: "FOG".to_string(),
        };
        let decision = evaluate_workload(&shard, &workload, Some("NODE-ALT".to_string()));
        match decision {
            RouterDecision::Reroute { suggested_node_id, .. } => {
                assert_eq!(suggested_node_id, "NODE-ALT");
            }
            _ => panic!("expected Reroute"),
        }
    }

    #[test]
    fn test_accept_on_all_predicates() {
        let shard = QpuDataShard {
            node_id: "NODE-04".to_string(),
            vt_prev: 0.12,
            vt_next_est: 0.10,
            tailwind_valid: true,
            biosurface_ok: true,
            hydraulic_ok: true,
            lyapunov_ok: true,
        };
        let workload = WorkloadDescriptor {
            workload_id: "WL-03".to_string(),
            energy_req_j: 750.0,
            media_class: "FOG".to_string(),
        };
        let decision = evaluate_workload(&shard, &workload, None);
        match decision {
            RouterDecision::Accept {
                node_id,
                projected_shard,
            } => {
                assert_eq!(node_id, "NODE-04");
                assert_eq!(projected_shard.node_id, "NODE-04");
                assert_eq!(projected_shard.vt_next_est, shard.vt_next_est);
            }
            _ => panic!("expected Accept"),
        }
    }

    #[test]
    fn test_basic_routing_decision_lua() {
        let sandbox = FogLuaSandbox::new().expect("sandbox init");

        let shard = FogShardView {
            node_id: "FOG-NODE-01",
            medium: "FOG",
            r_fog: 0.4,
            r_energy: 0.3,
            r_hydraulics: 0.2,
        };

        let lua_src = r#"
            function decide(shard)
                local risk = cain.classify_risk(shard.r_fog, shard.r_energy, shard.r_hydraulics)
                if risk == "low" then
                    return "route_ok"
                elseif risk == "medium" then
                    if cain.corridor_ok(shard.r_fog, shard.r_energy, shard.r_hydraulics) then
                        return "needs_desiccation"
                    else
                        return "block"
                    end
                else
                    return "block"
                end
            end
        "#;

        let decision = sandbox.eval_predicate(lua_src, shard).expect("decision");
        assert_eq!(decision, FogRoutingDecision::NeedsDesiccation);
    }

    #[test]
    fn test_block_high_risk_lua() {
        let sandbox = FogLuaSandbox::new().expect("sandbox init");

        let shard = FogShardView {
            node_id: "FOG-NODE-02",
            medium: "FOG",
            r_fog: 0.95,
            r_energy: 0.1,
            r_hydraulics: 0.2,
        };

        let lua_src = r#"
            function decide(shard)
                local ok = cain.corridor_ok(shard.r_fog, shard.r_energy, shard.r_hydraulics)
                if ok then
                    return "route_ok"
                else
                    return "block"
                end
            end
        "#;

        let decision = sandbox.eval_predicate(lua_src, shard).expect("decision");
        assert_eq!(decision, FogRoutingDecision::Block);
    }

    #[test]
    fn test_enkf_nonexpansive_vt() {
        let mut ensemble = [EnsembleMember {
            state: CanalState {
                q_m3_s: 5.0,
                t_c: 25.0,
                h_m: 2.0,
                theta_mm: 50.0,
            },
        }; 8];

        let obs = CanalObs {
            et_mm_day: 6.0,
            q_m3_s: 5.5,
            t_c: 26.0,
        };

        let weights = CanalLyapunovWeights {
            w_thermal: 0.5,
            w_hydraulic: 0.3,
            w_recharge: 0.2,
        };

        let corridors = (24.0, 30.0, 1.5, 3.0, 40.0, 80.0);

        let risk_prev = state_to_risk(
            &ensemble[0].state,
            corridors.0,
            corridors.1,
            corridors.2,
            corridors.3,
            corridors.4,
            corridors.5,
        );
        let vt_prev = compute_vt(&risk_prev, &weights);

        enkf_update(
            &mut ensemble,
            obs,
            vt_prev,
            &weights,
            corridors,
            0.01,
        );

        let risk_new = state_to_risk(
            &ensemble[0].state,
            corridors.0,
            corridors.1,
            corridors.2,
            corridors.3,
            corridors.4,
            corridors.5,
        );
        let vt_new = compute_vt(&risk_new, &weights);

        assert!(vt_new <= vt_prev + 0.01 + 1e-6);
    }
}
