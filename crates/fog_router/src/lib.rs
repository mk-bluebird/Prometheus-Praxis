// Filename: crates/fog_router/src/lib.rs
// License: MIT OR Apache-2.0
// Rust edition: 2024
// rust-version = "1.85"

#![cfg_attr(not(test), no_std)]

use core::marker::PhantomData;
use mlua::{Function, Lua, Result as LuaResult, Table, Value};
use serde::{Deserialize, Serialize};

/// Minimal qpudatashard view for routing decisions.
/// This struct mirrors the Lyapunov and predicate context exposed by ecosafety shards.
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

/// Workload descriptor for FOG routing and ecosafety checks.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct WorkloadDescriptor {
    pub workload_id: String,
    pub energy_req_j: f64,
    pub media_class: String,
}

/// Router decision for FOG routing layer.
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

/// Pure predicate bundle for ecosafety routing.
/// All fields are required to be true, and the Lyapunov residual must be non-increasing.
fn all_predicates_hold(shard: &QpuDataShard) -> bool {
    shard.tailwind_valid
        && shard.biosurface_ok
        && shard.hydraulic_ok
        && shard.lyapunov_ok
        && shard.vt_next_est <= shard.vt_prev
}

/// Core routing evaluation: enforce ecosafety predicates and decide Accept/Reroute/Reject.
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

/// FOG routing predicate test result from Lua.
/// Scripts may classify shards as `route_ok`, `needs_desiccation`, or `block`.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum FogRoutingDecision {
    RouteOk,
    NeedsDesiccation,
    Block,
}

/// Immutable view of a FOG shard row for Lua inspection.
#[derive(Debug, Copy, Clone)]
pub struct FogShardView {
    pub node_id: &'static str,
    pub medium: &'static str,
    pub r_fog: f32,
    pub r_energy: f32,
    pub r_hydraulics: f32,
}

/// Sandboxed Lua engine for evaluating FOG routing predicates on live shard dumps
/// without recompiling Rust or allowing state mutation.
pub struct FogLuaSandbox<'lua> {
    lua: Lua,
    _marker: PhantomData<&'lua ()>,
}

impl<'lua> FogLuaSandbox<'lua> {
    /// Create a new sandboxed Lua environment with a restricted API.
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

    /// Evaluate a routing predicate Lua snippet against a single shard view.
    ///
    /// The Lua source must define:
    ///
    ///     function decide(shard)
    ///       -- shard.node_id, shard.medium, shard.r_fog, shard.r_energy, shard.r_hydraulics
    ///       -- return "route_ok", "needs_desiccation", or "block"
    ///     end
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

        let decision = sandbox
            .eval_predicate(lua_src, shard)
            .expect("decision");
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

        let decision = sandbox
            .eval_predicate(lua_src, shard)
            .expect("decision");
        assert_eq!(decision, FogRoutingDecision::Block);
    }
}
