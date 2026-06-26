// filename: eco_restoration_index/src/bin/migrate.rs

use eco_restoration_index::migration::run_all_migrations;
use rusqlite::Connection;
use std::env;
use std::path::Path;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        eprintln!("Usage: eco_restoration_migrate <path-to-eco_restoration_index.sqlite3>");
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

    println!("eco_restoration_index migrations applied successfully at {db_path}");
}
