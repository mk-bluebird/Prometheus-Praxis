// File: ecoshard/sqlite/ecosafety_index_v1.rs
// Target repo: mk-bluebird/eco_restoration_shard

use rusqlite::{params, Connection, Result};

/// Knowledge-factor, eco-impact, and risk-of-harm scores.
/// All values must be in [0.0, 1.0].
#[derive(Debug, Clone, Copy)]
pub struct KerTriple {
    pub k: f64,
    pub e: f64,
    pub r: f64,
}

/// Blast-radius classification for any code or machine asset.
/// This is a logical classification; the actual radius is encoded
/// in the corridors referenced by `corridor_spec_id`.
#[derive(Debug, Clone, Copy)]
pub enum BlastRadius {
    /// Purely diagnostic, non-actuating workloads and documents.
    DiagnosticOnly,
    /// Actuation allowed, but only inside tightly bounded corridors.
    LocalActuation,
    /// City- or basin-scale impact if misused.
    CityScale,
}

impl BlastRadius {
    pub fn as_str(&self) -> &'static str {
        match self {
            BlastRadius::DiagnosticOnly => "diagnostic_only",
            BlastRadius::LocalActuation => "local_actuation",
            BlastRadius::CityScale => "city_scale",
        }
    }
}

/// Minimal ecological planes recorded for every asset.
#[derive(Debug, Clone, Copy)]
pub struct EcoPlanes {
    pub r_energy: f64,
    pub r_hydraulic: f64,
    pub r_biology: f64,
    pub r_carbon: f64,
    pub r_materials: f64,
}

