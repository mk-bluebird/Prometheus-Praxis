# Adaptive Corridors (Prometheus‑Praxis)

This subtree collects all real, non‑fictional adaptive corridor code and tooling for eco_restoration_shard, including CAP Phoenix Hflow guards and related diagnostics.

## Scope

- City‑scale corridors only (water, thermal, waste, noise, biotic).
- Non‑actuating guards and diagnostics (RoH, Hflow, Lyapunov).
- MCP servers and catalogs that expose corridor state safely to AI agents.

All content here respects:

- Global RoH ceiling 0.30 (shared with nanoswarm and healthcare planes).
- No corridor, no build doctrine – corridors are enforced before any actuation.
- Sovereignty and neurorights invariants from Cybercore and Organichain.

## Layout

- `ppx.function.meta.v1.aln`  
  - Catalog of MCP tools and Rust functions for adaptive corridors.  
  - Each entry declares `actuationallowed`, `ecosafetyrequired`, blast radius, input/output schemas, and safety vectors.

- `ts-mcp-server/`  
  - TypeScript MCP server exposing diagnostic tools to agents.  
  - Uses `@modelcontextprotocol/sdk` to register corridor tools.  
  - Example tool: `water.hflow_guard_cap_phx.diagnostic.v1`.

- Rust corridor guards (in main crate)  
  - `eco_restoration_shard/src/water/hflow_guard_cap_phx.rs`  
    - CAP Phoenix urban flood Hflow guard.  
    - Computes `R_vel`, `R_mob`, `R_geo`, `Hflow`, `Hflow_op`, lane, reasons.  
    - Non‑actuating; bound via MCP for diagnostics only.

Additional corridors (thermal, noise, waste) should follow the same pattern:

- Rust guard module under `eco_restoration_shard/src/<domain>/`.
- Catalog entry in `ppx.function.meta.v1.aln` with clear safety bands.
- MCP server tool definition with `actuationallowed = false` by default.

## MCP Tool Safety

All tools defined here:

- Prefer `actuationallowed = false`.  
- Use `blast_radius = "local"` by default (single corridor, single window).  
- Expose explicit `safety_vector` fields:
  - Band starts and ceilings.
  - Maximum tail probabilities.
  - Lyapunov margins where applicable.

Agents may:

- Read diagnostics and safety vectors.
- Propose policies or human‑reviewable plans.

Agents may not:

- Directly actuate hardware from this server.
- Override corridor bands or governance rules.

## Integration

- Governance:  
  - Tools are indexed by `ppx.function.meta.v1` and gated by ecosafety where necessary.  
  - Diagnostic tools (`actuationallowed = false`) are normally exempt from pre‑flight gates.

- Cybercore / Organichain:  
  - Corridor decisions and RoH lanes can be anchored as Veritas events for audit.  
  - Neurorights envelopes remain non‑derogable; corridor changes that affect citizens must pass FPIC gates elsewhere.

## License

MIT OR Apache‑2.0, consistent with eco_restoration_shard and Prometheus‑Praxis.
