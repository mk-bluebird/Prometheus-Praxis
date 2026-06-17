// filename: src/replay_lane_promotion_phx.rs
// destination: eco_restoration_shard/src/replay_lane_promotion_phx.rs
// repo-target: github.com/mk-bluebird/eco_restoration_shard

use rusqlite::{params, Connection, Result};
use chrono::{DateTime, Utc};

#[derive(Debug, Clone)]
pub struct ReplayConfig {
    pub region: String,
    pub lane_from: String,
    pub lane_to: String,
    pub snapshot_old_tag: String,
    pub snapshot_new_tag: String,
    pub eps_k: f64,
    pub eps_e: f64,
    pub eps_r: f64,
    pub eps_vt: f64,
    pub author_bostrom: String,
    pub author_contract: String,
    pub routingspec_hex: String,
}

fn now_utc_iso() -> String {
    let now: DateTime<Utc> = Utc::now();
    now.to_rfc3339()
}

pub fn replay_lane_promotion(conn: &Connection, cfg: &ReplayConfig) -> Result<i64> {
    conn.execute("BEGIN IMMEDIATE", [])?;

    let started = now_utc_iso();
    conn.execute(
        "INSERT INTO lane_promotion_replay_run_phx (
             region,lane_from,lane_to,
             snapshot_old_tag,snapshot_new_tag,
             eps_k,eps_e,eps_r,eps_vt,
             started_utc,status,
             author_bostrom,author_contract,routingspec_hex
         ) VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,'RUNNING',?11,?12,?13)",
        params![
            cfg.region,
            cfg.lane_from,
            cfg.lane_to,
            cfg.snapshot_old_tag,
            cfg.snapshot_new_tag,
            cfg.eps_k,
            cfg.eps_e,
            cfg.eps_r,
            cfg.eps_vt,
            started,
            cfg.author_bostrom,
            cfg.author_contract,
            cfg.routingspec_hex
        ],
    )?;
    let run_id = conn.last_insert_rowid();

    let mut stmt = conn.prepare(
        r#"
        SELECT
          a.shardid,
          a.tsendutc,
          a.kscore      AS k_old,
          b.kscore      AS k_new,
          a.escore      AS e_old,
          b.escore      AS e_new,
          a.rscore      AS r_old,
          b.rscore      AS r_new,
          a.vt_topology AS vt_old,
          b.vt_topology AS vt_new,
          COALESCE(a.violation_flag,0) AS v_old,
          COALESCE(b.violation_flag,0) AS v_new
        FROM vshardker_snapshot AS a
        JOIN vshardker_snapshot AS b
          ON a.shardid   = b.shardid
         AND a.tsendutc  = b.tsendutc
        WHERE a.snapshot_tag = ?1
          AND b.snapshot_tag = ?2
          AND a.region       = ?3
        "#,
    )?;

    let rows = stmt.query_map(
        params![cfg.snapshot_old_tag, cfg.snapshot_new_tag, cfg.region],
        |row| {
            Ok((
                row.get::<_, String>(0)?,
                row.get::<_, String>(1)?,
                row.get::<_, f64>(2)?,
                row.get::<_, f64>(3)?,
                row.get::<_, f64>(4)?,
                row.get::<_, f64>(5)?,
                row.get::<_, f64>(6)?,
                row.get::<_, f64>(7)?,
                row.get::<_, f64>(8)?,
                row.get::<_, f64>(9)?,
                row.get::<_, i64>(10)?,
                row.get::<_, i64>(11)?,
            ))
        },
    )?;

    let created_utc = now_utc_iso();
    let mut any_violation = false;

    for r in rows {
        let (
            shard_id,
            ts_end_utc,
            k_old,
            k_new,
            e_old,
            e_new,
            r_old,
            r_new,
            vt_old,
            vt_new,
            v_old,
            v_new,
        ) = r?;

        let mut k_violation = 0;
        let mut e_violation = 0;
        let mut r_violation = 0;
        let mut v_violation = 0;

        if k_new + cfg.eps_k < k_old {
            k_violation = 1;
            any_violation = true;
            conn.execute(
                "INSERT INTO lane_promotion_replay_violation_phx (
                     run_id,shard_id,ts_end_utc,violation_type,
                     k_old,k_new,created_utc,author_bostrom,author_contract
                 ) VALUES (?1,?2,?3,'K_REGRESSION',?4,?5,?6,?7,?8)",
                params![
                    run_id,
                    shard_id,
                    ts_end_utc.clone(),
                    k_old,
                    k_new,
                    created_utc,
                    cfg.author_bostrom,
                    cfg.author_contract
                ],
            )?;
        }

        if e_new + cfg.eps_e < e_old {
            e_violation = 1;
            any_violation = true;
            conn.execute(
                "INSERT INTO lane_promotion_replay_violation_phx (
                     run_id,shard_id,ts_end_utc,violation_type,
                     e_old,e_new,created_utc,author_bostrom,author_contract
                 ) VALUES (?1,?2,?3,'E_REGRESSION',?4,?5,?6,?7,?8)",
                params![
                    run_id,
                    shard_id,
                    ts_end_utc.clone(),
                    e_old,
                    e_new,
                    created_utc,
                    cfg.author_bostrom,
                    cfg.author_contract
                ],
            )?;
        }

        if r_new > r_old + cfg.eps_r {
            r_violation = 1;
            any_violation = true;
            conn.execute(
                "INSERT INTO lane_promotion_replay_violation_phx (
                     run_id,shard_id,ts_end_utc,violation_type,
                     r_old,r_new,created_utc,author_bostrom,author_contract
                 ) VALUES (?1,?2,?3,'R_INCREASE',?4,?5,?6,?7,?8)",
                params![
                    run_id,
                    shard_id,
                    ts_end_utc.clone(),
                    r_old,
                    r_new,
                    created_utc,
                    cfg.author_bostrom,
                    cfg.author_contract
                ],
            )?;
        }

        if vt_new + cfg.eps_vt < vt_old || (v_old == 0 && v_new != 0) {
            v_violation = 1;
            any_violation = true;
            let vtype = if vt_new + cfg.eps_vt < vt_old {
                "VT_REGRESSION"
            } else {
                "NEW_VIOLATION_ROW"
            };
            conn.execute(
                "INSERT INTO lane_promotion_replay_violation_phx (
                     run_id,shard_id,ts_end_utc,violation_type,
                     vt_old,vt_new,created_utc,author_bostrom,author_contract
                 ) VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9)",
                params![
                    run_id,
                    shard_id,
                    ts_end_utc.clone(),
                    vtype,
                    vt_old,
                    vt_new,
                    created_utc,
                    cfg.author_bostrom,
                    cfg.author_contract
                ],
            )?;
        }

        conn.execute(
            "INSERT INTO lane_promotion_replay_result_phx (
                 run_id,shard_id,ts_end_utc,
                 k_old,k_new,e_old,e_new,r_old,r_new,vt_old,vt_new,
                 v_violation,k_violation,e_violation,r_violation
             ) VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15)",
            params![
                run_id,
                shard_id,
                ts_end_utc,
                k_old,
                k_new,
                e_old,
                e_new,
                r_old,
                r_new,
                vt_old,
                vt_new,
                v_violation,
                k_violation,
                e_violation,
                r_violation
            ],
        )?;
    }

    let finished = now_utc_iso();
    let status = if any_violation { "FAILED" } else { "SUCCESS" };

    conn.execute(
        "UPDATE lane_promotion_replay_run_phx
         SET finished_utc = ?1, status = ?2
         WHERE run_id = ?3",
        params![finished, status, run_id],
    )?;

    conn.execute("COMMIT", [])?;
    Ok(run_id)
}