/// Main entry point: create or upgrade the ecosafety index database.
/// This function is idempotent: running it multiple times will keep the schema stable.
///
/// Destination: `ecoshard/sqlite/ecosafety_index_v1.db`
pub fn init_ecosafety_index(db_path: &str) -> Result<()> {
    let conn = Connection::open(db_path)?;

    // Global pragma for durability; journaling/WAL decisions remain with the operator.
    conn.pragma_update(None, "foreign_keys", &"ON")?;

    // Table 1: canonical repositories in the constellation.
    conn.execute_batch(
        r#"
        CREATE TABLE IF NOT EXISTS repo (
            repo_id         INTEGER PRIMARY KEY AUTOINCREMENT,
            name            TEXT NOT NULL UNIQUE,
            url             TEXT NOT NULL,
            description     TEXT,
            created_utc     TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
            -- Governance flags
            is_authoritative INTEGER NOT NULL DEFAULT 0,
            hex_provenance   TEXT NOT NULL
        );

        -- Table 2: logical projects within repos, e.g. "Cyboquatic FOG router",
        -- "Drainage decay diagnostics", "Artemis-Cyboquatic spine".
        CREATE TABLE IF NOT EXISTS project (
            project_id      INTEGER PRIMARY KEY AUTOINCREMENT,
            repo_id         INTEGER NOT NULL REFERENCES repo(repo_id) ON DELETE CASCADE,
            name            TEXT NOT NULL,
            description     TEXT,
            -- Bostrom-aligned DID of the primary steward of this project.
            steward_did     TEXT NOT NULL,
            -- Optional ALN shard id anchoring corridor specs for this project.
            corridor_spec_id TEXT,
            UNIQUE (repo_id, name)
        );

        -- Table 3: language-typed code artifacts (files, crates, modules, binaries).
        CREATE TABLE IF NOT EXISTS artifact (
            artifact_id     INTEGER PRIMARY KEY AUTOINCREMENT,
            project_id      INTEGER NOT NULL REFERENCES project(project_id) ON DELETE CASCADE,
            path            TEXT NOT NULL,
            language        TEXT NOT NULL CHECK (language IN (
                                'rust','aln','lua','cpp','kotlin','sql','schema','doc'
                            )),
            kind            TEXT NOT NULL CHECK (kind IN (
                                'library','binary','config','schema','document'
                            )),
            blast_radius    TEXT NOT NULL CHECK (blast_radius IN (
                                'diagnostic_only','local_actuation','city_scale'
                            )),
            -- Semantic hash of the artifact (e.g., content hash); algorithm is defined externally
            -- and must respect the blacklist (no forbidden hashes).
            semantic_hash   TEXT NOT NULL,
            -- Hex-stamped provenance string anchored to Phoenix / EcoNet ecosafety math.
            provenance_hex  TEXT NOT NULL,
            created_utc     TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
            UNIQUE (project_id, path)
        );

        -- Table 4: per-artifact KER window.
        CREATE TABLE IF NOT EXISTS artifact_ker (
            artifact_id     INTEGER PRIMARY KEY REFERENCES artifact(artifact_id) ON DELETE CASCADE,
            k_knowledge     REAL NOT NULL CHECK (k_knowledge >= 0.0 AND k_knowledge <= 1.0),
            e_eco_impact    REAL NOT NULL CHECK (e_eco_impact >= 0.0 AND e_eco_impact <= 1.0),
            r_risk_of_harm  REAL NOT NULL CHECK (r_risk_of_harm >= 0.0 AND r_risk_of_harm <= 1.0),
            -- True if allowed in production; CI must enforce K>=0.90, E>=0.90, R<=0.13.
            ker_deployable  INTEGER NOT NULL DEFAULT 0
        );

        -- Table 5: normalized ecosafety planes for each artifact.
        -- These are corridor-normalized 0..1 risk coordinates.
        CREATE TABLE IF NOT EXISTS artifact_planes (
            artifact_id     INTEGER PRIMARY KEY REFERENCES artifact(artifact_id) ON DELETE CASCADE,
            r_energy        REAL NOT NULL CHECK (r_energy >= 0.0 AND r_energy <= 1.0),
            r_hydraulic     REAL NOT NULL CHECK (r_hydraulic >= 0.0 AND r_hydraulic <= 1.0),
            r_biology       REAL NOT NULL CHECK (r_biology >= 0.0 AND r_biology <= 1.0),
            r_carbon        REAL NOT NULL CHECK (r_carbon >= 0.0 AND r_carbon <= 1.0),
            r_materials     REAL NOT NULL CHECK (r_materials >= 0.0 AND r_materials <= 1.0),
            -- Derived residual, using a simple equal-weighted quadratic form.
            v_residual      REAL NOT NULL,
            CHECK (v_residual >= 0.0)
        );

        -- Table 6: blast-radius corridors for grouping assets into operational bands.
        CREATE TABLE IF NOT EXISTS blast_radius_band (
            band_id         INTEGER PRIMARY KEY AUTOINCREMENT,
            name            TEXT NOT NULL UNIQUE,
            description     TEXT,
            -- Hard constraints on acceptable KER ranges for this band.
            min_k           REAL NOT NULL CHECK (min_k >= 0.0 AND min_k <= 1.0),
            min_e           REAL NOT NULL CHECK (min_e >= 0.0 AND min_e <= 1.0),
            max_r           REAL NOT NULL CHECK (max_r >= 0.0 AND max_r <= 1.0),
            -- True if actuation is allowed in this band; false for diagnostics-only bands.
            allows_actuation INTEGER NOT NULL DEFAULT 0
        );

        -- Table 7: mapping artifacts into blast-radius bands.
        CREATE TABLE IF NOT EXISTS artifact_band (
            artifact_id     INTEGER NOT NULL REFERENCES artifact(artifact_id) ON DELETE CASCADE,
            band_id         INTEGER NOT NULL REFERENCES blast_radius_band(band_id) ON DELETE CASCADE,
            PRIMARY KEY (artifact_id, band_id)
        );

        -- Table 8: SQLite index metadata for cross-repo constellation queries.
        CREATE TABLE IF NOT EXISTS index_snapshot (
            snapshot_id     INTEGER PRIMARY KEY AUTOINCREMENT,
            created_utc     TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
            -- Hex-stamped ALN shard id that describes how this index was built.
            aln_index_spec  TEXT NOT NULL,
            -- Simple scalar summarizing index health (0..1); implementation-specific.
            index_quality   REAL NOT NULL CHECK (index_quality >= 0.0 AND index_quality <= 1.0)
        );

        -- Indexes for fast constellation queries.
        CREATE INDEX IF NOT EXISTS idx_artifact_language
            ON artifact(language);

        CREATE INDEX IF NOT EXISTS idx_artifact_blastradius
            ON artifact(blast_radius);

        CREATE INDEX IF NOT EXISTS idx_artifact_ker_k
            ON artifact_ker(k_knowledge);

        CREATE INDEX IF NOT EXISTS idx_artifact_ker_r
            ON artifact_ker(r_risk_of_harm);

        CREATE INDEX IF NOT EXISTS idx_artifact_planes_v
            ON artifact_planes(v_residual);
        "#,
    )?;

    seed_default_bands(&conn)?;

    Ok(())
}

