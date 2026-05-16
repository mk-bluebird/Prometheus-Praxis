// filename: econet-index/src/migration_cyboquatic.rs

// Purpose:
// - Extend the EcoNet SQLite discovery spine with cyboquatic-specific,
//   ecologically-restorative blast-radius and energy/cost tracking.
// - Provide a production-grade migration plus example data for:
//   * One cyboquatic FOG-routing workload (hydraulic+energy+carbon).
//   * One biodegradable FlowVac substrate workload (materials+carbon+biodiversity).
// - Keep everything strictly non-actuating: this module only creates and
//   populates metadata tables used for analysis and planning.

use rusqlite::{params, Connection, Result as SqlResult};

/// Run all cyboquatic-focused migrations on an existing EcoNet spine DB.
///
/// Assumes the core schema from previous EcoNet designs is already present:
/// repo, repofile, alnschema, alnparticle, alnfield,
/// corridordefinition, shardinstance, knowledgeecoscore.
///
/// Safe to call multiple times; all CREATEs are IF NOT EXISTS and example
/// inserts are idempotent where possible.
pub fn run_cyboquatic_migrations(conn: &Connection) -> SqlResult<()> {
    conn.execute_batch(
        r#"
        PRAGMA foreign_keys = ON;

        -- 1. Blast-radius link metadata
        -- Describes how a repo/schema/particle/shard/file influences
        -- physical or ecological targets (nodes, machines, materials, regions).
        -- Purely descriptive, no actuation.

        CREATE TABLE IF NOT EXISTS blastradiuslink (
            linkid       INTEGER PRIMARY KEY AUTOINCREMENT,
            sourcetype   TEXT NOT NULL CHECK (sourcetype IN ('REPO','SCHEMA','PARTICLE','SHARD','FILE')),
            sourceid     INTEGER NOT NULL,
            targettype   TEXT NOT NULL CHECK (targettype IN ('NODE','SHARD','MACHINE','MATERIAL','REGION')),
            targetid     TEXT NOT NULL,
            impacttype   TEXT NOT NULL, -- HYDRAULIC,ENERGY,CARBON,BIODIVERSITY,MATERIAL,DATAQUALITY,GOVERNANCE
            impactscore  REAL NOT NULL, -- 0..1 fraction of corridor width or relative influence
            vtsensitivity REAL,         -- approximate ∂V/∂source, dimensionless
            notes        TEXT
        );

        CREATE INDEX IF NOT EXISTS idx_blastradius_source
            ON blastradiuslink (sourcetype, sourceid, impacttype);

        CREATE INDEX IF NOT EXISTS idx_blastradius_target
            ON blastradiuslink (targettype, targetid, impacttype);

        -- 2. Workload energy/carbon/biodiversity ledger
        -- Tracks cyboquatic workloads as they are simulated or executed
        -- under existing ecosafety gates. This is a read-only diagnostic
        -- surface for agents; controllers write rows, but actuation is fenced
        -- by Rust+ALN Lyapunov kernels outside of SQLite.

        CREATE TABLE IF NOT EXISTS workloadledger (
            ledgerid      INTEGER PRIMARY KEY AUTOINCREMENT,
            shardid       INTEGER NOT NULL REFERENCES shardinstance(shardid) ON DELETE CASCADE,
            variantid     TEXT NOT NULL, -- variant or planning id
            nodeid        TEXT NOT NULL, -- e.g. 'GILA-RCH-HBUF-07'
            channel       TEXT NOT NULL CHECK (channel IN ('energy','carbon','materials','biodiversity')),
            ereq_j        REAL NOT NULL, -- requested energy in joules
            esurplus_j    REAL NOT NULL, -- local surplus at decision time
            rcarbon       REAL,          -- carbon risk coordinate (0..1)
            rbiodiv       REAL,          -- biodiversity risk coordinate (0..1)
            vt_before     REAL NOT NULL, -- Lyapunov residual before workload
            vt_after      REAL NOT NULL, -- Lyapunov residual after workload
            decision      TEXT NOT NULL CHECK (decision IN ('ACCEPT','REJECT','REROUTE')),
            timestamputc  TEXT NOT NULL  -- ISO8601
        );

        CREATE INDEX IF NOT EXISTS idx_workload_node_time
            ON workloadledger (nodeid, timestamputc);

        CREATE INDEX IF NOT EXISTS idx_workload_shard
            ON workloadledger (shardid, channel);

        -- 3. Cyboquatic-oriented views for agents
        -- These are convenience surfaces that join core shardinstance
        -- with blast radius and eco-impact metadata.

        CREATE VIEW IF NOT EXISTS v_cyboquatic_node_blastradius AS
        SELECT
            s.shardid,
            s.nodeid,
            s.region,
            s.medium,
            s.lane,
            s.kmetric,
            s.emetric,
            s.rmetric,
            s.vtmax,
            b.impacttype,
            b.impactscore,
            b.vtsensitivity,
            b.notes
        FROM shardinstance AS s
        JOIN blastradiuslink AS b
          ON b.sourcetype = 'SHARD'
         AND b.sourceid   = s.shardid;

        CREATE VIEW IF NOT EXISTS v_cyboquatic_energy_carbon AS
        SELECT
            w.ledgerid,
            w.shardid,
            w.variantid,
            w.nodeid,
            w.channel,
            w.ereq_j,
            w.esurplus_j,
            w.rcarbon,
            w.rbiodiv,
            w.vt_before,
            w.vt_after,
            w.decision,
            w.timestamputc
        FROM workloadledger AS w
        WHERE w.channel IN ('energy','carbon');
        "#,
    )?;

    seed_example_cyboquatic_data(conn)?;
    Ok(())
}

