// filename: econet_index/src/migration/cyboquatic_blastradius_spine.rs

//! Non‑actuating SQLite spine for Cyboquatic eco‑machinery inside EcoNet.
//! - Extends the existing EcoNet index with blast‑radius and energy/carbon ledgers.
//! - Adds KER/eco-impact scoring surfaces for Cyboquatic shards.
//! - Designed to be consumed by Rust, ALN, Lua, C++, and Kotlin/Android via SQLite and FFI.
//!
//! Target repo (authoritative): github.com/mk-bluebird/eco_restoration_shard
//! This module is intended to be mirrored or vendored into the EcoNet spine crate
//! that other repos depend on, keeping ecosafety semantics centralized.

use rusqlite::{params, Connection, Result as SqlResult};

/// Run all Cyboquatic‑specific migrations and seed example data.
/// Call this from your existing `run_all_migrations` in the EcoNet spine.
pub fn run_cyboquatic_spine_migrations(conn: &Connection) -> SqlResult<()> {
    conn.execute_batch(
        r#"
        PRAGMA foreign_keys = ON;

        ----------------------------------------------------------------------
        -- 1. Blast‑radius link table (non‑actuating impact surfaces)
        ----------------------------------------------------------------------
        CREATE TABLE IF NOT EXISTS blastradius_link (
            link_id       INTEGER PRIMARY KEY AUTOINCREMENT,
            source_type   TEXT NOT NULL CHECK (source_type IN ('REPO','SCHEMA','PARTICLE','SHARD','FILE')),
            source_id     INTEGER NOT NULL,
            target_type   TEXT NOT NULL CHECK (target_type IN ('NODE','SHARD','MACHINE','MATERIAL','REGION')),
            target_id     TEXT NOT NULL,   -- e.g. node id, material id, region code
            impact_type   TEXT NOT NULL,   -- HYDRAULIC,ENERGY,CARBON,BIODIVERSITY,MATERIAL,DATA_QUALITY,GOVERNANCE
            impact_score  REAL NOT NULL,   -- 0..1 fraction of corridor width influenced
            vt_sensitivity REAL,           -- approximate ∂V/∂(change) for diagnostics (dimensionless)
            notes         TEXT
        );

        CREATE INDEX IF NOT EXISTS idx_blastradius_source
            ON blastradius_link (source_type, source_id, impact_type);

        CREATE INDEX IF NOT EXISTS idx_blastradius_target
            ON blastradius_link (target_type, target_id, impact_type);

        ----------------------------------------------------------------------
        -- 2. Cyboquatic workload energy/carbon ledger (readonly diagnostics)
        ----------------------------------------------------------------------
        CREATE TABLE IF NOT EXISTS cybo_workload_ledger (
            ledger_id     INTEGER PRIMARY KEY AUTOINCREMENT,
            shard_id      INTEGER NOT NULL REFERENCES shard_instance(shard_id) ON DELETE CASCADE,
            variant_id    TEXT NOT NULL,   -- variant or ALN particle variant id
            node_id       TEXT NOT NULL,
            channel       TEXT NOT NULL CHECK (channel IN ('energy','carbon','materials','biodiversity')),
            e_req_j       REAL NOT NULL,   -- requested energy [J]
            e_surplus_j   REAL NOT NULL,   -- surplus at dispatch [J]
            r_carbon      REAL,            -- normalized carbon risk coord 0..1
            r_biodiv      REAL,            -- normalized biodiversity risk 0..1
            vt_before     REAL NOT NULL,
            vt_after      REAL NOT NULL,
            decision      TEXT NOT NULL CHECK (decision IN ('ACCEPT','REJECT','REROUTE')),
            timestamp_utc TEXT NOT NULL    -- ISO‑8601
        );

        CREATE INDEX IF NOT EXISTS idx_cybo_workload_node_time
            ON cybo_workload_ledger (node_id, timestamp_utc);

        CREATE INDEX IF NOT EXISTS idx_cybo_workload_shard
            ON cybo_workload_ledger (shard_id, channel);

        ----------------------------------------------------------------------
        -- 3. View: blast‑radius summary per shard
        ----------------------------------------------------------------------
        CREATE VIEW IF NOT EXISTS v_shard_blastradius AS
        SELECT
            s.shard_id,
            s.node_id,
            s.region,
            br.impact_type,
            SUM(br.impact_score)          AS impact_score_sum,
            AVG(COALESCE(br.vt_sensitivity, 0.0)) AS vt_sensitivity_mean,
            COUNT(*)                      AS link_count
        FROM shard_instance s
        JOIN blastradius_link br
          ON br.source_type = 'SHARD'
         AND br.source_id = s.shard_id
        GROUP BY s.shard_id, s.node_id, s.region, br.impact_type;

        ----------------------------------------------------------------------
        -- 4. View: Cyboquatic workload window summaries (energy + carbon)
        --    This is diagnostic only: it summarizes how decisions behaved
        --    against V_t and risk coordinates, but does not control anything.
        ----------------------------------------------------------------------
        CREATE VIEW IF NOT EXISTS v_cybo_workload_window AS
        SELECT
            wl.node_id,
            MIN(wl.timestamp_utc)                 AS window_start_utc,
            MAX(wl.timestamp_utc)                 AS window_end_utc,
            SUM(wl.e_req_j)                       AS total_requests_j,
            SUM(wl.e_surplus_j)                   AS total_surplus_j,
            SUM(CASE WHEN wl.decision = 'ACCEPT'  THEN wl.e_req_j ELSE 0 END) AS accepted_requests_j,
            SUM(CASE WHEN wl.decision = 'REJECT'  THEN wl.e_req_j ELSE 0 END) AS rejected_requests_j,
            SUM(CASE WHEN wl.decision = 'REROUTE' THEN wl.e_req_j ELSE 0 END) AS rerouted_requests_j,
            AVG(wl.vt_before)                     AS mean_vt_before,
            AVG(wl.vt_after)                      AS mean_vt_after,
            AVG(wl.vt_after - wl.vt_before)       AS mean_delta_vt,
            AVG(wl.r_carbon)                      AS mean_r_carbon,
            AVG(wl.r_biodiv)                      AS mean_r_biodiv,
            CAST(SUM(CASE WHEN wl.decision = 'ACCEPT' THEN 1 ELSE 0 END) AS REAL)
              / NULLIF(COUNT(*), 0)               AS accept_fraction
        FROM cybo_workload_ledger wl
        GROUP BY wl.node_id;
        "#,
    )?;

    seed_cyboquatic_examples(conn)?;
    Ok(())
}