fn seed_default_bands(conn: &Connection) -> Result<()> {
    // Insert default bands only if the table is empty.
    let count: i64 = conn.query_row(
        "SELECT COUNT(*) FROM blast_radius_band",
        [],
        |row| row.get(0),
    )?;

    if count > 0 {
        return Ok(());
    }

    // Band: research-only diagnostics (no actuation).
    conn.execute(
        r#"
        INSERT INTO blast_radius_band
            (name, description, min_k, min_e, max_r, allows_actuation)
        VALUES
            (
                'diagnostic_research',
                'Function-only diagnostics; non-actuating frames and SQL schemas. Safe for broad replication.',
                0.80, 0.80, 0.20, 0
            )
        "#,
        [],
    )?;

    // Band: local actuation under strict corridors (prototype hardware).
    conn.execute(
        r#"
        INSERT INTO blast_radius_band
            (name, description, min_k, min_e, max_r, allows_actuation)
        VALUES
            (
                'local_actuation_pilot',
                'Pilot-grade local actuation; requires K>=0.90, E>=0.90, R<=0.13.',
                0.90, 0.90, 0.13, 1
            )
        "#,
        [],
    )?;

    // Band: city-scale actuation (Artemis-Cyboquatic, EcoNet-integrated).
    conn.execute(
        r#"
        INSERT INTO blast_radius_band
            (name, description, min_k, min_e, max_r, allows_actuation)
        VALUES
            (
                'city_scale_ker_strict',
                'City-scale ecosafety machinery under Artemis-Cyboquatic governance; K>=0.94, E>=0.91, R<=0.12.',
                0.94, 0.91, 0.12, 1
            )
        "#,
        [],
    )?;

    Ok(())
}

/// Register or update a repo entry.
/// For eco_restoration_shard, this should be called once at bootstrap.
pub fn upsert_repo(
    conn: &Connection,
    name: &str,
    url: &str,
    description: &str,
    is_authoritative: bool,
    hex_provenance: &str,
) -> Result<i64> {
    conn.execute(
        r#"
        INSERT INTO repo (name, url, description, is_authoritative, hex_provenance)
        VALUES (?1, ?2, ?3, ?4, ?5)
        ON CONFLICT(name) DO UPDATE SET
            url = excluded.url,
            description = excluded.description,
            is_authoritative = excluded.is_authoritative,
            hex_provenance = excluded.hex_provenance
        "#,
        params![
            name,
            url,
            description,
            if is_authoritative { 1 } else { 0 },
            hex_provenance
        ],
    )?;

    let id: i64 = conn.query_row(
        "SELECT repo_id FROM repo WHERE name = ?1",
        params![name],
        |row| row.get(0),
    )?;
    Ok(id)
}

/// Insert or update a project row.
pub fn upsert_project(
    conn: &Connection,
    repo_id: i64,
    name: &str,
    description: &str,
    steward_did: &str,
    corridor_spec_id: Option<&str>,
) -> Result<i64> {
    conn.execute(
        r#"
        INSERT INTO project (repo_id, name, description, steward_did, corridor_spec_id)
        VALUES (?1, ?2, ?3, ?4, ?5)
        ON CONFLICT(repo_id, name) DO UPDATE SET
            description = excluded.description,
            steward_did = excluded.steward_did,
            corridor_spec_id = excluded.corridor_spec_id
        "#,
        params![repo_id, name, description, steward_did, corridor_spec_id],
    )?;

    let id: i64 = conn.query_row(
        "SELECT project_id FROM project WHERE repo_id = ?1 AND name = ?2",
        params![repo_id, name],
        |row| row.get(0),
    )?;
    Ok(id)
}

