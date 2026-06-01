// filename: econet-index/src/migration_blastradius.rs
// Purpose: Apply blast-radius and workload ledger schema plus seed
//          example cyboquatic and FlowVac records. Non-actuating.

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
            impacttype    TEXT NOT NULL,
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
            variantid     TEXT    NOT NULL,
            nodeid        TEXT    NOT NULL,
            channel       TEXT    NOT NULL CHECK (channel IN ('energy','carbon','materials','biodiversity')),
            ereq_j        REAL    NOT NULL,
            esurplus_j    REAL    NOT NULL,
            rcarbon       REAL,
            rbiodiv       REAL,
            vt_before     REAL    NOT NULL,
            vt_after      REAL    NOT NULL,
            decision      TEXT    NOT NULL CHECK (decision IN ('ACCEPT','REJECT','REROUTE')),
            timestamputc  TEXT    NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_workload_node_time
            ON workloadledger (nodeid, timestamputc);

        CREATE INDEX IF NOT EXISTS idx_workload_shard
            ON workloadledger (shardid, channel);

        CREATE VIEW IF NOT EXISTS v_shard_blastradius AS
        SELECT
            s.shardid,
            s.nodeid,
            s.region,
            s.lane,
            s.kmetric,
            s.emetric,
            s.rmetric,
            s.vtmax,
            b.impacttype,
            SUM(b.impactscore)   AS impactscore_sum,
            MAX(b.vtsensitivity) AS vtsensitivity_max
        FROM shardinstance AS s
        LEFT JOIN blastradiuslink AS b
          ON b.sourcetype = 'SHARD'
         AND b.sourceid   = s.shardid
        GROUP BY
            s.shardid,
            s.nodeid,
            s.region,
            s.lane,
            s.kmetric,
            s.emetric,
            s.rmetric,
            s.vtmax,
            b.impacttype;

        CREATE VIEW IF NOT EXISTS v_lane_safe_carbon_negative AS
        SELECT
            s.shardid,
            s.nodeid,
            s.region,
            s.lane,
            s.kmetric,
            s.emetric,
            s.rmetric,
            s.vtmax,
            w.variantid,
            w.ereq_j,
            w.esurplus_j,
            w.rcarbon,
            w.rbiodiv,
            w.vt_before,
            w.vt_after,
            w.decision
        FROM shardinstance AS s
        JOIN workloadledger AS w
          ON w.shardid = s.shardid
        WHERE
            s.kerdeployable = 1
            AND s.lane IN ('RESEARCH','EXPPROD','PROD')
            AND w.decision = 'ACCEPT'
            AND w.rcarbon IS NOT NULL
            AND w.rcarbon <= 0.13
            AND w.vt_after <= w.vt_before;
        "#,
    )?;

    seed_example_blastradius(conn)?;
    Ok(())
}

fn seed_example_blastradius(conn: &Connection) -> SqlResult<()> {
    // Example: find one Phoenix hydrological buffer shard and one FlowVac substrate shard.
    let hydro_shardid: i64 = conn.query_row(
        r#"
        SELECT s.shardid
        FROM shardinstance AS s
        JOIN alnparticle AS p ON p.particleid = s.particleid
        WHERE p.particlename = 'HydrologicalBufferPhoenix2026v1'
        ORDER BY s.tstartutc ASC
        LIMIT 1;
        "#,
        [],
        |row| row.get(0),
    )?;

    let flowvac_shardid: i64 = conn.query_row(
        r#"
        SELECT s.shardid
        FROM shardinstance AS s
        JOIN alnparticle AS p ON p.particleid = s.particleid
        WHERE p.particlename = 'FlowVacSubstrateShard.v1'
        ORDER BY s.tstartutc ASC
        LIMIT 1;
        "#,
        [],
        |row| row.get(0),
    )?;

    let hydro_node: String = conn.query_row(
        "SELECT nodeid FROM shardinstance WHERE shardid = ?1;",
        params![hydro_shardid],
        |row| row.get(0),
    )?;

    let hydro_region: String = conn.query_row(
        "SELECT region FROM shardinstance WHERE shardid = ?1;",
        params![hydro_shardid],
        |row| row.get(0),
    )?;

    let flowvac_material_id = format!("FLOWVAC-MAT-{}", flowvac_shardid);

    // Blast link: FlowVac substrate shard influences Phoenix hydrological node hydraulics & carbon.
    conn.execute(
        r#"
        INSERT INTO blastradiuslink
            (sourcetype, sourceid, targettype, targetid, impacttype,
             impactscore, vtsensitivity, notes)
        VALUES
            ('SHARD', ?1, 'NODE', ?2, 'HYDRAULIC',
             0.10, 0.05,
             'FlowVac substrate corridor influences hydraulic buffer risk for this node.'),
            ('SHARD', ?1, 'NODE', ?2, 'CARBON',
             0.08, 0.04,
             'Material carbon footprint affects carbon plane at hydrological node.'),
            ('SHARD', ?1, 'MATERIAL', ?3, 'MATERIAL',
             0.20, 0.03,
             'Direct material-plane impact for this substrate shard.');
        "#,
        params![flowvac_shardid, hydro_node, flowvac_material_id],
    )?;

    // Blast link: Phoenix hydrological buffer shard influences its own region biodiversity.
    conn.execute(
        r#"
        INSERT INTO blastradiuslink
            (sourcetype, sourceid, targettype, targetid, impacttype,
             impactscore, vtsensitivity, notes)
        VALUES
            ('SHARD', ?1, 'REGION', ?2, 'BIODIVERSITY',
             0.12, 0.06,
             'Hydrological buffer performance constrains habitat connectivity in this region.');
        "#,
        params![hydro_shardid, hydro_region],
    )?;

    // Seed example workloadledger rows for cyboquatic FOG routing and FlowVac material optimization.
    conn.execute(
        r#"
        INSERT INTO workloadledger
            (shardid, variantid, nodeid, channel,
             ereq_j, esurplus_j, rcarbon, rbiodiv,
             vt_before, vt_after, decision, timestamputc)
        VALUES
            (?1, 'FOG-ROUTE-V1', ?2, 'energy',
             8.0e5, 1.1e6, 0.10, 0.04,
             0.45, 0.42, 'ACCEPT', '2026-05-30T12:00:00Z'),
            (?1, 'FOG-ROUTE-V1', ?2, 'carbon',
             0.0, 0.0, 0.10, 0.04,
             0.45, 0.42, 'ACCEPT', '2026-05-30T12:00:00Z'),
            (?3, 'FLOWVAC-OPT-V1', ?4, 'materials',
             2.0e5, 3.0e5, 0.09, 0.03,
             0.40, 0.38, 'ACCEPT', '2026-05-30T12:05:00Z');
        "#,
        params![hydro_shardid, hydro_node, flowvac_shardid, hydro_node],
    )?;

    Ok(())
}
