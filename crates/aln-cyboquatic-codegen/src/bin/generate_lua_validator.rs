// File: aln-cyboquatic-codegen/src/bin/generate_lua_validator.rs
use std::{env, fs, process};

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 3 {
        eprintln!("usage: generate_lua_validator <input.aln> <output.lua>");
        process::exit(64);
    }

    let source = fs::read_to_string(&args[1]).unwrap_or_else(|error| {
        eprintln!("cannot read {}: {}", args[1], error);
        process::exit(66);
    });
    if !source.contains("invariant energy_corridor(frame: workload_frame)") {
        eprintln!("unsupported ALN invariant set");
        process::exit(65);
    }

    let output = format!(
        "-- Generated from {}; do not edit manually.\n\
         local M = {{}}\n\
         local function bounded(v) return type(v) == \"number\" and v == v and v >= 0 and v <= 1 end\n\
         function M.validate_workload_frame(frame)\n\
           if type(frame) ~= \"table\" then return false, \"frame must be a table\" end\n\
           if type(frame.owner_did) ~= \"string\" or frame.owner_did == \"\" then return false, \"owner_did missing\" end\n\
           if type(frame.canal_node) ~= \"string\" or frame.canal_node == \"\" then return false, \"canal_node missing\" end\n\
           if type(frame.energyreqJ) ~= \"number\" or frame.energyreqJ < 0 then return false, \"energyreqJ invalid\" end\n\
           if not bounded(frame.deltaVt) or not bounded(frame.K) or not bounded(frame.E) or not bounded(frame.R) then return false, \"KER or residual invalid\" end\n\
           if not bounded(frame.fog_confidence) or not bounded(frame.eco_impact_value) then return false, \"FOG or impact invalid\" end\n\
           if frame.deltaVt > 0.35 then return false, \"deltaVt corridor breached\" end\n\
           if frame.K * frame.E <= frame.R then return false, \"KER corridor breached\" end\n\
           if frame.fog_confidence < 0.75 then return false, \"FOG confidence corridor breached\" end\n\
           if frame.eco_impact_value < 0.60 then return false, \"eco-impact corridor breached\" end\n\
           return true, nil\n\
         end\n\
         return M\n",
        args[1]
    );
    fs::write(&args[2], output).unwrap_or_else(|error| {
        eprintln!("cannot write {}: {}", args[2], error);
        process::exit(73);
    });
}
