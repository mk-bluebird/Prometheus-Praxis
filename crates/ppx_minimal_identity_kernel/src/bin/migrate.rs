// filename: ppx_minimal_identity_kernel/src/bin/migrate.rs
// repo: eco_restoration_shard/ppx_minimal_identity_kernel/src/bin/migrate.rs

use ppx_minimal_identity_kernel::migration::run_all_migrations;
use rusqlite::Connection;
use std::env;
use std::path::Path;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        eprintln!("Usage: ppx_minimal_identity_migrate <path-to-ppx_minimal_identity.sqlite3>");
        std::process::exit(1);
    }

    let db_path = &args[1];

    if let Some(parent) = Path::new(db_path).parent() {
        if !parent.exists() {
            if let Err(err) = std::fs::create_dir_all(parent) {
                eprintln!("Failed to create DB directory: {err}");
                std::process::exit(1);
            }
        }
    }

    let conn = match Connection::open(db_path) {
        Ok(c) => c,
        Err(err) => {
            eprintln!("Failed to open SQLite DB: {err}");
            std::process::exit(1);
        }
    };

    if let Err(err) = run_all_migrations(&conn) {
        eprintln!("Migration error: {err}");
        std::process::exit(1);
    }

    println!("PPX minimal identity kernel DB migrated at {db_path}");
}
