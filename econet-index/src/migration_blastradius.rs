// filename: econet-index/src/migration_blastradius.rs

use rusqlite::{params, Connection, Result as SqlResult};

/// Extend the EcoNet discovery spine with:
/// - blastradiuslink: normalized blast-radius metadata
/// - workloadledger: cyboquatic workload energy/carbon ledger
/// - example shard instances and links for one FOG-routing shard
///   and one FlowVac biodegradable substrate shard.
///
/// Assumptions (from existing spine):
/// - Tables repo, repofile, alnschema, alnparticle, shardinstance, knowledgeecoscore exist.
/// - Hydrological / FlowVac schemas and particles follow prior naming:
///   * HydrologicalBufferShard.v1 / HydrologicalBufferPhoenix2026v1
///   * FlowVacSubstrateShard.v1 / FlowVacSubstrateShard.v1
pub fn run_blastradius_and_ledger_migrations(conn: &Connection) -> SqlResult<()> {
    conn.execute_batch(
        r#"
        PRAGMA foreign_keys = ON;

        -- Blast-radius index: purely descriptive impact surface, no actuation.
        CREATE TABLE IF NOT EXISTS blastradiuslink (
            linkid        INTEGER PRIMARY KEY AUTOINCREMENT,
            sourcetype    TEXT NOT NULL CHECK (sourcetype IN ('REPO','SCHEMA','PARTICLE','SHARD','FILE')),
            sourceid      INTEGER NOT NULL,
            targettype    TEXT NOT NULL CHECK (targettype IN ('NODE','SHARD','MACHINE','MATERIAL','REGION')),
            targetid      TEXT NOT NULL, -- e.g. nodeid, region code, material id, machine id
            impacttype    TEXT NOT NULL, -- HYDRAULIC,ENERGY,CARBON,BIODIVERSITY,MATERIAL,DATAQUALITY,GOVERNANCE
            impactscore   REAL NOT NULL, -- 0..1 normalized fraction of corridor width touched
            vtsensitivity REAL,          -- approximate ΔVt source→target, dimensionless
            notes         TEXT
        );

        CREATE INDEX IF NOT EXISTS idx_blastradius_source
            ON blastradiuslink (sourcetype, sourceid, impacttype);

        CREATE INDEX IF NOT EXISTS idx_blastradius_target
            ON blastradiuslink (targettype, targetid, impacttype);

        -- Cyboquatic workload energy / carbon ledger.
        -- Read-only from controllers' perspective: records what was simulated / executed,
        -- actuation remains gated by Rust+ALN ecosafety kernels.
        CREATE TABLE IF NOT EXISTS workloadledger (
            ledgerid      INTEGER PRIMARY KEY AUTOINCREMENT,
            shardid       INTEGER NOT NULL REFERENCES shardinstance(shardid) ON DELETE CASCADE,
            variantid     TEXT NOT NULL, -- ALN variant / workload id
            nodeid        TEXT NOT NULL,
            channel       TEXT NOT NULL CHECK (channel IN ('energy','carbon','materials','biodiversity')),
            ereq_j        REAL NOT NULL, -- requested energy [J]
            esurplus_j    REAL NOT NULL, -- surplus available at dispatch [J]
            rcarbon       REAL,          -- normalized carbon risk coord (0..1)
            rbiodiv       REAL,          -- normalized biodiversity risk coord (0..1)
            vt_before     REAL NOT NULL, -- V(t-) before workload
            vt_after      REAL NOT NULL, -- V(t+) after workload
            decision      TEXT NOT NULL CHECK (decision IN ('ACCEPT','REJECT','REROUTE')),
            timestamputc  TEXT NOT NULL  -- ISO8601
        );

        CREATE INDEX IF NOT EXISTS idx_workload_node_time
            ON workloadledger (nodeid, timestamputc);

        CREATE INDEX IF NOT EXISTS idx_workload_shard
            ON workloadledger (shardid, channel);
        "#,
    )?;

    seed_example_blastradius_and_ledger(conn)?;
    Ok(())
}

