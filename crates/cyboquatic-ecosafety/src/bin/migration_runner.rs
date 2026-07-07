// filename: cyboquatic-ecosafety/src/bin/migration_runner.rs
// destination: cyboquatic-ecosafety/src/bin/migration_runner.rs

#![forbid(unsafe_code)]

use std::env;
use std::process::ExitCode;

use rusqlite::{Connection, NO_PARAMS};

use cyboquatic_ecosafety::shard_store::CyboNodeEcosafetyEnvelope;

fn main() -> ExitCode {
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        eprintln!("usage: migration_runner <sqlite_path>");
        return ExitCode::from(1);
    }

    let path = &args[1];
    match run_migrations_and_assert(path) {
        Ok(_) => ExitCode::from(0),
        Err(e) => {
            eprintln!("migration_runner error: {e}");
            ExitCode::from(1)
        }
    }
}

fn run_migrations_and_assert(path: &str) -> rusqlite::Result<()> {
    let conn = Connection::open(path)?;

    // Apply ecosafety migrations idempotently.
    // These should mirror your Phoenix migration files.
    conn.execute_batch(
        "CREATE TABLE IF NOT EXISTS cybo_node_ecosafety_envelope (
             node_id TEXT NOT NULL,
             window_start_utc TEXT NOT NULL,
             window_end_utc TEXT NOT NULL,
             lane TEXT NOT NULL,
             k REAL NOT NULL,
             e REAL NOT NULL,
             r REAL NOT NULL,
             vt REAL NOT NULL,
             roh REAL NOT NULL,
             ecosafety_state TEXT NOT NULL,
             evidence_hex TEXT NOT NULL,
             signing_hex TEXT NOT NULL
         );",
    )?;

    assert_schema_matches_envelope(&conn)?;
    Ok(())
}

fn assert_schema_matches_envelope(conn: &Connection) -> rusqlite::Result<()> {
    let mut stmt = conn.prepare(
        "PRAGMA table_info('cybo_node_ecosafety_envelope');",
    )?;

    #[derive(Debug)]
    struct ColInfo {
        name: String,
        ty: String,
        notnull: bool,
    }

    let cols_iter = stmt.query_map(NO_PARAMS, |row| {
        Ok(ColInfo {
            name: row.get(1)?,
            ty: row.get(2)?,
            notnull: row.get::<_, i32>(3)? != 0,
        })
    })?;

    let mut cols: Vec<ColInfo> = Vec::new();
    for c in cols_iter {
        cols.push(c?);
    }

    // Expected schema derived from CyboNodeEcosafetyEnvelope.
    let expected: [(&str, &str, bool); 12] = [
        ("node_id", "TEXT", true),
        ("window_start_utc", "TEXT", true),
        ("window_end_utc", "TEXT", true),
        ("lane", "TEXT", true),
        ("k", "REAL", true),
        ("e", "REAL", true),
        ("r", "REAL", true),
        ("vt", "REAL", true),
        ("roh", "REAL", true),
        ("ecosafety_state", "TEXT", true),
        ("evidence_hex", "TEXT", true),
        ("signing_hex", "TEXT", true),
    ];

    if cols.len() != expected.len() {
        return Err(rusqlite::Error::InvalidQuery);
    }

    for (i, (name, ty, notnull)) in expected.iter().enumerate() {
        let c = &cols[i];
        if c.name != *name || c.ty.to_uppercase() != *ty || c.notnull != *notnull {
            return Err(rusqlite::Error::InvalidQuery);
        }
    }

    Ok(())
}
