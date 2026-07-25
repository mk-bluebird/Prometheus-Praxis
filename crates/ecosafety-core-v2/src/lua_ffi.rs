// filename: crates/ecosafety-core-v2/src/lua_ffi.rs

#![forbid(unsafe_code)]

use mlua::prelude::*;
use crate::uhi_triads::{UhiObservables, UhiBands, build_uhi_triad};

pub fn attach_uhi_triads(lua: &Lua) -> LuaResult<()> {
    let func = lua.create_function(|lua_ctx, observables_table: LuaTable| {
        // Extract inputs.
        let surface_temp_c: f64 = observables_table.get("surface_temp_c")?;
        let ndvi: f64 = observables_table.get("ndvi")?;
        let canopy_fraction: Option<f64> = observables_table.get("canopy_fraction").ok();
        let air_quality_index: f64 = observables_table.get("air_quality_index")?;

        let observed = UhiObservables {
            surface_temp_c,
            ndvi,
            canopy_fraction,
            air_quality_index,
        };

        // Example bands; in practice, inject from Rust config, not Lua.
        let bands = UhiBands {
            t_min_safe_c: 30.0,
            t_max_extreme_c: 45.0,
            ndvi_min: 0.1,
            ndvi_optimal: 0.6,
            canopy_min: 0.05,
            canopy_target: 0.30,
            aqi_good: 0.0,
            aqi_hazardous: 200.0,
            w_temp: 0.5,
            w_veg: 0.3,
            w_air: 0.2,
        };

        let triad = build_uhi_triad(&observed, &bands)
            .map_err(|e| LuaError::external(e.to_string()))?;

        // Return Lua table.
        let out = lua_ctx.create_table()?;
        out.set("r_t", triad.r_t)?;
        out.set("r_c", triad.r_c)?;
        out.set("r_a", triad.r_a)?;
        out.set("r_thermal", triad.r_thermal)?;

        Ok(out)
    })?;

    lua.globals().set("build_uhi_triad_risk", func)?;
    Ok(())
}