/// Seed example data for:
/// - one cyboquatic FOG-routing hydrological buffer shard
/// - one FlowVac biodegradable substrate shard
/// - blast-radius links joining FlowVac materials to Phoenix hydrological nodes
/// - workload ledger rows capturing energy/carbon effects
fn seed_example_blastradius_and_ledger(conn: &Connection) -> SqlResult<()> {
    // 1. Ensure example repos exist (ENGINE + MATERIAL).
    conn.execute(
        r#"
        INSERT OR IGNORE INTO repo (name, githubslug, visibility, languageprimary, roleband, description, lastupdatedutc)
        VALUES
            ('EcoNet-CEIM-PhoenixWater',
             'Doctor0Evil/EcoNet-CEIM-PhoenixWater',
             'Public',
             'Rust',
             'ENGINE',
             'Cyboquatic CEIM/CPVM kernels and FOG-safe routing for Phoenix water nodes.',
             '2026-05-13T00:00:00Z'),
            ('FlowVac-Materials',
             'Doctor0Evil/FlowVac-Materials',
             'Public',
             'Rust',
             'MATERIAL',
             'Biodegradable FlowVac substrates and material-kinetics shards.',
             '2026-05-13T00:00:00Z');
        "#,
        [],
    )?;

    let phoenix_repo_id: i64 = conn.query_row(
        "SELECT repoid FROM repo WHERE name = 'EcoNet-CEIM-PhoenixWater';",
        [],
        |row| row.get(0),
    )?;
    let flowvac_repo_id: i64 = conn.query_row(
        "SELECT repoid FROM repo WHERE name = 'FlowVac-Materials';",
        [],
        |row| row.get(0),
    )?;

    // 2. Ensure example file entries exist for hydrological buffer + substrate CSV shards.
    conn.execute(
        r#"
        INSERT OR IGNORE INTO repofile
            (repoid, relpath, filename, ext, filekind, dirclass,
             sha256hex, bytessize, lastcommitsha, lastupdatedutc)
        VALUES
            (?1,
             'qpudatashards/phoenix/hydrology/HBUF_CAP-LP-01_2026Q1.csv',
             'HBUF_CAP-LP-01_2026Q1.csv',
             'csv',
             'CSV',
             'QPUDATASHARD',
             NULL,
             NULL,
             'hydro-cap-lp-01-demo',
             '2026-04-01T00:00:00Z'),
            (?2,
             'qpudatashards/materials/FlowVac/FlowVacSubstrateBatch_A1_2026.csv',
             'FlowVacSubstrateBatch_A1_2026.csv',
             'csv',
             'CSV',
             'QPUDATASHARD',
             NULL,
             NULL,
             'flowvac-batch-a1-demo',
             '2026-04-01T00:00:00Z');
        "#,
        params![phoenix_repo_id, flowvac_repo_id],
    )?;

    let hbuf_file_id: i64 = conn.query_row(
        "SELECT fileid FROM repofile
         WHERE relpath = 'qpudatashards/phoenix/hydrology/HBUF_CAP-LP-01_2026Q1.csv';",
        [],
        |row| row.get(0),
    )?;
    let flowvac_file_id: i64 = conn.query_row(
        "SELECT fileid FROM repofile
         WHERE relpath = 'qpudatashards/materials/FlowVac/FlowVacSubstrateBatch_A1_2026.csv';",
        [],
        |row| row.get(0),
    )?;

    // 3. Ensure schemas exist for hydrological buffer and FlowVac substrate.
    conn.execute(
        r#"
        INSERT OR IGNORE INTO alnschema
            (repofileid, schemaname, versiontag, title, description, category,
             spechashhex, mandatory, deprecated)
        VALUES
            (?1,
             'HydrologicalBufferShard.v1',
             'v1',
             'Phoenix hydrological buffer shard',
             'CEIM/CPVM hydrological-buffer qpudatashard for Phoenix nodes.',
             'HYDRO',
             NULL,
             0,
             0),
            (?2,
             'FlowVacSubstrateShard.v1',
             'v1',
             'FlowVac biodegradable substrate shard',
             'Material kinetics and toxicity coordinates for FlowVac substrates.',
             'FLOWVAC',
             NULL,
             0,
             0);
        "#,
        params![hbuf_file_id, flowvac_file_id],
    )?;

    let hbuf_schema_id: i64 = conn.query_row(
        "SELECT schemaid FROM alnschema WHERE schemaname = 'HydrologicalBufferShard.v1';",
        [],
        |row| row.get(0),
    )?;
    let flowvac_schema_id: i64 = conn.query_row(
        "SELECT schemaid FROM alnschema WHERE schemaname = 'FlowVacSubstrateShard.v1';",
        [],
        |row| row.get(0),
    )?;

    // 4. Ensure particles exist for hydrological buffer and substrate.
    conn.execute(
        r#"
        INSERT OR IGNORE INTO alnparticle
            (schemaid, particlename, role, versiontag, description, lyapchannel,
             haskerfields, hasriskfields, hasadmissibility)
        VALUES
            (?1,
             'HydrologicalBufferPhoenix2026v1',
             'QPUDATASHARD',
             'v1',
             'Phoenix hydrological buffer qpudatashard rows for CEIM/CPVM.',
             'hydraulics',
             1,
             1,
             1),
            (?2,
             'FlowVacSubstrateShard.v1',
             'SUBSTRATE',
             'v1',
             'FlowVac substrate batch kinetics and toxicity (rmassloss,rtox,rmicro,rPFASresid,rcarbon,rbiodiversity).',
             'materials',
             1,
             1,
             1);
        "#,
        params![hbuf_schema_id, flowvac_schema_id],
    )?;

    let hbuf_particle_id: i64 = conn.query_row(
        "SELECT particleid FROM alnparticle WHERE particlename = 'HydrologicalBufferPhoenix2026v1';",
        [],
        |row| row.get(0),
    )?;
    let flowvac_particle_id: i64 = conn.query_row(
        "SELECT particleid FROM alnparticle WHERE particlename = 'FlowVacSubstrateShard.v1';",
        [],
        |row| row.get(0),
    )?;

    // 5. Insert example shard instances (hydrological node + substrate batch).
    conn.execute(
        r#"
        INSERT INTO shardinstance
            (repofileid, particleid, nodeid, assettype, medium, region,
             tstartutc,  tendutc,   lane,
             kmetric,   emetric,   rmetric, vtmax,
             kerdeployable, evidencehex, signingdid)
        VALUES
            (?1, ?2,
             'CAP-LP-HBUF-01',
             'HydroBufferReach',
             'water',
             'Phoenix-AZ',
             '2026-01-01T00:00:00Z',
             '2026-03-31T23:59:59Z',
             'PROD',
             0.95,
             0.91,
             0.10,
             0.32,
             1,
             'ab12cd34ef56hydrocaplpdemo',
             'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'),
            (?3, ?4,
             'FLOWVAC-BATCH-A1',
             'FlowVacSubstrateBatch',
             'materials',
             'Phoenix-AZ',
             '2026-01-01T00:00:00Z',
             '2026-02-28T23:59:59Z',
             'RESEARCH',
             0.93,
             0.95,
             0.11,
             0.27,
             1,
             'cd34ef56ab12flowvacbatcha1demo',
             'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7');
        "#,
        params![hbuf_file_id, hbuf_particle_id, flowvac_file_id, flowvac_particle_id],
    )?;

    let hbuf_shard_id: i64 = conn.query_row(
        "SELECT shardid FROM shardinstance WHERE nodeid = 'CAP-LP-HBUF-01';",
        [],
        |row| row.get(0),
    )?;
    let flowvac_shard_id: i64 = conn.query_row(
        "SELECT shardid FROM shardinstance WHERE nodeid = 'FLOWVAC-BATCH-A1';",
        [],
        |row| row.get(0),
    )?;

    // 6. Insert blast-radius links.
    //
    // Example 1: FlowVac substrate shard influences CAP-LP hydrological node
    // on carbon and materials channels with small Vt sensitivity.
    conn.execute(
        r#"
        INSERT INTO blastradiuslink
            (sourcetype, sourceid, targettype, targetid,
             impacttype, impactscore, vtsensitivity, notes)
        VALUES
            ('SHARD',
             ?1,
             'NODE',
             'CAP-LP-HBUF-01',
             'CARBON',
             0.12,
             0.03,
             'FlowVac substrate batch A1 contributes modest carbon risk to CAP-LP hydrological reach.'),
            ('SHARD',
             ?1,
             'NODE',
             'CAP-LP-HBUF-01',
             'MATERIAL',
             0.18,
             0.02,
             'Biodegradable substrate alters material risk envelope for CAP-LP hydrological buffer.');
        "#,
        params![flowvac_shard_id],
    )?;

    // Example 2: EcoNet-CEIM-PhoenixWater repo influences Phoenix-AZ region hydraulics.
    conn.execute(
        r#"
        INSERT INTO blastradiuslink
            (sourcetype, sourceid, targettype, targetid,
             impacttype, impactscore, vtsensitivity, notes)
        VALUES
            ('REPO',
             ?1,
             'REGION',
             'Phoenix-AZ',
             'HYDRAULIC',
             0.65,
             0.10,
             'CEIM/CPVM cyboquatic kernels materially influence Phoenix hydraulic risk vectors.');
        "#,
        params![phoenix_repo_id],
    )?;

    // 7. Insert workload ledger rows capturing one cyboquatic routing decision
    //    and one material/energy workload effect.

    // Cyboquatic FOG routing workload at CAP-LP node.
    conn.execute(
        r#"
        INSERT INTO workloadledger
            (shardid, variantid, nodeid, channel,
             ereq_j, esurplus_j, rcarbon, rbiodiv,
             vt_before, vt_after, decision, timestamputc)
        VALUES
            (?1,
             'FOGRoute-CAP-LP-2026Q1-v1',
             'CAP-LP-HBUF-01',
             'energy',
             2.5e6,
             3.1e6,
             0.08,
             0.06,
             0.32,
             0.30,
             'ACCEPT',
             '2026-02-10T12:00:00Z');
        "#,
        params![hbuf_shard_id],
    )?;

    // FlowVac substrate deployment workload (analyzed, not necessarily actuated).
    conn.execute(
        r#"
        INSERT INTO workloadledger
            (shardid, variantid, nodeid, channel,
             ereq_j, esurplus_j, rcarbon, rbiodiv,
             vt_before, vt_after, decision, timestamputc)
        VALUES
            (?1,
             'FlowVacBatchA1-Eval-2026-v1',
             'FLOWVAC-BATCH-A1',
             'carbon',
             4.0e5,
             5.0e5,
             0.11,
             0.07,
             0.27,
             0.26,
             'ACCEPT',
             '2026-02-15T09:30:00Z');
        "#,
        params![flowvac_shard_id],
    )?;

    // 8. Score these shard schemas in knowledgeecoscore (meta-eco ledger).
    //
    // Note: scoped as SCHEMA, pointing to alnschema rows, non-actuating.
    conn.execute(
        r#"
        INSERT INTO knowledgeecoscore
            (scopetype, scoperefid, kfactor, efactor, rfactor,
             rationale, timestamputc, issuedby)
        VALUES
            ('SCHEMA',
             ?1,
             0.95,
             0.90,
             0.12,
             'HydrologicalBufferShard.v1 reuses CEIM/CPVM rx,Vt, KER grammar for Phoenix hydrological nodes.',
             '2026-05-13T00:00:00Z',
             'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'),
            ('SCHEMA',
             ?2,
             0.93,
             0.95,
             0.12,
             'FlowVacSubstrateShard.v1 hard-gates biodegradable substrates using kinetics and toxicity corridors.',
             '2026-05-13T00:00:00Z',
             'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7');
        "#,
        params![hbuf_schema_id, flowvac_schema_id],
    )?;

    Ok(())
}
