# Phoenix Hex-Anchor Registry and FOG-Router Wiring

## Registry Format

- `eco_restoration_shard/hex/phoenix_hex_registry.md` uses YAML front matter plus markdown body to define:
  - Global metadata (city, H3 resolution, coordinate system, governance DID).
  - A simple KER-tag-based DID governance scheme tied to `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`.
  - Per-hex entries with:
    - H3 index.
    - Center coordinates.
    - Typical LST anomaly.
    - Assigned cyboquatic node.
    - DID governance tag.

This format is easily parsed by tooling and aligns with Prometheus-Praxis eco_restoration shard conventions.

## Kotlin FOG-Router Micro-Service

- `kotlin/src/main/kotlin/org/cyboquatic/fog/FOGRouterService.kt` implements:
  - A JVM service that handles FOG routing requests (e.g., via gRPC).
  - SQL lookups against `cyboquatic_workload_telemetry` and auxiliary `fog_media_ext` tables to retrieve recent PFAS and turbidity data.
  - Staleness detection using a 15-minute threshold.
  - A Lua fallback path via LuaJ that calls `FOGRouter.classify_media`, `predicate_score`, and `suggest_route` when telemetry is stale.
  - KER-weighted confidence for each route decision:
    - High confidence (0.9) when based on recent telemetry.
    - Lower confidence (0.6) when relying on Lua fallback approximations.

Technical justification: The Phoenix hex registry format provides a clear, DID-bound hex-anchor structure for thermal recovery and cyboquatic node assignment without using disallowed hashes, while the Kotlin FOG-router micro-service integrates SQL telemetry with Lua-based fallback routing and explicit KER-weighted confidence, ensuring safe, carbon-negative routing decisions even under data staleness and constrained edge environments.
