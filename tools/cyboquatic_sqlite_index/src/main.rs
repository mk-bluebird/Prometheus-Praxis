// File: tools/cyboquatic_sqlite_index/src/main.rs
// Target repo: github.com/Doctor0Evil/eco_restoration_shard (new tool crate)
// Purpose: Production-grade SQLite index + blast-radius tables for EcoNet constellation nodes.
// This binary can be wired into CI, FOG routers, or offline planners.

use rusqlite::{params, Connection, Result};
use std::path::Path;

/// Initialize or migrate the EcoNet constellation SQLite database.
/// Schema is versioned and append-only for safety.
fn init_schema(conn: &Connection) -> Result<()> {
    conn.execute_batch(
        r#"
        PRAGMA journal_mode = WAL;
        PRAGMA foreign_keys = ON;

        CREATE TABLE IF NOT EXISTS schema_version (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            version INTEGER NOT NULL
        );

        INSERT INTO schema_version (id, version)
        VALUES (1, 1)
        ON CONFLICT(id) DO NOTHING;

        CREATE TABLE IF NOT EXISTS nodes (
            node_id TEXT PRIMARY KEY,
            repo_hint TEXT NOT NULL,
            region TEXT NOT NULL,
            node_type TEXT NOT NULL,
            lane TEXT NOT NULL CHECK (lane IN ('RESEARCH', 'PILOT', 'PRODUCTION')),
            bostrom_did TEXT NOT NULL,
            geo_lat REAL,
            geo_lon REAL
        );

        CREATE TABLE IF NOT EXISTS risk_planes (
            plane_id INTEGER PRIMARY KEY,
            name TEXT NOT NULL UNIQUE
        );

        INSERT OR IGNORE INTO risk_planes (plane_id, name) VALUES
            (0, 'energy'),
            (1, 'hydraulics'),
            (2, 'biology'),
            (3, 'carbon'),
            (4, 'materials'),
            (5, 'biodiversity'),
            (6, 'uncertainty');

        CREATE TABLE IF NOT EXISTS node_risk_state (
            node_id TEXT NOT NULL,
            ts_utc INTEGER NOT NULL,
            plane_id INTEGER NOT NULL,
            coord REAL NOT NULL CHECK (coord >= 0.0 AND coord <= 1.0),
            vt REAL NOT NULL CHECK (vt >= 0.0),
            k_knowledge REAL NOT NULL CHECK (k_knowledge >= 0.0 AND k_knowledge <= 1.0),
            e_ecoimpact REAL NOT NULL CHECK (e_ecoimpact >= 0.0 AND e_ecoimpact <= 1.0),
            r_risk_of_harm REAL NOT NULL CHECK (r_risk_of_harm >= 0.0 AND r_risk_of_harm <= 1.0),
            blast_radius TEXT NOT NULL CHECK (
                blast_radius IN ('local_low','local_moderate','basin','constellation')
            ),
            evidence_hex TEXT NOT NULL,
            PRIMARY KEY (node_id, ts_utc, plane_id),
            FOREIGN KEY (node_id) REFERENCES nodes(node_id) ON DELETE CASCADE,
            FOREIGN KEY (plane_id) REFERENCES risk_planes(plane_id) ON DELETE RESTRICT
        );

        CREATE TABLE IF NOT EXISTS blast_radius_summary (
            node_id TEXT NOT NULL,
            ts_utc INTEGER NOT NULL,
            vt REAL NOT NULL CHECK (vt >= 0.0),
            blast_radius TEXT NOT NULL CHECK (
                blast_radius IN ('local_low','local_moderate','basin','constellation')
            ),
            lane TEXT NOT NULL CHECK (lane IN ('RESEARCH', 'PILOT', 'PRODUCTION')),
            eco_score REAL NOT NULL CHECK (eco_score >= 0.0 AND eco_score <= 1.0),
            energy_cost_score REAL NOT NULL CHECK (energy_cost_score >= 0.0 AND energy_cost_score <= 1.0),
            PRIMARY KEY (node_id, ts_utc),
            FOREIGN KEY (node_id) REFERENCES nodes(node_id) ON DELETE CASCADE
        );

        CREATE INDEX IF NOT EXISTS idx_node_risk_state_time
            ON node_risk_state (ts_utc);

        CREATE INDEX IF NOT EXISTS idx_blast_radius_summary_time
            ON blast_radius_summary (ts_utc);

        "#,
    )?;
    Ok(())
}