/// Insert example cyboquatic workloads and blast-radius links.
///
/// This function is safe to run repeatedly; lookups are based on names where
/// possible and avoid duplicate inserts via INSERT OR IGNORE.
fn seed_example_cyboquatic_data(conn: &Connection) -> SqlResult<()> {
    // 1. Ensure example repos exist in `repo` table.

    conn.execute(
        r#"
        INSERT OR IGNORE INTO repo (name, githubslug, visibility, languageprimary, roleband, description, lastupdatedutc)
        VALUES (
            'EcoNet-CEIM-PhoenixWater',
            'Doctor0Evil/EcoNet-CEIM-PhoenixWater',
            'Public',
            'Rust',
            'ENGINE',
            'Cyboquatic CEIM/CPVM kernels and FOG-safe routing for Phoenix/Gila water nodes.',
            '2026-05-15T00:00:00Z'
        );
        "#,
        [],
    )?;

    conn.execute(
        r#"
        INSERT OR IGNORE INTO repo (name, githubslug, visibility, languageprimary, roleband, description, lastupdatedutc)
        VALUES (
            'BugsLife',
            'Doctor0Evil/BugsLife',
            'Public',
            'Rust',
            'MATERIAL',
            'Eco-safe pest-control and biodegradable substrate kinetics under ecosafety corridors.',
            '2026-05-15T00:00:00Z'
        );
        "#,
        [],
    )?;

    // Look up repo ids.
    let phoenix_repo_id: i64 = conn.query_row(
        "SELECT repoid FROM repo WHERE name = 'EcoNet-CEIM-PhoenixWater';",
        [],
        |row| row.get(0),
    )?;
    let bugslife_repo_id: i64 = conn.query_row(
        "SELECT repoid FROM repo WHERE name = 'BugsLife';",
        [],
        |row| row.get(0),
    )?;

    // 2. Ensure example files exist in `repofile`.

    // Cyboquatic FOG-routing shard (hydrological buffer).
    conn.execute(
        r#"
        INSERT OR IGNORE INTO repofile (
            repoid, relpath, filename, ext, filekind, dirclass,
            sha256hex, bytessize, lastcommitsha, lastupdatedutc
        )
        VALUES (
            ?1,
            'qpudatashards/hydrology/Gila/HydrologicalBufferPhoenix2026v1.csv',
            'HydrologicalBufferPhoenix2026v1.csv',
            'csv',
            'CSV',
            'QPUDATASHARD',
            NULL,
            NULL,
            NULL,
            '2026-05-15T00:00:00Z'
        );
        "#,
        params![phoenix_repo_id],
    )?;

    // FlowVac substrate shard for biodegradable media.
    conn.execute(
        r#"
        INSERT OR IGNORE INTO repofile (
            repoid, relpath, filename, ext, filekind, dirclass,
            sha256hex, bytessize, lastcommitsha, lastupdatedutc
        )
        VALUES (
            ?1,
            'qpudatashards/materials/FlowVac/FlowVacSubstrateShard.v1.csv',
            'FlowVacSubstrateShard.v1.csv',
            'csv',
            'CSV',
            'QPUDATASHARD',
            NULL,
            NULL,
            NULL,
            '2026-05-15T00:00:00Z'
        );
        "#,
        params![bugslife_repo_id],
    )?;

    let hyd_buf_file_id: i64 = conn.query_row(
        r#"
        SELECT fileid FROM repofile
        WHERE repoid = ?1
          AND filename = 'HydrologicalBufferPhoenix2026v1.csv';
        "#,
        params![phoenix_repo_id],
        |row| row.get(0),
    )?;

    let flowvac_file_id: i64 = conn.query_row(
        r#"
        SELECT fileid FROM repofile
        WHERE repoid = ?1
          AND filename = 'FlowVacSubstrateShard.v1.csv';
        "#,
        params![bugslife_repo_id],
        |row| row.get(0),
    )?;

    // 3. Ensure schemas and particles exist in ALN registry.

    // Hydrological buffer schema.
    conn.execute(
        r#"
        INSERT OR IGNORE INTO alnschema (
            repofileid, schemaname, versiontag, title, description, category,
            spechashhex, mandatory, deprecated
        )
        VALUES (
            ?1,
            'HydrologicalBufferShard.v1',
            'v1',
            'Hydrological buffer shard for Phoenix/Gila nodes',
            'QPUDATASHARD schema for hydrological buffer risk coordinates (rFOG, rTDS, rEcoli, rPFAS, rcarbon, rbiodiversity).',
            'HYDRO',
            NULL,
            0,
            0
        );
        "#,
        params![hyd_buf_file_id],
    )?;

    // FlowVac substrate schema.
    conn.execute(
        r#"
        INSERT OR IGNORE INTO alnschema (
            repofileid, schemaname, versiontag, title, description, category,
            spechashhex, mandatory, deprecated
        )
        VALUES (
            ?1,
            'FlowVacSubstrateShard.v1',
            'v1',
            'FlowVac biodegradable substrate shard',
            'Substrate kinetics and toxicity coordinates for biodegradable FlowVac media.',
            'FLOWVAC',
            NULL,
            0,
            0
        );
        "#,
        params![flowvac_file_id],
    )?;

    let hydro_schema_id: i64 = conn.query_row(
        "SELECT schemaid FROM alnschema WHERE schemaname = 'HydrologicalBufferShard.v1';",
        [],
        |row| row.get(0),
    )?;
    let flowvac_schema_id: i64 = conn.query_row(
        "SELECT schemaid FROM alnschema WHERE schemaname = 'FlowVacSubstrateShard.v1';",
        [],
        |row| row.get(0),
    )?;

    // Particles.
    conn.execute(
        r#"
        INSERT OR IGNORE INTO alnparticle (
            schemaid, particlename, role, versiontag, description,
            lyapchannel, haskerfields, hasriskfields, hasadmissibility
        )
        VALUES (
            ?1,
            'HydrologicalBufferPhoenix2026v1',
            'QPUDATASHARD',
            'v1',
            'Phoenix/Gila hydrological buffer shard rows for CEIM/CPVM hydrology.',
            'hydraulics',
            1,
            1,
            1
        );
        "#,
        params![hydro_schema_id],
    )?;

    conn.execute(
        r#"
        INSERT OR IGNORE INTO alnparticle (
            schemaid, particlename, role, versiontag, description,
            lyapchannel, haskerfields, hasriskfields, hasadmissibility
        )
        VALUES (
            ?1,
            'FlowVacSubstrateShard.v1',
            'SUBSTRATE',
            'v1',
            'FlowVac substrate batch coordinates (rmassloss, rtox, rmicro, rPFASresid, rcarbon, rbiodiversity).',
            'materials',
            1,
            1,
            1
        );
        "#,
        params![flowvac_schema_id],
    )?;

    let hydro_particle_id: i64 = conn.query_row(
        "SELECT particleid FROM alnparticle WHERE particlename = 'HydrologicalBufferPhoenix2026v1';",
        [],
        |row| row.get(0),
    )?;
    let flowvac_particle_id: i64 = conn.query_row(
        "SELECT particleid FROM alnparticle WHERE particlename = 'FlowVacSubstrateShard.v1';",
        [],
        |row| row.get(0),
    )?;

    // 4. Insert example shardinstance rows.

    // Example hydrological buffer shard (cyboquatic FOG/CEIM workload context).
    conn.execute(
        r#"
        INSERT INTO shardinstance (
            repofileid, particleid, nodeid, assettype, medium, region,
            tstartutc, tendutc, lane,
            kmetric, emetric, rmetric, vtmax,
            kerdeployable, evidencehex, signingdid
        )
        VALUES (
            ?1, ?2,
            'GILA-RCH-HBUF-07',
            'CyboquaticFOGCluster',
            'water',
            'Phoenix-AZ',
            '2026-05-01T00:00:00Z',
            '2026-05-01T01:00:00Z',
            'EXPPROD',
            0.95,
            0.91,
            0.11,
            0.62,
            1,
            '0xhydro_gila_hbuf_07_v1',
            'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
        );
        "#,
        params![hyd_buf_file_id, hydro_particle_id],
    )?;

    let hydro_shard_id: i64 = conn.query_row(
        r#"
        SELECT shardid FROM shardinstance
        WHERE repofileid = ?1
          AND nodeid = 'GILA-RCH-HBUF-07'
        ORDER BY shardid DESC
        LIMIT 1;
        "#,
        params![hyd_buf_file_id],
        |row| row.get(0),
    )?;

    // Example FlowVac biodegradable substrate shard.
    conn.execute(
        r#"
        INSERT INTO shardinstance (
            repofileid, particleid, nodeid, assettype, medium, region,
            tstartutc, tendutc, lane,
            kmetric, emetric, rmetric, vtmax,
            kerdeployable, evidencehex, signingdid
        )
        VALUES (
            ?1, ?2,
            'FLOWVAC-MAT-BATCH-2026-05-01',
            'FlowVacSubstrateMixer',
            'bio',
            'Phoenix-AZ',
            '2026-05-01T00:00:00Z',
            '2026-05-01T12:00:00Z',
            'RESEARCH',
            0.93,
            0.95,
            0.12,
            0.48,
            1,
            '0xflowvac_batch_2026_05_01_v1',
            'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
        );
        "#,
        params![flowvac_file_id, flowvac_particle_id],
    )?;

    let flowvac_shard_id: i64 = conn.query_row(
        r#"
        SELECT shardid FROM shardinstance
        WHERE repofileid = ?1
          AND nodeid = 'FLOWVAC-MAT-BATCH-2026-05-01'
        ORDER BY shardid DESC
        LIMIT 1;
        "#,
        params![flowvac_file_id],
        |row| row.get(0),
    )?;

    // 5. Blast-radius links for these shards.

    conn.execute(
        r#"
        INSERT INTO blastradiuslink (
            sourcetype, sourceid, targettype, targetid,
            impacttype, impactscore, vtsensitivity, notes
        )
        VALUES (
            'SHARD',
            ?1,
            'NODE',
            'GILA-RCH-HBUF-07',
            'HYDRAULIC',
            0.65,
            0.08,
            'Hydrological buffer shard influences hydraulic risk and Lyapunov residual for Gila reach 07.'
        );
        "#,
        params![hydro_shard_id],
    )?;

    conn.execute(
        r#"
        INSERT INTO blastradiuslink (
            sourcetype, sourceid, targettype, targetid,
            impacttype, impactscore, vtsensitivity, notes
        )
        VALUES (
            'SHARD',
            ?1,
            'MATERIAL',
            'FLOWVAC-SUBSTRATE-FAMILY-2026V1',
            'MATERIAL',
            0.72,
            0.05,
            'FlowVac biodegradable substrate batch affects material risk corridors for substrate family 2026v1.'
        );
        "#,
        params![flowvac_shard_id],
    )?;

    conn.execute(
        r#"
        INSERT INTO blastradiuslink (
            sourcetype, sourceid, targettype, targetid,
            impacttype, impactscore, vtsensitivity, notes
        )
        VALUES (
            'SHARD',
            ?1,
            'REGION',
            'Phoenix-AZ',
            'CARBON',
            0.34,
            0.03,
            'FlowVac biodegradable substrate batch reduces effective carbon risk for Phoenix-AZ materials channel.'
        );
        "#,
        params![flowvac_shard_id],
    )?;

    // 6. Workload ledger entries linking energy+carbon dynamics to these shards.

    // Cyboquatic FOG routing decision at a hydrological node.
    conn.execute(
        r#"
        INSERT INTO workloadledger (
            shardid,
            variantid,
            nodeid,
            channel,
            ereq_j,
            esurplus_j,
            rcarbon,
            rbiodiv,
            vt_before,
            vt_after,
            decision,
            timestamputc
        )
        VALUES (
            ?1,
            'CYBOFOG-GILA-RCH-HBUF-07-2026-05-01T00:30Z',
            'GILA-RCH-HBUF-07',
            'energy',
            3.5e5,
            5.0e5,
            0.18,
            0.21,
            0.62,
            0.60,
            'ACCEPT',
            '2026-05-01T00:30:00Z'
        );
        "#,
        params![hydro_shard_id],
    )?;

    // FlowVac substrate simulation for materials+carbon+biodiversity.
    conn.execute(
        r#"
        INSERT INTO workloadledger (
            shardid,
            variantid,
            nodeid,
            channel,
            ereq_j,
            esurplus_j,
            rcarbon,
            rbiodiv,
            vt_before,
            vt_after,
            decision,
            timestamputc
        )
        VALUES (
            ?1,
            'FLOWVAC-SUBSTRATE-2026-05-01-TEST-A',
            'FLOWVAC-MAT-BATCH-2026-05-01',
            'materials',
            1.2e4,
            2.0e4,
            0.14,
            0.19,
            0.48,
            0.46,
            'ACCEPT',
            '2026-05-01T06:00:00Z'
        );
        "#,
        params![flowvac_shard_id],
    )?;

    Ok(())
}
