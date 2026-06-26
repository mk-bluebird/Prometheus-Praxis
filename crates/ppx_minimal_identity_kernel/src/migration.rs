// filename: ppx_minimal_identity_kernel/src/migration.rs
// repo: eco_restoration_shard/ppx_minimal_identity_kernel/src/migration.rs

use rusqlite::{Connection, Result as SqlResult};

pub fn run_all_migrations(conn: &Connection) -> SqlResult<()> {
    conn.execute_batch(include_str!("../sql/ppx_minimal_continuity_neurorights.sql"))?;
    Ok(())
}