/// Upsert a code or document artifact and its ecosafety planes + KER triple.
/// This is the main integration point for cross-repo constellation logic.
pub fn upsert_artifact_with_metrics(
    conn: &Connection,
    project_id: i64,
    path: &str,
    language: &str,
    kind: &str,
    blast_radius: BlastRadius,
    semantic_hash: &str,
    provenance_hex: &str,
    ker: KerTriple,
    planes: EcoPlanes,
) -> Result<i64> {
    conn.execute(
        r#"
        INSERT INTO artifact
            (project_id, path, language, kind, blast_radius, semantic_hash, provenance_hex)
        VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)
        ON CONFLICT(project_id, path) DO UPDATE SET
            language = excluded.language,
            kind = excluded.kind,
            blast_radius = excluded.blast_radius,
            semantic_hash = excluded.semantic_hash,
            provenance_hex = excluded.provenance_hex
        "#,
        params![
            project_id,
            path,
            language,
            kind,
            blast_radius.as_str(),
            semantic_hash,
            provenance_hex
        ],
    )?;

    let artifact_id: i64 = conn.query_row(
        "SELECT artifact_id FROM artifact WHERE project_id = ?1 AND path = ?2",
        params![project_id, path],
        |row| row.get(0),
    )?;

    // Derived residual as equal-weighted quadratic over planes.
    let v_residual = {
        let sum_sq = planes.r_energy * planes.r_energy
            + planes.r_hydraulic * planes.r_hydraulic
            + planes.r_biology * planes.r_biology
            + planes.r_carbon * planes.r_carbon
            + planes.r_materials * planes.r_materials;
        // Equal weights => V = sum r_j^2
        sum_sq
    };

    // KER deployability is a soft flag; CI must enforce thresholds.
    let ker_deployable = if ker.k >= 0.90 && ker.e >= 0.90 && ker.r <= 0.13 {
        1
    } else {
        0
    };

    conn.execute(
        r#"
        INSERT INTO artifact_ker
            (artifact_id, k_knowledge, e_eco_impact, r_risk_of_harm, ker_deployable)
        VALUES (?1, ?2, ?3, ?4, ?5)
        ON CONFLICT(artifact_id) DO UPDATE SET
            k_knowledge   = excluded.k_knowledge,
            e_eco_impact  = excluded.e_eco_impact,
            r_risk_of_harm= excluded.r_risk_of_harm,
            ker_deployable= excluded.ker_deployable
        "#,
        params![
            artifact_id,
            ker.k,
            ker.e,
            ker.r,
            ker_deployable
        ],
    )?;

    conn.execute(
        r#"
        INSERT INTO artifact_planes
            (artifact_id, r_energy, r_hydraulic, r_biology, r_carbon, r_materials, v_residual)
        VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)
        ON CONFLICT(artifact_id) DO UPDATE SET
            r_energy    = excluded.r_energy,
            r_hydraulic = excluded.r_hydraulic,
            r_biology   = excluded.r_biology,
            r_carbon    = excluded.r_carbon,
            r_materials = excluded.r_materials,
            v_residual  = excluded.v_residual
        "#,
        params![
            artifact_id,
            planes.r_energy,
            planes.r_hydraulic,
            planes.r_biology,
            planes.r_carbon,
            planes.r_materials,
            v_residual
        ],
    )?;

    Ok(artifact_id)
}

/// Query helper: find all deployable Rust ecosafety artifacts for a given blast radius band.
/// This is meant to drive autonomous code improvement pipelines that only compose safe pieces.
pub fn list_deployable_rust_by_band(
    conn: &Connection,
    band_name: &str,
) -> Result<Vec<(String, String)>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT a.path, p.name
        FROM artifact a
        JOIN artifact_ker k ON a.artifact_id = k.artifact_id
        JOIN artifact_band ab ON a.artifact_id = ab.artifact_id
        JOIN blast_radius_band b ON ab.band_id = b.band_id
        JOIN project p ON a.project_id = p.project_id
        WHERE a.language = 'rust'
          AND k.ker_deployable = 1
          AND b.name = ?1
        ORDER BY p.name, a.path
        "#,
    )?;

    let rows = stmt
        .query_map([band_name], |row| {
            let path: String = row.get(0)?;
            let project_name: String = row.get(1)?;
            Ok((project_name, path))
        })?
        .collect::<Result<Vec<_>, _>>()?;

    Ok(rows)
}
