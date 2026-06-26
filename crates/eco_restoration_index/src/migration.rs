// filename: eco_restoration_index/src/migration.rs

use rusqlite::{params, Connection, Result as SqlResult};

pub fn run_all_migrations(conn: &Connection) -> SqlResult<()> {
    conn.execute_batch(include_str!("../sql/db_eco_restoration_index.sql"))?;
    seed_example_shards(conn)?;
    Ok(())
}

fn seed_example_shards(conn: &Connection) -> SqlResult<()> {
    let signingdid = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";

    // Get repo id
    let repoid: i64 = conn.query_row(
        "SELECT repoid FROM repo WHERE name = 'eco_restoration_shard'",
        [],
        |row| row.get(0),
    )?;

    // Register ALN files for example Cyboquatic and FlowVac substrate shards
    conn.execute(
        "INSERT OR IGNORE INTO repofile
         (repoid, relpath, purpose, language, createdutc, updatedutc)
         VALUES (?1, ?2, 'DATASHARD', 'ALN', datetime('now'), datetime('now'))",
        params![repoid, "qpudatashards/CyboquaticFogRoutingPhoenix2026v1.aln"],
    )?;
    conn.execute(
        "INSERT OR IGNORE INTO repofile
         (repoid, relpath, purpose, language, createdutc, updatedutc)
         VALUES (?1, ?2, 'DATASHARD', 'ALN', datetime('now'), datetime('now'))",
        params![repoid, "qpudatashards/FlowVacSubstrateShard.v1.aln"],
    )?;

    let fog_fileid: i64 = conn.query_row(
        "SELECT fileid FROM repofile
         WHERE repoid = ?1 AND relpath = ?2",
        params![repoid, "qpudatashards/CyboquaticFogRoutingPhoenix2026v1.aln"],
        |row| row.get(0),
    )?;
    let flowvac_fileid: i64 = conn.query_row(
        "SELECT fileid FROM repofile
         WHERE repoid = ?1 AND relpath = ?2",
        params![repoid, "qpudatashards/FlowVacSubstrateShard.v1.aln"],
        |row| row.get(0),
    )?;

    // Seed shardinstances
    conn.execute(
        "INSERT OR IGNORE INTO shardinstance
         (repofileid, particle, nodeid, assettype, medium, region,
          lane, tstartutc, tendutc, kmetric, emetric, rmetric, vtmax,
          kerdeployable, evidencehex, signingdid, createdutc, updatedutc)
         VALUES
         (?1, 'CyboquaticFogRoutingPhoenix2026v1', 'PHX-CYBO-NODE-01',
          'FogRouterCluster', 'water', 'Phoenix-AZ',
          'EXPPROD', '2026-01-01T00:00:00Z', '2026-01-31T23:59:59Z',
          0.94, 0.91, 0.12, 0.40,
          1, 'a1b2c3d4e5f67890', ?2, datetime('now'), datetime('now'))",
        params![fog_fileid, signingdid],
    )?;

    conn.execute(
        "INSERT OR IGNORE INTO shardinstance
         (repofileid, particle, nodeid, assettype, medium, region,
          lane, tstartutc, tendutc, kmetric, emetric, rmetric, vtmax,
          kerdeployable, evidencehex, signingdid, createdutc, updatedutc)
         VALUES
         (?1, 'FlowVacSubstrateShard.v1', 'FLOWVAC-BATCH-2026-01',
          'SubstrateBatch', 'material', 'Lab-CI',
          'RESEARCH', '2026-01-01T00:00:00Z', '2026-01-01T00:00:00Z',
          0.93, 0.95, 0.12, 0.37,
          1, 'f0e1d2c3b4a59687', ?2, datetime('now'), datetime('now'))",
        params![flowvac_fileid, signingdid],
    )?;

    let fog_shardid: i64 = conn.query_row(
        "SELECT shardid FROM shardinstance
         WHERE nodeid = 'PHX-CYBO-NODE-01' AND assettype = 'FogRouterCluster'
         LIMIT 1",
        [],
        |row| row.get(0),
    )?;
    let flowvac_shardid: i64 = conn.query_row(
        "SELECT shardid FROM shardinstance
         WHERE nodeid = 'FLOWVAC-BATCH-2026-01' AND assettype = 'SubstrateBatch'
         LIMIT 1",
        [],
        |row| row.get(0),
    )?;

    // Seed knowledgeecoscore for the shards
    conn.execute(
        "INSERT OR IGNORE INTO knowledgeecoscore
         (scopetype, scoperefid, kfactor, efactor, rfactor, createdutc, updatedutc)
         VALUES ('SHARD', ?1, 0.94, 0.91, 0.12, datetime('now'), datetime('now'))",
        params![fog_shardid],
    )?;
    conn.execute(
        "INSERT OR IGNORE INTO knowledgeecoscore
         (scopetype, scoperefid, kfactor, efactor, rfactor, createdutc, updatedutc)
         VALUES ('SHARD', ?1, 0.93, 0.95, 0.12, datetime('now'), datetime('now'))",
        params![flowvac_shardid],
    )?;

    // Attach blast-radius links
    conn.execute(
        "INSERT INTO blastradiuslink
         (sourcetype, sourceid, targettype, targetid,
          impacttype, impactscore, vtsensitivity, notes)
         VALUES
         ('SHARD', ?1, 'NODE', 'PHX-CYBO-NODE-01',
          'HYDRAULIC', 0.40, -0.03,
          'Cyboquatic routing shard influences surcharge corridors for PHX-CYBO-NODE-01.')",
        params![fog_shardid],
    )?;
    conn.execute(
        "INSERT INTO blastradiuslink
         (sourcetype, sourceid, targettype, targetid,
          impacttype, impactscore, vtsensitivity, notes)
         VALUES
         ('SHARD', ?1, 'NODE', 'PHX-CYBO-NODE-01',
          'ENERGY', 0.30, -0.02,
          'Routing decisions adjust per-joule utilization without increasing Vt.')",
        params![fog_shardid],
    )?;
    conn.execute(
        "INSERT INTO blastradiuslink
         (sourcetype, sourceid, targettype, targetid,
          impacttype, impactscore, vtsensitivity, notes)
         VALUES
         ('SHARD', ?1, 'MATERIAL', 'FLOWVAC-BATCH-2026-01',
          'CARBON', 0.25, -0.01,
          'FlowVac substrate batch has carbon-negative profile for this Cyboquatic region.')",
        params![flowvac_shardid],
    )?;

    // Seed workload ledger examples
    conn.execute(
        "INSERT INTO workloadledger
         (shardid, variantid, nodeid, channel,
          ereqj, esurplusj, rcarbon, rbiodiv,
          vtbefore, vtafter, decision, timestamputc)
         VALUES
         (?1, 'CyboVariant-42', 'PHX-CYBO-NODE-01', 'energy',
          500.0, 5000.0, 0.20, 0.10,
          0.40, 0.395, 'ACCEPT', '2026-01-15T12:00:00Z')",
        params![fog_shardid],
    )?;
    conn.execute(
        "INSERT INTO workloadledger
         (shardid, variantid, nodeid, channel,
          ereqj, esurplusj, rcarbon, rbiodiv,
          vtbefore, vtafter, decision, timestamputc)
         VALUES
         (?1, 'CyboVariant-99', 'PHX-CYBO-NODE-01', 'carbon',
          200.0, 4800.0, 0.18, 0.09,
          0.395, 0.392, 'ACCEPT', '2026-01-15T12:05:00Z')",
        params![fog_shardid],
    )?;

    Ok(())
}
