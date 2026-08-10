// File: eco_restoration_shard_core/src/materialized_refresh.rs
use rusqlite::{Connection, Result};
use std::{
    path::Path,
    sync::{atomic::{AtomicBool, Ordering}, Arc},
    thread,
    time::Duration,
};

const REFRESH_SQL: &str = "
BEGIN IMMEDIATE;
DELETE FROM m_cyboquatic_workload_admissible;
INSERT INTO m_cyboquatic_workload_admissible (
  frame_id, observed_utc, node_id, canal_node, energyreq_j, delta_vt,
  knowledge_factor, eco_impact_value, ker_k, ker_e, ker_r, fog_confidence, refreshed_utc
)
SELECT frame_id, observed_utc, node_id, canal_node, energyreq_j, delta_vt,
  knowledge_factor, eco_impact_value, ker_k, ker_e, ker_r, fog_confidence,
  strftime('%Y-%m-%dT%H:%M:%SZ','now')
FROM v_cyboquatic_workload_admissible;
COMMIT;";

pub fn refresh(connection: &mut Connection) -> Result<usize> {
    connection.execute_batch(REFRESH_SQL)?;
    connection.query_row(
        "SELECT COUNT(*) FROM m_cyboquatic_workload_admissible",
        [],
        |row| row.get(0),
    )
}

pub fn run_periodically(
    database_path: impl AsRef<Path>,
    interval: Duration,
    stop: Arc<AtomicBool>,
) -> thread::JoinHandle<()> {
    let path = database_path.as_ref().to_owned();
    thread::spawn(move || {
        let mut connection = match Connection::open(path) {
            Ok(connection) => connection,
            Err(_) => return,
        };
        if connection.busy_timeout(Duration::from_secs(5)).is_err() {
            return;
        }
        while !stop.load(Ordering::Acquire) {
            let _ = refresh(&mut connection);
            let mut elapsed = Duration::ZERO;
            while elapsed < interval && !stop.load(Ordering::Acquire) {
                let step = Duration::from_millis(250).min(interval - elapsed);
                thread::sleep(step);
                elapsed += step;
            }
        }
    })
}