/// Register or update a Cyboquatic node in the index.
fn upsert_node(
    conn: &Connection,
    node_id: &str,
    repo_hint: &str,
    region: &str,
    node_type: &str,
    lane: &str,
    bostrom_did: &str,
    geo_lat: Option<f64>,
    geo_lon: Option<f64>,
) -> Result<()> {
    conn.execute(
        r#"
        INSERT INTO nodes (node_id, repo_hint, region, node_type, lane, bostrom_did, geo_lat, geo_lon)
        VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)
        ON CONFLICT(node_id) DO UPDATE SET
            repo_hint = excluded.repo_hint,
            region = excluded.region,
            node_type = excluded.node_type,
            lane = excluded.lane,
            bostrom_did = excluded.bostrom_did,
            geo_lat = excluded.geo_lat,
            geo_lon = excluded.geo_lon
        "#,
        params![node_id, repo_hint, region, node_type, lane, bostrom_did, geo_lat, geo_lon],
    )?;
    Ok(())
}

/// Insert a risk state row and derived blast-radius summary for a node.
fn insert_risk_state(
    conn: &Connection,
    node_id: &str,
    ts_utc: i64,
    plane_id: i32,
    coord: f32,
    vt: f32,
    k_knowledge: f32,
    e_ecoimpact: f32,
    r_risk_of_harm: f32,
    blast_radius: &str,
    evidence_hex: &str,
) -> Result<()> {
    conn.execute(
        r#"
        INSERT OR REPLACE INTO node_risk_state
        (node_id, ts_utc, plane_id, coord, vt, k_knowledge, e_ecoimpact, r_risk_of_harm, blast_radius, evidence_hex)
        VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)
        "#,
        params![
            node_id,
            ts_utc,
            plane_id,
            coord,
            vt,
            k_knowledge,
            e_ecoimpact,
            r_risk_of_harm,
            blast_radius,
            evidence_hex
        ],
    )?;

    conn.execute(
        r#"
        INSERT OR REPLACE INTO blast_radius_summary
        (node_id, ts_utc, vt, blast_radius, lane, eco_score, energy_cost_score)
        SELECT
            n.node_id,
            ?2 AS ts_utc,
            ?3 AS vt,
            ?4 AS blast_radius,
            n.lane,
            ?5 AS eco_score,
            ?6 AS energy_cost_score
        FROM nodes n
        WHERE n.node_id = ?1
        "#,
        params![
            node_id,
            ts_utc,
            vt,
            blast_radius,
            e_ecoimpact,
            1.0_f32 - coord
        ],
    )?;

    Ok(())
}

/// Compute an eco-impact metric summarizing a node's recent history.
fn compute_node_eco_score(conn: &Connection, node_id: &str) -> Result<Option<f32>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT AVG(eco_score)
        FROM blast_radius_summary
        WHERE node_id = ?1
        "#,
    )?;
    let avg: Option<f32> = stmt.query_row(params![node_id], |row| row.get(0)).optional()?;
    Ok(avg)
}

/// Helper to support optional() without pulling in extra crates.
trait QueryOptional<T> {
    fn optional(self) -> Result<Option<T>>;
}

impl<T> QueryOptional<T> for Result<T> {
    fn optional(self) -> Result<Option<T>> {
        match self {
            Ok(v) => Ok(Some(v)),
            Err(e) => {
                if let rusqlite::Error::QueryReturnedNoRows = e {
                    Ok(None)
                } else {
                    Err(e)
                }
            }
        }
    }
}

fn main() -> Result<()> {
    let db_path = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "econet_constellation.sqlite3".to_string());

    let needs_init = !Path::new(&db_path).exists();
    let conn = Connection::open(&db_path)?;

    if needs_init {
        init_schema(&conn)?;
    }

    // Example: register one Phoenix-class Cyboquatic MAR node.
    upsert_node(
        &conn,
        "cyboquatic_mar_phx_001",
        "github.com/Doctor0Evil/EcoNet",
        "Phoenix-CentralAZ",
        "MAR_VAULT",
        "RESEARCH",
        "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7",
        Some(33.4484),
        Some(-112.0740),
    )?;

    // Demonstration risk record, scored by ecosafety-core and routed in from Rust client.
    insert_risk_state(
        &conn,
        "cyboquatic_mar_phx_001",
        1_777_777_777,
        3,           // carbon plane
        0.20,        // rcarbon
        0.45,        // V_t
        0.94,        // K
        0.90,        // E
        0.13,        // R
        "local_low", // blast-radius
        "0xa1b2c3d4e5f6789051",
    )?;

    if let Some(score) = compute_node_eco_score(&conn, "cyboquatic_mar_phx_001")? {
        println!("Node cyboquatic_mar_phx_001 eco_score={score:.3}");
    }

    Ok(())
}
