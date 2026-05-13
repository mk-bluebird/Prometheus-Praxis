// filename: eco_response_shard/src/bin/backfill.rs

use rusqlite::{params, Connection, NO_PARAMS};
use std::env;
use std::path::Path;

/// Simple struct holding a summarized shard record for eco_response_shard.
struct ShardSummary {
    shard_id: i64,
    topic_tag: String,
    user_did: String,
    node_id: Option<String>,
    asset_type: Option<String>,
    medium: Option<String>,
    region: Option<String>,
    lane: String,
    k_metric: f64,
    e_metric: f64,
    r_metric: f64,
    vt_max: f64,
    evidence_hex: Option<String>,
    // optional meta-KER from knowledgeecoscore
    k_factor: f64,
    e_factor: f64,
    r_factor: f64,
}

/// Create the local eco_response_shard tables that will store ResponseShard-style rows.
/// This DB is intentionally read-mostly and non-actuating: it mirrors governance and
/// eco-restoration evidence from the global econet-index DB.
fn create_local_schema(conn: &Connection) -> rusqlite::Result<()> {
    conn.execute_batch(
        r#"
        PRAGMA foreign_keys = ON;

        -- Core response_shard table: one row per shardinstance snapshot.
        -- This is the minimal surface needed to mathematically tie governance,
        -- Lyapunov residuals, and KER metrics to a DID and topic.
        CREATE TABLE IF NOT EXISTS response_shard (
            response_id      INTEGER PRIMARY KEY AUTOINCREMENT,
            shard_id         INTEGER NOT NULL,
            topic_tag        TEXT    NOT NULL,
            user_did         TEXT    NOT NULL,
            node_id          TEXT,
            asset_type       TEXT,
            medium           TEXT,
            region           TEXT,
            lane             TEXT    NOT NULL,
            k_metric         REAL    NOT NULL,
            e_metric         REAL    NOT NULL,
            r_metric         REAL    NOT NULL,
            vt_max           REAL    NOT NULL,
            k_factor         REAL    NOT NULL,
            e_factor         REAL    NOT NULL,
            r_factor         REAL    NOT NULL,
            residual_score   REAL    NOT NULL,
            evidence_hex     TEXT,
            created_utc      TEXT    NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_response_shard_lane
            ON response_shard(lane, k_metric, e_metric, r_metric);

        CREATE INDEX IF NOT EXISTS idx_response_shard_region
            ON response_shard(region, medium);

        CREATE INDEX IF NOT EXISTS idx_response_shard_topic
            ON response_shard(topic_tag);

        -- Optional corridor bands captured at response time, to prove that
        -- each shard was evaluated against the frozen ecosafety.corridors grammar.
        CREATE TABLE IF NOT EXISTS response_shard_corridor (
            response_id   INTEGER NOT NULL REFERENCES response_shard(response_id) ON DELETE CASCADE,
            var_id        TEXT    NOT NULL,
            safe_band_lo  REAL    NOT NULL,
            safe_band_hi  REAL    NOT NULL,
            gold_band_lo  REAL    NOT NULL,
            gold_band_hi  REAL    NOT NULL,
            hard_band_lo  REAL    NOT NULL,
            hard_band_hi  REAL    NOT NULL,
            lyap_weight   REAL    NOT NULL,
            lyap_channel  TEXT    NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_response_shard_corridor_var
            ON response_shard_corridor(var_id, lyap_channel);

        -- Simple metadata table to record which econet-index snapshot this backfill
        -- came from. This lets you prove continuity over migrations.
        CREATE TABLE IF NOT EXISTS response_backfill_meta (
            meta_id        INTEGER PRIMARY KEY AUTOINCREMENT,
            econet_db_path TEXT    NOT NULL,
            snapshot_utc   TEXT    NOT NULL,
            comment        TEXT
        );
        "#,
    )?;
    Ok(())
}

/// Read shardinstance + knowledgeecoscore from the global econet-index DB and
/// produce a stream of ShardSummary rows.
///
/// Assumptions (aligned with existing spine schema):
/// - shardinstance(shardid, nodeid, assettype, medium, region, lane,
///                 kmetric, emetric, rmetric, vtmax, evidencehex, signingdid)
/// - knowledgeecoscore(scopetype, scoperefid, kfactor, efactor, rfactor)
fn load_shard_summaries(econet_conn: &Connection) -> rusqlite::Result<Vec<ShardSummary>> {
    let mut stmt = econet_conn.prepare(
        r#"
        SELECT
            s.shardid,
            COALESCE(s.nodeid, '')              AS nodeid,
            s.assettype,
            s.medium,
            s.region,
            s.lane,
            s.kmetric,
            s.emetric,
            s.rmetric,
            s.vtmax,
            s.evidencehex,
            COALESCE(s.signingdid, '')          AS signingdid,
            COALESCE(ks.kfactor, s.kmetric)     AS kfactor,
            COALESCE(ks.efactor, s.emetric)     AS efactor,
            COALESCE(ks.rfactor, s.rmetric)     AS rfactor
        FROM shardinstance s
        LEFT JOIN knowledgeecoscore ks
          ON ks.scopetype = 'SHARD'
         AND ks.scoperefid = s.shardid;
        "#,
    )?;

    let mut rows = stmt.query(NO_PARAMS)?;
    let mut out = Vec::new();

    while let Some(row) = rows.next()? {
        let shard_id: i64 = row.get(0)?;
        let node_id: String = row.get(1)?;
        let asset_type: Option<String> = row.get(2)?;
        let medium: Option<String> = row.get(3)?;
        let region: Option<String> = row.get(4)?;
        let lane: String = row.get(5)?;
        let k_metric: f64 = row.get(6)?;
        let e_metric: f64 = row.get(7)?;
        let r_metric: f64 = row.get(8)?;
        let vt_max: f64 = row.get(9)?;
        let evidence_hex: Option<String> = row.get(10)?;
        let signing_did: String = row.get(11)?;
        let k_factor: f64 = row.get(12)?;
        let e_factor: f64 = row.get(13)?;
        let r_factor: f64 = row.get(14)?;

        // Topic tag is derived mechanically; you can tune this later.
        // For now, tie eco_response_shard to the lane + medium + region.
        let topic_tag = format!(
            "lane:{};medium:{};region:{}",
            lane,
            medium.as_deref().unwrap_or("unknown"),
            region.as_deref().unwrap_or("unknown")
        );

        let summary = ShardSummary {
            shard_id,
            topic_tag,
            user_did: if signing_did.is_empty() {
                // Fallback to primary Bostrom DID if shard has no explicit signing DID;
                // this keeps the table usable while you backfill older shards.
                "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7".to_string()
            } else {
                signing_did
            },
            node_id: if node_id.is_empty() { None } else { Some(node_id) },
            asset_type,
            medium,
            region,
            lane,
            k_metric,
            e_metric,
            r_metric,
            vt_max,
            evidence_hex,
            k_factor,
            e_factor,
            r_factor,
        };

        out.push(summary);
    }

    Ok(out)
}

/// Insert a batch of ShardSummary rows into the local response_shard table.
/// residual_score is computed as the scalar Lyapunov residual j w_j r_j^2 at
/// the shard level; here we use vt_max directly as the summary.
fn insert_response_shards(local_conn: &Connection, summaries: &[ShardSummary]) -> rusqlite::Result<()> {
    let tx = local_conn.transaction()?;
    {
        let mut stmt = tx.prepare(
            r#"
            INSERT INTO response_shard (
                shard_id,
                topic_tag,
                user_did,
                node_id,
                asset_type,
                medium,
                region,
                lane,
                k_metric,
                e_metric,
                r_metric,
                vt_max,
                k_factor,
                e_factor,
                r_factor,
                residual_score,
                evidence_hex,
                created_utc
            )
            VALUES (
                ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8,
                ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17,
                datetime('now')
            );
            "#,
        )?;

        for s in summaries {
            // For initial backfill, treat residual_score as vt_max; future
            // refinements can compute it directly from per-plane coordinates.
            let residual_score = s.vt_max;

            stmt.execute(params![
                s.shard_id,
                s.topic_tag,
                s.user_did,
                s.node_id,
                s.asset_type,
                s.medium,
                s.region,
                s.lane,
                s.k_metric,
                s.e_metric,
                s.r_metric,
                s.vt_max,
                s.k_factor,
                s.e_factor,
                s.r_factor,
                residual_score,
                s.evidence_hex
            ])?;
        }
    }
    tx.commit()?;
    Ok(())
}

/// Optionally backfill corridor bands for variables that appear in corridordefinition.
/// This makes eco_response_shard self-sufficient for mathematical proofs that
/// lanes, KER, and V_t are tied to the frozen ecosafety.corridors grammar.
fn backfill_corridors(
    econet_conn: &Connection,
    local_conn: &Connection,
) -> rusqlite::Result<()> {
    // Load corridor definitions from econet-index.
    let mut stmt = econet_conn.prepare(
        r#"
        SELECT
            varid,
            safe,
            gold,
            hard,
            weight,
            lyapchannel
        FROM corridordefinition;
        "#,
    )?;

    struct CorridorRow {
        var_id: String,
        safe: f64,
        gold: f64,
        hard: f64,
        weight: f64,
        lyap_channel: String,
    }

    let mut rows = stmt.query(NO_PARAMS)?;
    let mut corridors: Vec<CorridorRow> = Vec::new();

    while let Some(row) = rows.next()? {
        let var_id: String = row.get(0)?;
        let safe: f64 = row.get(1)?;
        let gold: f64 = row.get(2)?;
        let hard: f64 = row.get(3)?;
        let weight: f64 = row.get(4)?;
        let lyap_channel: String = row.get(5)?;

        // Here we interpret safe/gold/hard as the midpoints of their respective bands.
        // If your corridordefinition uses [safe, gold, hard] as canonical points in 0..1,
        // you can treat them as singletons; the band width is then implied.
        let band_half_width = 0.05_f64; // narrow bands; tune as needed but keep fixed.

        let row = CorridorRow {
            var_id,
            safe,
            gold,
            hard,
            weight,
            lyap_channel,
        };

        corridors.push(row);
    }

    let tx = local_conn.transaction()?;
    {
        // Attach the same corridor rows to every response_shard for now.
        // This is conservative and ensures that any proof over response_shard
        // has access to the same bands used in the spine.
        let mut resp_stmt = tx.prepare("SELECT response_id FROM response_shard;")?;
        let mut resp_rows = resp_stmt.query(NO_PARAMS)?;

        let mut insert_stmt = tx.prepare(
            r#"
            INSERT INTO response_shard_corridor (
                response_id,
                var_id,
                safe_band_lo,
                safe_band_hi,
                gold_band_lo,
                gold_band_hi,
                hard_band_lo,
                hard_band_hi,
                lyap_weight,
                lyap_channel
            )
            VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10);
            "#,
        )?;

        while let Some(r) = resp_rows.next()? {
            let response_id: i64 = r.get(0)?;
            for c in &corridors {
                let safe_lo = (c.safe - band_half_width).max(0.0);
                let safe_hi = (c.safe + band_half_width).min(1.0);
                let gold_lo = (c.gold - band_half_width).max(0.0);
                let gold_hi = (c.gold + band_half_width).min(1.0);
                let hard_lo = (c.hard - band_half_width).max(0.0);
                let hard_hi = (c.hard + band_half_width).min(1.0);

                insert_stmt.execute(params![
                    response_id,
                    c.var_id,
                    safe_lo,
                    safe_hi,
                    gold_lo,
                    gold_hi,
                    hard_lo,
                    hard_hi,
                    c.weight,
                    c.lyap_channel
                ])?;
            }
        }
    }
    tx.commit()?;
    Ok(())
}

fn main() -> rusqlite::Result<()> {
    let args: Vec<String> = env::args().collect();
    if args.len() != 3 {
        eprintln!(
            "Usage: {} <econet_index_db_path> <eco_response_shard_db_path>",
            args[0]
        );
        std::process::exit(1);
    }

    let econet_path = &args[1];
    let response_path = &args[2];

    if !Path::new(econet_path).exists() {
        eprintln!("econet-index DB not found at path: {}", econet_path);
        std::process::exit(1);
    }

    let econet_conn = Connection::open(econet_path)?;
    let response_conn = Connection::open(response_path)?;

    create_local_schema(&response_conn)?;

    let summaries = load_shard_summaries(&econet_conn)?;
    if summaries.is_empty() {
        eprintln!("No shardinstance rows found in econet-index DB; nothing to backfill.");
        return Ok(());
    }

    insert_response_shards(&response_conn, &summaries)?;
    backfill_corridors(&econet_conn, &response_conn)?;

    // Record meta row so later proofs can show which econet-index snapshot
    // this backfill originated from.
    response_conn.execute(
        r#"
        INSERT INTO response_backfill_meta (econet_db_path, snapshot_utc, comment)
        VALUES (?1, datetime('now'), 'Initial backfill from econet-index into eco_response_shard');
        "#,
        params![econet_path],
    )?;

    Ok(())
}
