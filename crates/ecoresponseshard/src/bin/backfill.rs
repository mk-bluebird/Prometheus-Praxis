// FILE: crates/ecoresponseshard/src/bin/backfill.rs
// DESTINATION: crates/ecoresponseshard/src/bin/backfill.rs
// REPO-TARGET: github.com/mk-bluebird/eco_restoration_shard
//
// Reads rows from a source governance DB and back-fills
// `response_shard_snapshot` in the target DB.  Both paths are
// supplied as CLI arguments; the target DB is opened read-write
// only for this migration; after this binary completes the target
// is used read-only by the library.

#![forbid(unsafe_code)]

use std::env;
use std::process::ExitCode;

use ecoresponseshard::ensure_schema;
use rusqlite::{Connection, OpenFlags};

fn main() -> ExitCode {
    let args: Vec<String> = env::args().collect();
    if args.len() != 3 {
        eprintln!(
            "usage: eco_response_backfill <source_governance_db> <target_response_db>"
        );
        return ExitCode::from(1);
    }

    let source_path = &args[1];
    let target_path = &args[2];

    if let Err(e) = ensure_schema(target_path) {
        eprintln!("schema init failed: {e}");
        return ExitCode::from(1);
    }

    let source = match Connection::open_with_flags(
        source_path,
        OpenFlags::SQLITE_OPEN_READ_ONLY | OpenFlags::SQLITE_OPEN_URI,
    ) {
        Ok(c) => c,
        Err(e) => {
            eprintln!("cannot open source db: {e}");
            return ExitCode::from(1);
        }
    };

    let target = match Connection::open(target_path) {
        Ok(c) => c,
        Err(e) => {
            eprintln!("cannot open target db: {e}");
            return ExitCode::from(1);
        }
    };

    // Source query: pull shardinstance rows with KER and lane data.
    // Column names match the canonical ecorestorationshard shardinstance schema.
    let select_sql = r#"
        SELECT
            si.shardid,
            COALESCE(si.ownerdid, 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'),
            COALESCE(si.region,   'Phoenix-AZ-US'),
            COALESCE(si.lane,     'RESEARCH'),
            CASE
                WHEN si.kmetric >= 0.90 AND si.rmetric <= 0.14
                THEN 'HIGH'
                ELSE 'STANDARD'
            END                         AS trust_tier,
            COALESCE(si.kmetric,  0.0)  AS k_factor,
            COALESCE(si.emetric,  0.0)  AS e_factor,
            COALESCE(si.rmetric,  1.0)  AS r_factor,
            COALESCE(si.vtmax,    0.0)  AS vt_residual,
            COALESCE(si.kerdeployable, 0) AS kerdeployable,
            COALESCE(si.windowstartutc, datetime('now','-1 day')),
            COALESCE(si.windowendutc,   datetime('now')),
            COALESCE(si.evidencehex,    '0x0000000000000000')
        FROM shardinstance AS si
    "#;

    let mut src_stmt = match source.prepare(select_sql) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("source prepare failed: {e}");
            return ExitCode::from(1);
        }
    };

    let insert_sql = r#"
        INSERT OR IGNORE INTO response_shard_snapshot (
            shard_id, owner_did, region, lane, trust_tier,
            k_factor, e_factor, r_factor, vt_residual,
            kerdeployable, window_start_utc, window_end_utc,
            evidence_hex, created_utc
        ) VALUES (
            ?1, ?2, ?3, ?4, ?5,
            ?6, ?7, ?8, ?9,
            ?10, ?11, ?12,
            ?13, strftime('%Y-%m-%dT%H:%M:%SZ','now')
        )
    "#;

    let mut inserted: u64 = 0;

    let result: Result<(), rusqlite::Error> = (|| {
        let mut rows = src_stmt.query([])?;
        while let Some(row) = rows.next()? {
            let shard_id:     String = row.get(0)?;
            let owner_did:    String = row.get(1)?;
            let region:       String = row.get(2)?;
            let lane:         String = row.get(3)?;
            let trust_tier:   String = row.get(4)?;
            let k_factor:     f64    = row.get(5)?;
            let e_factor:     f64    = row.get(6)?;
            let r_factor:     f64    = row.get(7)?;
            let vt_residual:  f64    = row.get(8)?;
            let kerdeployable: i64   = row.get(9)?;
            let ws:           String = row.get(10)?;
            let we:           String = row.get(11)?;
            let ev_hex:       String = row.get(12)?;

            target.execute(
                insert_sql,
                rusqlite::params![
                    shard_id, owner_did, region, lane, trust_tier,
                    k_factor, e_factor, r_factor, vt_residual,
                    kerdeployable, ws, we, ev_hex
                ],
            )?;
            inserted += 1;
        }
        Ok(())
    })();

    if let Err(e) = result {
        eprintln!("backfill error: {e}");
        return ExitCode::from(1);
    }

    println!("backfill complete: {inserted} rows inserted");
    ExitCode::SUCCESS
}
