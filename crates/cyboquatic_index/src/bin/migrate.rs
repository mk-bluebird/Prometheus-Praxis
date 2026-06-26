// filename: cyboquatic_index/src/bin/migrate.rs
// destination: eco_restoration_shard/cyboquatic_index/src/bin/migrate.rs

use cyboquatic_index::migration::run_all_migrations;
use rusqlite::Connection;
use std::env;
use std::path::Path;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        eprintln!("Usage: cyboquatic_migrate <path-to-db_cyboquatic_machinery_index.sqlite3>");
        std::process::exit(1);
    }
    let db_path = &args[1];

    if let Some(parent) = Path::new(db_path).parent() {
        if !parent.exists() {
            if let Err(e) = std::fs::create_dir_all(parent) {
                eprintln!("Failed to create directory: {e}");
                std::process::exit(1);
            }
        }
    }

    let conn = match Connection::open(db_path) {
        Ok(c) => c,
        Err(e) => {
            eprintln!("Failed to open DB: {e}");
            std::process::exit(1);
        }
    };

    if let Err(e) = run_all_migrations(&conn) {
        eprintln!("Migration error: {e}");
        std::process::exit(1);
    }

    println!("Cyboquatic machinery index migrated at {db_path}");
}
