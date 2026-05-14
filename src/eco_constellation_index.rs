// filename src/eco_constellation_index.rs
// destination eco_restoration_shard/src/eco_constellation_index.rs

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

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DeprecatedRepoWithSuccessor {
    pub repoid: i64,
    pub name: String,
    pub githubslug: String,
    pub terminal_repoid: i64,
    pub terminal_name: String,
    pub terminal_githubslug: String,
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

    pub fn list_deprecated_repos_with_successors(
        &self,
    ) -> SqlResult<Vec<DeprecatedRepoWithSuccessor>> {
        let mut stmt = self.conn.prepare(
            "SELECT repoid, name, githubslug, visibility, roleband, successor_repo
             FROM repo
             WHERE successor_repo IS NOT NULL",
        )?;

        let mut rows = stmt.query([])?;
        let mut results = Vec::new();

        while let Some(row) = rows.next()? {
            let repo = map_repo_row(&row)?;
            if let Some(successor_id) = repo.successor_repo {
                if let Some(terminal) = self.resolve_successor_chain(successor_id)? {
                    results.push(DeprecatedRepoWithSuccessor {
                        repoid: repo.repoid,
                        name: repo.name.clone(),
                        githubslug: repo.githubslug.clone(),
                        terminal_repoid: terminal.repoid,
                        terminal_name: terminal.name,
                        terminal_githubslug: terminal.githubslug,
                    });
                }
            }
        }

        Ok(results)
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