/// Seed one Cyboquatic FOG‑routing shard, one biodegradable substrate shard,
/// and attach blast‑radius + ledger entries.
/// All IDs and strings are stable examples suitable for CI replay.
/// Non‑actuating: only metadata and diagnostics are written.
fn seed_cyboquatic_examples(conn: &Connection) -> SqlResult<()> {
    // 1. Ensure example repos exist in the EcoNet index.
    conn.execute(
        r#"
        INSERT OR IGNORE INTO repo
            (name, github_slug, visibility, language_primary, role_band, description, last_updated_utc)
        VALUES
            ('EcoNet-CEIM-PhoenixWater',
             'mk-bluebird/EcoNet',
             'Public',
             'Rust',
             'ENGINE',
             'Cyboquatic CEIM/CPVM kernels and FOG-safe routing for Phoenix water nodes.',
             '2026-05-05T00:00:00Z')
        "#,
        [],
    )?;

    conn.execute(
        r#"
        INSERT OR IGNORE INTO repo
            (name, github_slug, visibility, language_primary, role_band, description, last_updated_utc)
        VALUES
            ('eco_restoration_shard',
             'mk-bluebird/eco_restoration_shard',
             'Public',
             'Rust',
             'RESEARCH',
             'Eco-restoration research shard, biodegradable materials and Cyboquatic substrates.',
             '2026-05-05T00:00:00Z')
        "#,
        [],
    )?;

    let phoenix_repo_id: i64 = conn.query_row(
        "SELECT repo_id FROM repo WHERE name = 'EcoNet-CEIM-PhoenixWater'",
        [],
        |row| row.get(0),
    )?;

    let eco_rest_repo_id: i64 = conn.query_row(
        "SELECT repo_id FROM repo WHERE name = 'eco_restoration_shard'",
        [],
        |row| row.get(0),
    )?;

    // 2. Register ALN files representing the shards.
    conn.execute(
        r#"
        INSERT OR IGNORE INTO repo_file
            (repo_id, rel_path, filename, ext, file_kind, dir_class)
        VALUES
            (?1,
             'qpudatashards/particles/CyboquaticFogRoutingPhoenix2026v1.aln',
             'CyboquaticFogRoutingPhoenix2026v1.aln',
             'aln',
             'ALN',
             'QPUDATASHARD')
        "#,
        params![phoenix_repo_id],
    )?;

    conn.execute(
        r#"
        INSERT OR IGNORE INTO repo_file
            (repo_id, rel_path, filename, ext, file_kind, dir_class)
        VALUES
            (?1,
             'qpudatashards/particles/CyboSubstrateFlowVac2026v1.aln',
             'CyboSubstrateFlowVac2026v1.aln',
             'aln',
             'ALN',
             'QPUDATASHARD')
        "#,
        params![eco_rest_repo_id],
    )?;

    let fog_file_id: i64 = conn.query_row(
        "SELECT file_id FROM repo_file WHERE filename = 'CyboquaticFogRoutingPhoenix2026v1.aln'",
        [],
        |row| row.get(0),
    )?;

    let flowvac_file_id: i64 = conn.query_row(
        "SELECT file_id FROM repo_file WHERE filename = 'CyboSubstrateFlowVac2026v1.aln'",
        [],
        |row| row.get(0),
    )?;

    // 3. Register ALN schemas and particles.
    conn.execute(
        r#"
        INSERT OR IGNORE INTO aln_schema
            (repo_file_id, schema_name, version_tag, title, description, category, mandatory)
        VALUES
            (?1,
             'CyboquaticFogRoutingShard2026v1',
             'v1',
             'Cyboquatic FOG Routing Phoenix 2026',
             'qpudatashard for FOG-safe Cyboquatic routing in Phoenix hydraulic nodes, with V_t and KER.',
             'ROUTING',
             0)
        "#,
        params![fog_file_id],
    )?;

    conn.execute(
        r#"
        INSERT OR IGNORE INTO aln_schema
            (repo_file_id, schema_name, version_tag, title, description, category, mandatory)
        VALUES
            (?1,
             'CyboSubstrateFlowVacShard2026v1',
             'v1',
             'Biodegradable FlowVac substrate shard',
             'Material kinetics and toxicity coordinates for Cyboquatic-compatible FlowVac substrates.',
             'MATERIALS',
             0)
        "#,
        params![flowvac_file_id],
    )?;

    let fog_schema_id: i64 = conn.query_row(
        "SELECT schema_id FROM aln_schema WHERE schema_name = 'CyboquaticFogRoutingShard2026v1'",
        [],
        |row| row.get(0),
    )?;

    let flowvac_schema_id: i64 = conn.query_row(
        "SELECT schema_id FROM aln_schema WHERE schema_name = 'CyboSubstrateFlowVacShard2026v1'",
        [],
        |row| row.get(0),
    )?;

    conn.execute(
        r#"
        INSERT OR IGNORE INTO aln_particle
            (schema_id, particle_name, role, version_tag, description,
             lyap_channel, has_ker_fields, has_risk_fields, has_admissibility)
        VALUES
            (?1,
             'CyboquaticFogRoutingPhoenix2026v1',
             'QPUDATASHARD',
             'v1',
             'Cyboquatic routing state for Phoenix water nodes, with V_t residual and KER.',
             'hydraulics',
             1, 1, 1)
        "#,
        params![fog_schema_id],
    )?;

    conn.execute(
        r#"
        INSERT OR IGNORE INTO aln_particle
            (schema_id, particle_name, role, version_tag, description,
             lyap_channel, has_ker_fields, has_risk_fields, has_admissibility)
        VALUES
            (?1,
             'CyboSubstrateFlowVac2026v1',
             'SUBSTRATE',
             'v1',
             'Cyboquatic-compatible biodegradable FlowVac substrate with r_massloss, r_tox, r_micro, r_carbon, r_biodiv.',
             'materials',
             1, 1, 1)
        "#,
        params![flowvac_schema_id],
    )?;

    let fog_particle_id: i64 = conn.query_row(
        "SELECT particle_id FROM aln_particle WHERE particle_name = 'CyboquaticFogRoutingPhoenix2026v1'",
        [],
        |row| row.get(0),
    )?;

    let flowvac_particle_id: i64 = conn.query_row(
        "SELECT particle_id FROM aln_particle WHERE particle_name = 'CyboSubstrateFlowVac2026v1'",
        [],
        |row| row.get(0),
    )?;

    // 4. Insert shard instances (Cyboquatic node + substrate batch).
    let signing_did = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";

    conn.execute(
        r#"
        INSERT OR IGNORE INTO shard_instance
            (repo_file_id, particle_id, node_id, asset_type, medium, region,
             t_start_utc, t_end_utc, lane,
             k_metric, e_metric, r_metric, vt_max,
             ker_deployable, evidence_hex, signing_did)
        VALUES
            (?1, ?2,
             'PHX-CYBO-NODE-01',
             'FogRouterCluster',
             'water',
             'Phoenix-AZ',
             '2026-01-01T00:00:00Z',
             '2026-01-31T23:59:59Z',
             'EXPPROD',
             0.94, 0.91, 0.12, 0.40,
             1,
             'a1b2c3d4e5f67890',
             ?3)
        "#,
        params![fog_file_id, fog_particle_id, signing_did],
    )?;

    conn.execute(
        r#"
        INSERT OR IGNORE INTO shard_instance
            (repo_file_id, particle_id, node_id, asset_type, medium, region,
             t_start_utc, t_end_utc, lane,
             k_metric, e_metric, r_metric, vt_max,
             ker_deployable, evidence_hex, signing_did)
        VALUES
            (?1, ?2,
             'FLOWVAC-BATCH-2026-01',
             'SubstrateBatch',
             'material',
             'Lab-CI',
             '2026-01-01T00:00:00Z',
             '2026-01-01T00:00:00Z',
             'RESEARCH',
             0.93, 0.95, 0.12, 0.37,
             1,
             'f0e1d2c3b4a59687',
             ?3)
        "#,
        params![flowvac_file_id, flowvac_particle_id, signing_did],
    )?;

    let fog_shard_id: i64 = conn.query_row(
        r#"
        SELECT shard_id
        FROM shard_instance
        WHERE node_id = 'PHX-CYBO-NODE-01'
          AND asset_type = 'FogRouterCluster'
        LIMIT 1
        "#,
        [],
        |row| row.get(0),
    )?;

    let flowvac_shard_id: i64 = conn.query_row(
        r#"
        SELECT shard_id
        FROM shard_instance
        WHERE node_id = 'FLOWVAC-BATCH-2026-01'
          AND asset_type = 'SubstrateBatch'
        LIMIT 1
        "#,
        [],
        |row| row.get(0),
    )?;

    // 5. Attach blast‑radius links.
    conn.execute(
        r#"
        INSERT INTO blastradius_link
            (source_type, source_id, target_type, target_id,
             impact_type, impact_score, vt_sensitivity, notes)
        VALUES
            ('SHARD', ?1, 'NODE',    'PHX-CYBO-NODE-01', 'HYDRAULIC',
             0.40, -0.03,
             'Cyboquatic FOG routing shard influences surcharge corridors in PHX-CYBO-NODE-01.')
        "#,
        params![fog_shard_id],
    )?;

    conn.execute(
        r#"
        INSERT INTO blastradius_link
            (source_type, source_id, target_type, target_id,
             impact_type, impact_score, vt_sensitivity, notes)
        VALUES
            ('SHARD', ?1, 'NODE',    'PHX-CYBO-NODE-01', 'ENERGY',
             0.30, -0.02,
             'Routing decisions adjust per-joule utilization without increasing V_t.')
        "#,
        params![fog_shard_id],
    )?;

    conn.execute(
        r#"
        INSERT INTO blastradius_link
            (source_type, source_id, target_type, target_id,
             impact_type, impact_score, vt_sensitivity, notes)
        VALUES
            ('SHARD', ?1, 'MATERIAL', 'FLOWVAC-BATCH-2026-01', 'CARBON',
             0.25, -0.01,
             'FlowVac substrate batch is carbon-negative for this Cyboquatic corridor.')
        "#,
        params![flowvac_shard_id],
    )?;

    // 6. Seed Cyboquatic workload ledger with Lyapunov‑safe decisions.
    conn.execute(
        r#"
        INSERT INTO cybo_workload_ledger
            (shard_id, variant_id, node_id, channel,
             e_req_j, e_surplus_j, r_carbon, r_biodiv,
             vt_before, vt_after, decision, timestamp_utc)
        VALUES
            (?1, 'CyboVariant-42', 'PHX-CYBO-NODE-01', 'energy',
             500.0, 5000.0, 0.20, 0.10,
             0.40, 0.395, 'ACCEPT', '2026-01-15T12:00:00Z')
        "#,
        params![fog_shard_id],
    )?;

    conn.execute(
        r#"
        INSERT INTO cybo_workload_ledger
            (shard_id, variant_id, node_id, channel,
             e_req_j, e_surplus_j, r_carbon, r_biodiv,
             vt_before, vt_after, decision, timestamp_utc)
        VALUES
            (?1, 'CyboVariant-99', 'PHX-CYBO-NODE-01', 'carbon',
             200.0, 4800.0, 0.18, 0.09,
             0.395, 0.392, 'ACCEPT', '2026-01-15T12:05:00Z')
        "#,
        params![fog_shard_id],
    )?;

    Ok(())
}
