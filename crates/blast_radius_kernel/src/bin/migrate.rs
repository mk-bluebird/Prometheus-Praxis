// filename: migrate.rs
// destination: ecorestoration_shard/blast_radius_kernel/src/bin/migrate.rs

use std::env;
use std::fs;
use std::path::Path;

use rusqlite::Connection;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        eprintln!("Usage: blast_radius_migrate <path-to-sqlite-db>");
        std::process::exit(1);
    }

    let db_path = &args[1];
    let db_dir = Path::new(db_path)
        .parent()
        .unwrap_or_else(|| Path::new("."));

    if !db_dir.exists() {
        if let Err(err) = fs::create_dir_all(db_dir) {
            eprintln!("Failed to create directory {}: {}", db_dir.display(), err);
            std::process::exit(1);
        }
    }

    let conn = match Connection::open(db_path) {
        Ok(c) => c,
        Err(err) => {
            eprintln!("Failed to open SQLite DB at {}: {}", db_path, err);
            std::process::exit(1);
        }
    };

    if let Err(err) = run_all_migrations(&conn) {
        eprintln!("Migration failed: {}", err);
        std::process::exit(1);
    }

    println!("blast_radius_kernel migration completed for {}", db_path);
}

fn run_all_migrations(conn: &Connection) -> rusqlite::Result<()> {
    let sql = include_str!("../../db/db_blast_radius_kernel.sql");
    conn.execute_batch(sql)?;
    Ok(())
}
