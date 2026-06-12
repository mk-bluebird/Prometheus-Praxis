// filename: crates/lake_risk_init/src/main.rs
// edition: 2021 or 2024 in Cargo.toml (Rust 1.85 compatible)

use std::fs;
use std::path::Path;
use rusqlite::Connection;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let db_path = Path::new("data").join("lake_risk.db");
    let sql_path = Path::new("data").join("lake_risk_az_co_system.sql");

    if !sql_path.exists() {
        return Err(format!("SQL seed file not found at {:?}", sql_path).into());
    }

    if let Some(parent) = db_path.parent() {
        fs::create_dir_all(parent)?;
    }

    let conn = Connection::open(&db_path)?;
    let sql = fs::read_to_string(&sql_path)?;
    conn.execute_batch(&sql)?;

    println!("Initialized lake_risk.db from {:?}", sql_path);
    Ok(())
}
