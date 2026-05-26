// filename: econet-index/src/migration_blastradius.rs

use rusqlite::{params, Connection, Result as SqlResult};

pub fn run_blastradius_and_ledger_migrations(conn: &Connection) -> SqlResult<()> {
    conn.execute_batch(
        r#"
        PRAGMA foreign_keys = ON;

        CREATE TABLE IF NOT EXISTS blastradiuslink (
            linkid        INTEGER PRIMARY KEY AUTOINCREMENT,
            sourcetype    TEXT NOT NULL CHECK (sourcetype IN ('REPO','SCHEMA','PARTICLE','SHARD','FILE')),
            sourceid      INTEGER NOT NULL,
            targettype    TEXT NOT NULL CHECK (targettype IN ('NODE','SHARD','MACHINE','MATERIAL','REGION')),
            targetid      TEXT NOT NULL,
            impacttype    TEXT NOT NULL CHECK (
                               impacttype IN (
                                   'HYDRAULIC',
                                   'ENERGY',
                                   'CARBON',
                                   'BIODIVERSITY',
                                   'MATERIAL',
                                   'DATAQUALITY',
                                   'GOVERNANCE'
                               )
                           ),
            impactscore   REAL NOT NULL,
            vtsensitivity REAL,
            notes         TEXT
        );

        CREATE INDEX IF NOT EXISTS idx_blastradius_source
            ON blastradiuslink (sourcetype, sourceid, impacttype);

        CREATE INDEX IF NOT EXISTS idx_blastradius_target
            ON blastradiuslink (targettype, targetid, impacttype);

        CREATE TABLE IF NOT EXISTS workloadledger (
            ledgerid      INTEGER PRIMARY KEY AUTOINCREMENT,
            shardid       INTEGER NOT NULL REFERENCES shardinstance(shardid) ON DELETE CASCADE,
            variantid     TEXT NOT NULL,
            nodeid        TEXT NOT NULL,
            channel       TEXT NOT NULL CHECK (channel IN ('energy','carbon','materials','biodiversity')),
            ereqj         REAL NOT NULL,
            esurplusj     REAL NOT NULL,
            rcarbon       REAL,
            rbiodiv       REAL,
            vtbefore      REAL NOT NULL,
            vtafter       REAL NOT NULL,
            decision      TEXT NOT NULL CHECK (decision IN ('ACCEPT','REJECT','REROUTE')),
            timestamputc  TEXT NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_workload_node_time
            ON workloadledger (nodeid, timestamputc);

        CREATE INDEX IF NOT EXISTS idx_workload_shard_channel
            ON workloadledger (shardid, channel);
        "#,
    )?;

    seed_example_blastradius_and_ledger(conn)?;
    Ok(())
}

fn seed_example_blastradius_and_ledger(conn: &Connection) -> SqlResult<()> {
    conn.execute(
        r#"
        INSERT OR IGNORE INTO repo (
            name,
            githubslug,
            visibility,
            languageprimary,
            roleband,
            description,
            lastupdatedutc
        ) VALUES
            (
                'EcoNet-CEIM-PhoenixWater',
                'mk-bluebird/EcoNet-CEIM-PhoenixWater',
                'Public',
                'Rust',
                'ENGINE',
                'Cyboquatic CEIM/CPVM kernels and FOG-safe routing for Phoenix water nodes.',
                '2026-05-13T00:00:00Z'
            ),
            (
                'FlowVac-Materials',
                'mk-bluebird/FlowVac-Materials',
                'Public',
                'Rust',
                'MATERIAL',
                'Biodegradable FlowVac substrates and material-kinetics shards.',
                '2026-05-13T00:00:00Z'
            );
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

    conn.execute(
        r#"
        INSERT OR IGNORE INTO repofile (
            repoid,
            relpath,
            filename,
            extension,
            dirclass,
            sha256hex,
            bytessize,
            lastcommitsha,
            lastupdatedutc
        ) VALUES
            (
                ?1,
                'qpudatashards/phoenix/hydrology/',
                'HBUF_CAP-LP-01_2026Q1.csv',
                'csv',
                'QPUDATASHARD',
                '0000000000000000000000000000000000000000000000000000000000000000',
                0,
                'hydro-cap-lp-01-demo',
                '2026-04-01T00:00:00Z'
            ),
            (
                ?2,
                'qpudatashards/materials/FlowVac/',
                'FlowVacSubstrateBatch_A1_2026.csv',
                'csv',
                'QPUDATASHARD',
                '0000000000000000000000000000000000000000000000000000000000000000',
                0,
                'flowvac-batch-a1-demo',
                '2026-04-01T00:00:00Z'
            );
        "#,
        params![phoenix_repo_id, flowvac_repo_id],
    )?;

    let hbuf_file_id: i64 = conn.query_row(
        r#"
        SELECT fileid
        FROM repofile
        WHERE repoid = ?1
          AND filename = 'HBUF_CAP-LP-01_2026Q1.csv';
        "#,
        params![phoenix_repo_id],
        |row| row.get(0),
    )?;
    let flowvac_file_id: i64 = conn.query_row(
        r#"
        SELECT fileid
        FROM repofile
        WHERE repoid = ?1
          AND filename = 'FlowVacSubstrateBatch_A1_2026.csv';
        "#,
        params![flowvac_repo_id],
        |row| row.get(0),
    )?;

    conn.execute(
        r#"
        INSERT OR IGNORE INTO alnschema (
            fileid,
            schemaname,
            versiontag,
            category,
            title,
            description,
            spechashhex,
            mandatory,
            deprecated
        ) VALUES
            (
                ?1,
                'HydrologicalBufferShard.v1',
                'v1',
                'HYDRO',
                'Phoenix hydrological buffer shard',
                'CEIM/CPVM hydrological-buffer qpudatashard for Phoenix nodes.',
                '0000000000000000000000000000000000000000000000000000000000000000',
                0,
                0
            ),
            (
                ?2,
                'FlowVacSubstrateShard.v1',
                'v1',
                'FLOWVAC',
                'FlowVac biodegradable substrate shard',
                'Material kinetics and toxicity coordinates for FlowVac substrates.',
                '0000000000000000000000000000000000000000000000000000000000000000',
                0,
                0
            );
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

    conn.execute(
        r#"
        INSERT OR IGNORE INTO alnparticle (
            schemaid,
            particlename,
            role,
            versiontag,
            description,
            lyapchannel,
            haskerfields,
            hasriskfields,
            hasadmissibility
        ) VALUES
            (
                ?1,
                'HydrologicalBufferPhoenix2026v1',
                'QPUDATASHARD',
                'v1',
                'Phoenix hydrological buffer qpudatashard rows for CEIM/CPVM.',
                'hydraulics',
                1,
                1,
                1
            ),
            (
                ?2,
                'FlowVacSubstrateShard.v1',
                'SUBSTRATE',
                'v1',
                'FlowVac substrate batch kinetics and toxicity (rmassloss,rtox,rmicro,rPFASresid,rcarbon,rbiodiversity).',
                'materials',
                1,
                1,
                1
            );
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

    conn.execute(
        r#"
        INSERT OR IGNORE INTO shardinstance (
            fileid,
            particleid,
            nodeid,
            assettype,
            medium,
            region,
            lane,
            kmetric,
            emetric,
            rmetric,
            vtmax,
            kerdeployable,
            evidencehex,
            signingdid,
            timestamputc
        ) VALUES
            (
                ?1,
                ?2,
                'CAP-LP-HBUF-01',
                'HydroBufferReach',
                'water',
                'Phoenix-AZ',
                'PROD',
                0.95,
                0.91,
                0.10,
                0.32,
                1,
                'ab12cd34ef56hydrocaplpdemo00000000000000000000000000000000',
                'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
                '2026-03-31T23:59:59Z'
            ),
            (
                ?3,
                ?4,
                'FLOWVAC-BATCH-A1',
                'FlowVacSubstrateBatch',
                'materials',
                'Phoenix-AZ',
                'RESEARCH',
                0.93,
                0.95,
                0.11,
                0.27,
                1,
                'cd34ef56ab12flowvacbatcha1demo000000000000000000000000000000',
                'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
                '2026-02-28T23:59:59Z'
            );
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

    conn.execute(
        r#"
        INSERT INTO blastradiuslink (
            sourcetype,
            sourceid,
            targettype,
            targetid,
            impacttype,
            impactscore,
            vtsensitivity,
            notes
        ) VALUES
            (
                'SHARD',
                ?1,
                'NODE',
                'CAP-LP-HBUF-01',
                'CARBON',
                0.12,
                0.03,
                'FlowVac substrate batch A1 contributes modest carbon risk to CAP-LP hydrological reach.'
            ),
            (
                'SHARD',
                ?1,
                'NODE',
                'CAP-LP-HBUF-01',
                'MATERIAL',
                0.18,
                0.02,
                'Biodegradable substrate alters material risk envelope for CAP-LP hydrological buffer.'
            );
        "#,
        params![flowvac_shard_id],
    )?;

    conn.execute(
        r#"
        INSERT INTO blastradiuslink (
            sourcetype,
            sourceid,
            targettype,
            targetid,
            impacttype,
            impactscore,
            vtsensitivity,
            notes
        ) VALUES
            (
                'REPO',
                ?1,
                'REGION',
                'Phoenix-AZ',
                'HYDRAULIC',
                0.65,
                0.10,
                'EcoNet-CEIM-PhoenixWater kernels influence Phoenix hydraulic risk vectors.'
            );
        "#,
        params![phoenix_repo_id],
    )?;

    conn.execute(
        r#"
        INSERT INTO workloadledger (
            shardid,
            variantid,
            nodeid,
            channel,
            ereqj,
            esurplusj,
            rcarbon,
            rbiodiv,
            vtbefore,
            vtafter,
            decision,
            timestamputc
        ) VALUES
            (
                ?1,
                'FOGRoute-CAP-LP-2026Q1-v1',
                'CAP-LP-HBUF-01',
                'energy',
                2500000.0,
                3100000.0,
                0.08,
                0.06,
                0.32,
                0.30,
                'ACCEPT',
                '2026-02-10T12:00:00Z'
            );
        "#,
        params![hbuf_shard_id],
    )?;

    conn.execute(
        r#"
        INSERT INTO workloadledger (
            shardid,
            variantid,
            nodeid,
            channel,
            ereqj,
            esurplusj,
            rcarbon,
            rbiodiv,
            vtbefore,
            vtafter,
            decision,
            timestamputc
        ) VALUES
            (
                ?1,
                'FlowVacBatchA1-Eval-2026-v1',
                'FLOWVAC-BATCH-A1',
                'carbon',
                400000.0,
                500000.0,
                0.11,
                0.07,
                0.27,
                0.26,
                'ACCEPT',
                '2026-02-15T09:30:00Z'
            );
        "#,
        params![flowvac_shard_id],
    )?;

    conn.execute(
        r#"
        INSERT OR IGNORE INTO knowledgeecoscore (
            scopetype,
            scoperefid,
            kfactor,
            efactor,
            rfactor,
            rationale,
            timestamputc,
            issuedby
        ) VALUES
            (
                'SCHEMA',
                ?1,
                0.95,
                0.90,
                0.12,
                'HydrologicalBufferShard.v1 reuses CEIM/CPVM risk vector and Lyapunov grammar for Phoenix nodes.',
                '2026-05-13T00:00:00Z',
                'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
            ),
            (
                'SCHEMA',
                ?2,
                0.93,
                0.95,
                0.12,
                'FlowVacSubstrateShard.v1 hard-gates biodegradable substrates using kinetics and toxicity corridors.',
                '2026-05-13T00:00:00Z',
                'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
            );
        "#,
        params![hbuf_schema_id, flowvac_schema_id],
    )?;

    Ok(())
}
