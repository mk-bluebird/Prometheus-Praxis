// filename src/ecoconstellation_index.rs
// destination eco_restoration_shard/src/ecoconstellation_index.rs

use std::collections::HashSet;

use rusqlite::{params, Connection, Row, Result as SqlResult};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RepoRecord {
    pub repoid: i64,
    pub name: String,
    pub githubslug: String,
    pub visibility: String,
    pub roleband: String,
    pub successor_repo: Option<i64>,
}

pub struct EcoConstellationIndex {
    conn: Connection,
}

impl EcoConstellationIndex {
    pub fn new(conn: Connection) -> Self {
        EcoConstellationIndex { conn }
    }

    pub fn load_repo_by_id(&self, repoid: i64) -> SqlResult<Option<RepoRecord>> {
        let mut stmt = self.conn.prepare(
            "SELECT repoid, name, githubslug, visibility, roleband, successor_repo
             FROM repo
             WHERE repoid = ?1",
        )?;
        let mut rows = stmt.query(params![repoid])?;
        if let Some(row) = rows.next()? {
            Ok(Some(map_repo_row(&row)?))
        } else {
            Ok(None)
        }
    }

    pub fn resolve_successor_chain(&self, start_repoid: i64) -> SqlResult<Option<RepoRecord>> {
        let mut current = start_repoid;
        let mut visited = HashSet::new();

        loop {
            if !visited.insert(current) {
                return Ok(None);
            }

            let repo_opt = self.load_repo_by_id(current)?;
            let repo = match repo_opt {
                Some(r) => r,
                None => return Ok(None),
            };

            match repo.successor_repo {
                Some(next_id) => {
                    current = next_id;
                }
                None => {
                    return Ok(Some(repo));
                }
            }
        }
    }
}

fn map_repo_row(row: &Row<'_>) -> SqlResult<RepoRecord> {
    Ok(RepoRecord {
        repoid: row.get(0)?,
        name: row.get(1)?,
        githubslug: row.get(2)?,
        visibility: row.get(3)?,
        roleband: row.get(4)?,
        successor_repo: row.get(5)?,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use rusqlite::Connection;

    fn setup_in_memory_db() -> Connection {
        let conn = Connection::open_in_memory().expect("in-memory DB");
        conn.execute_batch(
            "PRAGMA foreign_keys = ON;
             CREATE TABLE repo (
                 repoid        INTEGER PRIMARY KEY,
                 name          TEXT NOT NULL,
                 githubslug    TEXT NOT NULL,
                 visibility    TEXT NOT NULL,
                 roleband      TEXT NOT NULL,
                 successor_repo INTEGER
             );",
        )
        .expect("create repo table");
        conn
    }

    #[test]
    fn resolve_successor_chain_simple_chain() {
        let conn = setup_in_memory_db();

        conn.execute(
            "INSERT INTO repo (repoid, name, githubslug, visibility, roleband, successor_repo)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6);",
            params![1_i64, "A", "Doctor0Evil/A", "Public", "RESEARCH", Some(2_i64)],
        )
        .unwrap();

        conn.execute(
            "INSERT INTO repo (repoid, name, githubslug, visibility, roleband, successor_repo)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6);",
            params![2_i64, "B", "Doctor0Evil/B", "Public", "RESEARCH", None::<i64>],
        )
        .unwrap();

        let index = EcoConstellationIndex::new(conn);

        let terminal = index.resolve_successor_chain(1).unwrap();
        assert!(terminal.is_some());
        let repo = terminal.unwrap();
        assert_eq!(repo.repoid, 2);
        assert_eq!(repo.name, "B");
    }

    #[test]
    fn resolve_successor_chain_cycle_three_nodes_detected() {
        let conn = setup_in_memory_db();

        conn.execute(
            "INSERT INTO repo (repoid, name, githubslug, visibility, roleband, successor_repo)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6);",
            params![1_i64, "A", "Doctor0Evil/A", "Public", "RESEARCH", Some(2_i64)],
        )
        .unwrap();

        conn.execute(
            "INSERT INTO repo (repoid, name, githubslug, visibility, roleband, successor_repo)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6);",
            params![2_i64, "B", "Doctor0Evil/B", "Public", "RESEARCH", Some(3_i64)],
        )
        .unwrap();

        conn.execute(
            "INSERT INTO repo (repoid, name, githubslug, visibility, roleband, successor_repo)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6);",
            params![3_i64, "C", "Doctor0Evil/C", "Public", "RESEARCH", Some(1_i64)],
        )
        .unwrap();

        let index = EcoConstellationIndex::new(conn);

        let from_a = index.resolve_successor_chain(1).unwrap();
        assert!(from_a.is_none());

        let from_b = index.resolve_successor_chain(2).unwrap();
        assert!(from_b.is_none());

        let from_c = index.resolve_successor_chain(3).unwrap();
        assert!(from_c.is_none());
    }
}
