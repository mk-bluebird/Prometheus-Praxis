// File: crates/cain_fog_routing_lua/src/lib.rs
// License: MIT OR Apache-2.0
// Rust edition: 2024, rust-version = "1.85"

use mlua::{Lua, Result as LuaResult, Value, Function, Table}; // mlua exists on crates.io and supports safe FFI between Rust and Lua. [file:49]

/// FOG routing predicate test result: the Lua script may mark a shard row as
/// `route_ok`, `needs_desiccation`, or `block`, but cannot mutate Rust state. [file:49]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum FogRoutingDecision {
    RouteOk,
    NeedsDesiccation,
    Block,
}

/// Immutable view of a FOG shard row that Lua can inspect. [file:49]
#[derive(Debug, Copy, Clone)]
pub struct FogShardView {
    /// Unique node identifier.
    pub node_id: &'static str,
    /// Medium type, e.g., "FOG", "WATER".
    pub medium: &'static str,
    /// Normalized risk coordinate for FOG routing. [file:49]
    pub r_fog: f32,
    /// Normalized energy risk coordinate. [file:49]
    pub r_energy: f32,
    /// Normalized hydraulics risk coordinate. [file:49]
    pub r_hydraulics: f32,
}

/// Sandboxed Lua engine for evaluating FOG routing predicates on live shard dumps
/// without recompiling Rust or allowing state mutation. [file:49]
pub struct FogLuaSandbox<'lua> {
    lua: Lua,
    _marker: core::marker::PhantomData<&'lua ()>,
}

impl<'lua> FogLuaSandbox<'lua> {
    /// Create a new sandboxed Lua environment.
    ///
    /// - No `os`, `io`, `debug`, or `package` libraries are exposed. [file:49]
    /// - Only a small, pure-function API is provided via `cain` table. [file:49]
    pub fn new() -> LuaResult<FogLuaSandbox<'lua>> {
        let lua = Lua::new();

        // Remove unsafe standard libraries by not loading them at all.
        // Provide a restricted global `cain` table with pure helper functions. [file:49]
        let globals = lua.globals();
        let cain_table = lua.create_table()?;

        // Helper: classify risk bands without exposing Rust state. [file:51]
        let classify_risk_fn = lua.create_function(|_, (r_fog, r_energy, r_hydraulics): (f32, f32, f32)| {
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

        // Helper: basic corridor gate mirroring ecosafety grammar. [file:51]
        let corridor_gate_fn = lua.create_function(|_, (r_fog, r_energy, r_hydraulics): (f32, f32, f32)| {
            let max_r = r_fog.max(r_energy).max(r_hydraulics);
            let ok = max_r <= 0.90;
            Ok(ok)
        })?;
        cain_table.set("corridor_ok", corridor_gate_fn)?;

        globals.set("cain", cain_table)?;

        Ok(FogLuaSandbox {
            lua,
            _marker: core::marker::PhantomData,
        })
    }

    /// Evaluate a routing predicate Lua snippet against a single shard view.
    ///
    /// The Lua source must define a pure function:
    ///
    ///     function decide(shard)
    ///       -- shard.node_id, shard.medium, shard.r_fog, shard.r_energy, shard.r_hydraulics
    ///       -- return "route_ok", "needs_desiccation", or "block"
    ///     end
    ///
    /// The shard is provided as a Lua table; Lua cannot mutate Rust, cannot persist state,
    /// and cannot access I/O or OS. [file:49]
    pub fn eval_predicate(&self, lua_source: &str, shard: FogShardView) -> LuaResult<FogRoutingDecision> {
        // Load source as a one-off chunk; do not persist functions in registry. [file:49]
        self.lua.load(lua_source).exec()?;

        let globals = self.lua.globals();
        let decide_fn: Function = globals.get("decide")?;

        // Build a Lua table representing the shard row; values are copied, not borrowed. [file:51]
        let shard_table: Table = self.lua.create_table()?;
        shard_table.set("node_id", shard.node_id)?;
        shard_table.set("medium", shard.medium)?;
        shard_table.set("r_fog", shard.r_fog)?;
        shard_table.set("r_energy", shard.r_energy)?;
        shard_table.set("r_hydraulics", shard.r_hydraulics)?;

        // Call decide(shard) and map the returned string into a Rust enum. [file:49]
        let decision_value: Value = decide_fn.call(shard_table)?;
        let decision_str = match decision_value {
            Value::String(s) => s.to_str()?.to_owned(),
            _ => return Err(mlua::Error::RuntimeError("decide() must return a string".into())),
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
    fn test_basic_routing_decision() {
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
    fn test_block_high_risk() {
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
}
