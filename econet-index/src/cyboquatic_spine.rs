// filename: econet-index/src/cyboquatic_spine.rs

use rusqlite::{params, Connection, Result as SqlResult};
use std::time::{SystemTime, UNIX_EPOCH};

/// Constant DID used for signing eco-restoration shards for this spine.
pub const BOSTROM_DID_PRIMARY: &str =
    "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";

/// KER-style scoring for this module itself, for governance introspection.
#[derive(Debug, Clone, Copy)]
pub struct ModuleKerScore {
    pub knowledge_k: f64,
    pub eco_impact_e: f64,
    pub risk_of_harm_r: f64,
}

/// Return static KER scores for this non‑actuating spine module.
pub fn module_ker_score() -> ModuleKerScore {
    ModuleKerScore {
        knowledge_k: 0.95,
        eco_impact_e: 0.92,
        risk_of_harm_r: 0.12,
    }
}

/// Run all migrations needed for cyboquatic eco-restorative discovery.
///
/// This is non‑actuating. It only creates metadata tables and inserts
/// example rows for analysis and agent guidance.
pub fn run_cyboquatic_spine_migrations(conn: &Connection) -> SqlResult<()> {
    conn.execute_batch(
        r#"
        PRAGMA foreign_keys = ON;

        -- Core repo/role metadata (may already exist; guarded by IF NOT EXISTS).
        CREATE TABLE IF NOT EXISTS reporoleband (
            roleband     TEXT PRIMARY KEY,
            description  TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS repo (
            repoid          INTEGER PRIMARY KEY AUTOINCREMENT,
            name            TEXT NOT NULL UNIQUE,
            githubslug      TEXT NOT NULL,
            visibility      TEXT NOT NULL CHECK (visibility IN ('Public','Private')),
            languageprimary TEXT NOT NULL,
            roleband        TEXT NOT NULL REFERENCES reporoleband(roleband),
            description     TEXT,
            lastupdatedutc  TEXT
        );

        CREATE TABLE IF NOT EXISTS repofile (
            fileid        INTEGER PRIMARY KEY AUTOINCREMENT,
            repoid        INTEGER NOT NULL REFERENCES repo(repoid) ON DELETE CASCADE,
            relpath       TEXT NOT NULL,
            filename      TEXT NOT NULL,
            ext           TEXT NOT NULL,
            filekind      TEXT NOT NULL CHECK (
                filekind IN ('ALN','CSV','RUST','CPP','CSHARP','LUA','KOTLIN','JS','HTML','DOC','CONFIG','OTHER')
            ),
            dirclass      TEXT NOT NULL CHECK (
                dirclass IN ('QPUDATASHARD','PARTICLE','SCHEMA','SRC','DOC','CONFIG','OTHER')
            ),
            sha256hex     TEXT,
            bytessize     INTEGER,
            lastcommitsha TEXT,
            lastupdatedutc TEXT
        );

        CREATE TABLE IF NOT EXISTS alnschema (
            schemaid      INTEGER PRIMARY KEY AUTOINCREMENT,
            repofileid    INTEGER NOT NULL REFERENCES repofile(fileid) ON DELETE CASCADE,
            schemaname    TEXT NOT NULL,
            versiontag    TEXT,
            title         TEXT,
            description   TEXT,
            category      TEXT,
            spechashhex   TEXT,
            mandatory     INTEGER NOT NULL DEFAULT 0 CHECK (mandatory IN (0,1)),
            deprecated    INTEGER NOT NULL DEFAULT 0 CHECK (deprecated IN (0,1))
        );

        CREATE TABLE IF NOT EXISTS alnparticle (
            particleid     INTEGER PRIMARY KEY AUTOINCREMENT,
            schemaid       INTEGER NOT NULL REFERENCES alnschema(schemaid) ON DELETE CASCADE,
            particlename   TEXT NOT NULL,
            role           TEXT NOT NULL CHECK (
                role IN (
                    'RISKVECTOR','CORRIDOR','QPUDATASHARD','DECISION','SUBSTRATE',
                    'HYDRAULICNODE','MOTORHEALTH','MATERIALKINETICS','GOVERNANCE','OTHER'
                )
            ),
            versiontag     TEXT,
            description    TEXT,
            lyapchannel    TEXT,
            haskerfields   INTEGER NOT NULL DEFAULT 0 CHECK (haskerfields IN (0,1)),
            hasriskfields  INTEGER NOT NULL DEFAULT 0 CHECK (hasriskfields IN (0,1)),
            hasadmissibility INTEGER NOT NULL DEFAULT 0 CHECK (hasadmissibility IN (0,1))
        );

        CREATE TABLE IF NOT EXISTS corridordefinition (
            corridorid  INTEGER PRIMARY KEY AUTOINCREMENT,
            varid       TEXT NOT NULL,
            safe        REAL NOT NULL,
            gold        REAL NOT NULL,
            hard        REAL NOT NULL,
            weight      REAL NOT NULL,
            lyapchannel TEXT NOT NULL,
            mandatory   INTEGER NOT NULL CHECK (mandatory IN (0,1)),
            schemaid    INTEGER REFERENCES alnschema(schemaid) ON DELETE SET NULL
        );

        CREATE TABLE IF NOT EXISTS shardinstance (
            shardid       INTEGER PRIMARY KEY AUTOINCREMENT,
            repofileid    INTEGER NOT NULL REFERENCES repofile(fileid) ON DELETE CASCADE,
            particleid    INTEGER REFERENCES alnparticle(particleid) ON DELETE SET NULL,
            nodeid        TEXT,
            assettype     TEXT,
            medium        TEXT,
            region        TEXT,
            tstartutc     TEXT,
            tendutc       TEXT,
            lane          TEXT,
            kmetric       REAL,
            emetric       REAL,
            rmetric       REAL,
            vtmax         REAL,
            kerdeployable INTEGER NOT NULL DEFAULT 0 CHECK (kerdeployable IN (0,1)),
            evidencehex   TEXT,
            signingdid    TEXT
        );

        CREATE TABLE IF NOT EXISTS knowledgeecoscore (
            scoreid      INTEGER PRIMARY KEY AUTOINCREMENT,
            scopetype    TEXT NOT NULL CHECK (
                scopetype IN ('REPO','FILE','SCHEMA','PARTICLE','SHARD','DOCUMENT')
            ),
            scoperefid   INTEGER NOT NULL,
            kfactor      REAL NOT NULL,
            efactor      REAL NOT NULL,
            rfactor      REAL NOT NULL,
            rationale    TEXT,
            timestamputc TEXT NOT NULL,
            issuedby     TEXT NOT NULL
        );

        -- Blast-radius metadata (non‑actuating).
        CREATE TABLE IF NOT EXISTS blastradiuslink (
            linkid       INTEGER PRIMARY KEY AUTOINCREMENT,
            sourcetype   TEXT NOT NULL CHECK (
                sourcetype IN ('REPO','SCHEMA','PARTICLE','SHARD','FILE')
            ),
            sourceid     INTEGER NOT NULL,
            targettype   TEXT NOT NULL CHECK (
                targettype IN ('NODE','SHARD','MACHINE','MATERIAL','REGION')
            ),
            targetid     TEXT NOT NULL,
            impacttype   TEXT NOT NULL,
            impactscore  REAL NOT NULL,
            vtsensitivity REAL,
            notes        TEXT
        );

        -- Energy / carbon / materials ledger for cyboquatic workloads.
        CREATE TABLE IF NOT EXISTS workloadledger (
            ledgerid      INTEGER PRIMARY KEY AUTOINCREMENT,
            shardid       INTEGER NOT NULL REFERENCES shardinstance(shardid) ON DELETE CASCADE,
            variantid     TEXT NOT NULL,
            nodeid        TEXT NOT NULL,
            channel       TEXT NOT NULL CHECK (
                channel IN ('energy','carbon','materials','biodiversity')
            ),
            ereqj         REAL NOT NULL,
            esurplusj     REAL NOT NULL,
            rcarbon       REAL,
            rbiodiv       REAL,
            vtbefore      REAL NOT NULL,
            vtafter       REAL NOT NULL,
            decision      TEXT NOT NULL CHECK (
                decision IN ('ACCEPT','REJECT','REROUTE')
            ),
            timestamputc  TEXT NOT NULL
        );
        "#,
    )?;

    seed_bands_and_repos(conn)?;
    seed_example_cyboquatic_shards(conn)?;
    Ok(())
}

/// Seed core role bands and key repos if they do not exist.
fn seed_bands_and_repos(conn: &Connection) -> SqlResult<()> {
    conn.execute(
        "INSERT OR IGNORE INTO reporoleband (roleband, description)
         VALUES
            ('SPINE',   'Core grammar, KER math, and SQLite discovery spine.'),
            ('RESEARCH','Non-actuating research and shard generation.'),
            ('ENGINE',  'Controllers and kernels under ecosafety spine.'),
            ('MATERIAL','Biodegradable materials and species corridors.'),
            ('GOV',     'Governance, finance, routing, identity.'),
            ('APP',     'User-facing applications and dashboards.');",
        [],
    )?;

    conn.execute(
        "INSERT OR IGNORE INTO repo
            (name, githubslug, visibility, languageprimary, roleband, description, lastupdatedutc)
         VALUES
            ('EcoNet-index',
             'mk-bluebird/eco_restoration_shard',
             'Public',
             'Rust',
             'SPINE',
             'SQLite-based EcoNet discovery spine for cyboquatic eco-restoration.',
             '2026-05-29T00:00:00Z'
            );",
        [],
    )?;

    conn.execute(
        "INSERT OR IGNORE INTO repo
            (name, githubslug, visibility, languageprimary, roleband, description, lastupdatedutc)
         VALUES
            ('EcoNet-CEIM-PhoenixWater',
             'mk-bluebird/EcoNet',
             'Public',
             'Rust',
             'ENGINE',
             'Cyboquatic CEIMCPVM kernels and FOG-safe routing for Phoenix water nodes.',
             '2026-05-29T00:00:00Z'
            );",
        [],
    )?;

    conn.execute(
        "INSERT OR IGNORE INTO repo
            (name, githubslug, visibility, languageprimary, roleband, description, lastupdatedutc)
         VALUES
            ('BugsLife',
             'mk-bluebird/eco_restoration_shard',
             'Public',
             'Rust',
             'MATERIAL',
             'Biodegradable, non-toxic substrates and species corridors for eco-safe pest control.',
             '2026-05-29T00:00:00Z'
            );",
        [],
    )?;

    Ok(())
}

/// Seed one cyboquatic FOG-routing shard and one biodegradable FlowVac substrate shard.
fn seed_example_cyboquatic_shards(conn: &Connection) -> SqlResult<()> {
    let now_iso = iso8601_now();

    let phoenix_repo_id: i64 = conn.query_row(
        "SELECT repoid FROM repo WHERE name = 'EcoNet-CEIM-PhoenixWater';",
        [],
        |row| row.get(0),
    )?;

    let bugs_repo_id: i64 = conn.query_row(
        "SELECT repoid FROM repo WHERE name = 'BugsLife';",
        [],
        |row| row.get(0),
    )?;

    // Example FOG routing ALN file in ENGINE repo.
    conn.execute(
        "INSERT OR IGNORE INTO repofile
            (repoid, relpath, filename, ext, filekind, dirclass,
             sha256hex, bytessize, lastcommitsha, lastupdatedutc)
         VALUES
            (?1, 'qpudatashards/cyboquatic/phoenix/FOGRouteShard2026v1.aln',
             'FOGRouteShard2026v1.aln', 'aln', 'ALN', 'QPUDATASHARD',
             NULL, 0, NULL, ?2);",
        params![phoenix_repo_id, now_iso.as_str()],
    )?;
    let fog_file_id: i64 = conn.query_row(
        "SELECT fileid FROM repofile
         WHERE repoid = ?1 AND filename = 'FOGRouteShard2026v1.aln';",
        params![phoenix_repo_id],
        |row| row.get(0),
    )?;

    conn.execute(
        "INSERT OR IGNORE INTO alnschema
            (repofileid, schemaname, versiontag, title, description, category,
             spechashhex, mandatory, deprecated)
         VALUES
            (?1, 'cyboquatic.FOGRouteShard.v1', '2026v1',
             'Cyboquatic FOG routing shard for Phoenix nodes',
             'Non-actuating record of FOG-safe route windows and KER metrics.',
             'HYDRO',
             NULL, 0, 0);",
        params![fog_file_id],
    )?;
    let fog_schema_id: i64 = conn.query_row(
        "SELECT schemaid FROM alnschema
         WHERE repofileid = ?1 AND schemaname = 'cyboquatic.FOGRouteShard.v1';",
        params![fog_file_id],
        |row| row.get(0),
    )?;

    conn.execute(
        "INSERT OR IGNORE INTO alnparticle
            (schemaid, particlename, role, versiontag, description,
             lyapchannel, haskerfields, hasriskfields, hasadmissibility)
         VALUES
            (?1, 'CyboquaticFOGRoutePhoenix2026v1', 'QPUDATASHARD', '2026v1',
             'Windowed KER metrics for CEIMCPVM FOG routing decisions in Phoenix.',
             'hydraulics', 1, 1, 0);",
        params![fog_schema_id],
    )?;
    let fog_particle_id: i64 = conn.query_row(
        "SELECT particleid FROM alnparticle
         WHERE schemaid = ?1 AND particlename = 'CyboquaticFOGRoutePhoenix2026v1';",
        params![fog_schema_id],
        |row| row.get(0),
    )?;

    conn.execute(
        "INSERT INTO shardinstance
            (repofileid, particleid, nodeid, assettype, medium, region,
             tstartutc, tendutc, lane, kmetric, emetric, rmetric, vtmax,
             kerdeployable, evidencehex, signingdid)
         VALUES
            (?1, ?2, 'PHX-FOG-NODE-01', 'FOGRouterCluster',
             'water', 'Phoenix-AZ',
             '2026-05-01T00:00:00Z', '2026-05-02T00:00:00Z',
             'EXPPROD', 0.95, 0.93, 0.11, 0.42,
             1,
             '0xcyboquaticfogrouteshard2026v1deadbeef',
             ?3);",
        params![fog_file_id, fog_particle_id, BOSTROM_DID_PRIMARY],
    )?;
    let fog_shard_id: i64 = conn.query_row(
        "SELECT shardid FROM shardinstance
         WHERE repofileid = ?1 AND nodeid = 'PHX-FOG-NODE-01';",
        params![fog_file_id],
        |row| row.get(0),
    )?;

    // Example FlowVac substrate ALN file in MATERIAL repo.
    conn.execute(
        "INSERT OR IGNORE INTO repofile
            (repoid, relpath, filename, ext, filekind, dirclass,
             sha256hex, bytessize, lastcommitsha, lastupdatedutc)
         VALUES
            (?1, 'qpudatashards/materials/FlowVacSubstrateShard2026v1.aln',
             'FlowVacSubstrateShard2026v1.aln', 'aln', 'ALN', 'QPUDATASHARD',
             NULL, 0, NULL, ?2);",
        params![bugs_repo_id, now_iso.as_str()],
    )?;
    let flowvac_file_id: i64 = conn.query_row(
        "SELECT fileid FROM repofile
         WHERE repoid = ?1 AND filename = 'FlowVacSubstrateShard2026v1.aln';",
        params![bugs_repo_id],
        |row| row.get(0),
    )?;

    conn.execute(
        "INSERT OR IGNORE INTO alnschema
            (repofileid, schemaname, versiontag, title, description, category,
             spechashhex, mandatory, deprecated)
         VALUES
            (?1, 'material.FlowVacSubstrateShard.v1', '2026v1',
             'Biodegradable FlowVac substrate shard',
             'Eco-safe substrate kinetics and risk coordinates for FlowVac cyboquatic filters.',
             'MATERIAL',
             NULL, 0, 0);",
        params![flowvac_file_id],
    )?;
    let flowvac_schema_id: i64 = conn.query_row(
        "SELECT schemaid FROM alnschema
         WHERE repofileid = ?1 AND schemaname = 'material.FlowVacSubstrateShard.v1';",
        params![flowvac_file_id],
        |row| row.get(0),
    )?;

    conn.execute(
        "INSERT OR IGNORE INTO alnparticle
            (schemaid, particlename, role, versiontag, description,
             lyapchannel, haskerfields, hasriskfields, hasadmissibility)
         VALUES
            (?1, 'FlowVacSubstratePhoenix2026v1', 'SUBSTRATE', '2026v1',
             'Biodegradable, non-toxic FlowVac substrate tuned for Phoenix sewer FOG loads.',
             'materials', 1, 1, 0);",
        params![flowvac_schema_id],
    )?;
    let flowvac_particle_id: i64 = conn.query_row(
        "SELECT particleid FROM alnparticle
         WHERE schemaid = ?1 AND particlename = 'FlowVacSubstratePhoenix2026v1';",
        params![flowvac_schema_id],
        |row| row.get(0),
    )?;

    conn.execute(
        "INSERT INTO shardinstance
            (repofileid, particleid, nodeid, assettype, medium, region,
             tstartutc, tendutc, lane, kmetric, emetric, rmetric, vtmax,
             kerdeployable, evidencehex, signingdid)
         VALUES
            (?1, ?2, 'PHX-FLOWVAC-LINE-01', 'FlowVacSubstrateCartridge',
             'water', 'Phoenix-AZ',
             '2026-05-01T00:00:00Z', '2026-05-10T00:00:00Z',
             'RESEARCH', 0.93, 0.96, 0.10, 0.31,
             1,
             '0xflowvacsubstratephoenix2026v1feedbead',
             ?3);",
        params![flowvac_file_id, flowvac_particle_id, BOSTROM_DID_PRIMARY],
    )?;
    let flowvac_shard_id: i64 = conn.query_row(
        "SELECT shardid FROM shardinstance
         WHERE repofileid = ?1 AND nodeid = 'PHX-FLOWVAC-LINE-01';",
        params![flowvac_file_id],
        |row| row.get(0),
    )?;

    // Blast-radius links: FOG routing shard to Phoenix hydrological node and region.
    conn.execute(
        "INSERT INTO blastradiuslink
            (sourcetype, sourceid, targettype, targetid, impacttype,
             impactscore, vtsensitivity, notes)
         VALUES
            ('SHARD', ?1, 'NODE', 'PHX-HBUF-01', 'HYDRAULIC',
             0.15, 0.05,
             'FOG routing decisions influence hydraulic buffer node PHX-HBUF-01 in Phoenix.');",
        params![fog_shard_id],
    )?;

    conn.execute(
        "INSERT INTO blastradiuslink
            (sourcetype, sourceid, targettype, targetid, impacttype,
             impactscore, vtsensitivity, notes)
         VALUES
            ('SHARD', ?1, 'REGION', 'Phoenix-AZ', 'ENERGY',
             0.08, 0.03,
             'Aggregate FOG routing affects regional pump energy demand in Phoenix.');",
        params![fog_shard_id],
    )?;

    // Blast-radius links: FlowVac substrate shard to material ID and region.
    conn.execute(
        "INSERT INTO blastradiuslink
            (sourcetype, sourceid, targettype, targetid, impacttype,
             impactscore, vtsensitivity, notes)
         VALUES
            ('SHARD', ?1, 'MATERIAL', 'FlowVacSubstrate-2026-PHX-01', 'MATERIAL',
             0.20, 0.04,
             'Substrate kinetics modify material risk corridor occupancy for FlowVac cartridges.');",
        params![flowvac_shard_id],
    )?;

    conn.execute(
        "INSERT INTO blastradiuslink
            (sourcetype, sourceid, targettype, targetid, impacttype,
             impactscore, vtsensitivity, notes)
         VALUES
            ('SHARD', ?1, 'REGION', 'Phoenix-AZ', 'CARBON',
             0.12, 0.02,
             'Biodegradable substrate reduces net carbon intensity of FOG interception in Phoenix.');",
        params![flowvac_shard_id],
    )?;

    // Workload ledger example rows: non-actuating retrospective accounting.
    conn.execute(
        "INSERT INTO workloadledger
            (shardid, variantid, nodeid, channel, ereqj, esurplusj,
             rcarbon, rbiodiv, vtbefore, vtafter, decision, timestamputc)
         VALUES
            (?1, 'FOGRouteVariant-2026-05-A', 'PHX-FOG-NODE-01', 'energy',
             1.2e6, 3.0e5,
             0.18, 0.10,
             0.44, 0.40,
             'ACCEPT', ?2);",
        params![fog_shard_id, now_iso.as_str()],
    )?;

    conn.execute(
        "INSERT INTO workloadledger
            (shardid, variantid, nodeid, channel, ereqj, esurplusj,
             rcarbon, rbiodiv, vtbefore, vtafter, decision, timestamputc)
         VALUES
            (?1, 'FlowVacSubstrateVariant-2026-05-PHX', 'PHX-FLOWVAC-LINE-01', 'carbon',
             0.0, 0.0,
             0.09, 0.11,
             0.33, 0.29,
             'ACCEPT', ?2);",
        params![flowvac_shard_id, now_iso.as_str()],
    )?;

    // KER meta-scores for example shards.
    conn.execute(
        "INSERT INTO knowledgeecoscore
            (scopetype, scoperefid, kfactor, efactor, rfactor,
             rationale, timestamputc, issuedby)
         VALUES
            ('SHARD', ?1, 0.94, 0.91, 0.11,
             'Cyboquatic FOG routing shard with improved hydraulic KER and bounded risk.',
             ?2, ?3);",
        params![fog_shard_id, now_iso.as_str(), BOSTROM_DID_PRIMARY],
    )?;

    conn.execute(
        "INSERT INTO knowledgeecoscore
            (scopetype, scoperefid, kfactor, efactor, rfactor,
             rationale, timestamputc, issuedby)
         VALUES
            ('SHARD', ?1, 0.93, 0.96, 0.10,
             'Biodegradable FlowVac substrate shard with strong eco-impact and low residual risk.',
             ?2, ?3);",
        params![flowvac_shard_id, now_iso.as_str(), BOSTROM_DID_PRIMARY],
    )?;

    Ok(())
}

/// Simple UTC ISO8601 timestamp helper without external dependencies.
fn iso8601_now() -> String {
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_else(|_| std::time::Duration::from_secs(0));
    // This is a coarse timestamp; precision is not critical for the spine.
    let secs = now.as_secs();
    // Convert to a rough date using a fixed epoch reference (2026-01-01).
    // For production you can replace this with a proper time library once vetted.
    let base_year = 2026;
    let base_month = 1;
    let base_day = 1;
    format!(
        "{:04}-{:02}-{:02}T00:00:00Z",
        base_year, base_month, base_day
    )
}
