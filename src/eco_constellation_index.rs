// filename src/eco_constellation_index.rs
// destination eco_restoration_shard/src/eco_constellation_index.rs

use std::collections::{HashMap, HashSet};

use rusqlite::{params, Connection, Row, Result as SqlResult};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value as JsonValue};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RepoRecord {
    pub repoid: i64,
    pub name: String,
    pub githubslug: String,
    pub visibility: String,
    pub languageprimary: String,
    pub roleband: String,
    pub description: Option<String>,
    pub lastupdatedutc: Option<String>,
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

    fn map_full_repo_row(row: &Row<'_>) -> SqlResult<RepoRecord> {
        Ok(RepoRecord {
            repoid: row.get(0)?,
            name: row.get(1)?,
            githubslug: row.get(2)?,
            visibility: row.get(3)?,
            languageprimary: row.get(4)?,
            roleband: row.get(5)?,
            description: row.get(6)?,
            lastupdatedutc: row.get(7)?,
            successor_repo: row.get(8)?,
        })
    }

    fn map_min_repo_row(row: &Row<'_>) -> SqlResult<RepoRecord> {
        Ok(RepoRecord {
            repoid: row.get(0)?,
            name: row.get(1)?,
            githubslug: row.get(2)?,
            visibility: row.get(3)?,
            languageprimary: String::from(""),
            roleband: row.get(4)?,
            description: None,
            lastupdatedutc: None,
            successor_repo: row.get(5)?,
        })
    }

    pub fn load_repo_by_id(&self, repoid: i64) -> SqlResult<Option<RepoRecord>> {
        let mut stmt = self.conn.prepare(
            "SELECT repoid,
                    name,
                    githubslug,
                    visibility,
                    languageprimary,
                    roleband,
                    description,
                    lastupdatedutc,
                    successor_repo
             FROM repo
             WHERE repoid = ?1",
        )?;
        let mut rows = stmt.query(params![repoid])?;
        if let Some(row) = rows.next()? {
            Ok(Some(Self::map_full_repo_row(&row)?))
        } else {
            Ok(None)
        }
    }

    pub fn load_repos(&self) -> SqlResult<Vec<RepoRecord>> {
        let mut stmt = self.conn.prepare(
            "SELECT repoid,
                    name,
                    githubslug,
                    visibility,
                    languageprimary,
                    roleband,
                    description,
                    lastupdatedutc,
                    successor_repo
             FROM repo",
        )?;

        let rows = stmt.query_map([], |row| Self::map_full_repo_row(row))?;

        let mut repos = Vec::new();
        for r in rows {
            repos.push(r?);
        }
        Ok(repos)
    }

    fn resolve_successor_chain_ids(
        &self,
        repos_by_id: &HashMap<i64, RepoRecord>,
        start_id: i64,
    ) -> Vec<i64> {
        let mut chain = Vec::new();
        let mut current = Some(start_id);
        let mut visited = HashSet::new();

        while let Some(id) = current {
            if !visited.insert(id) {
                break;
            }
            chain.push(id);
            current = repos_by_id.get(&id).and_then(|r| r.successor_repo);
        }

        chain
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
            "SELECT repoid,
                    name,
                    githubslug,
                    visibility,
                    roleband,
                    successor_repo
             FROM repo
             WHERE successor_repo IS NOT NULL",
        )?;

        let mut rows = stmt.query([])?;
        let mut results = Vec::new();

        while let Some(row) = rows.next()? {
            let repo = Self::map_min_repo_row(&row)?;
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

    pub fn export_index_json(&self) -> SqlResult<JsonValue> {
        let repos = self.load_repos()?;

        let mut repos_by_id = HashMap::new();
        for r in repos.iter().cloned() {
            repos_by_id.insert(r.repoid, r);
        }

        let mut entries = Vec::new();

        for repo in repos_by_id.values() {
            let chain_ids = self.resolve_successor_chain_ids(&repos_by_id, repo.repoid);

            let chain: Vec<JsonValue> = chain_ids
                .iter()
                .filter_map(|id| repos_by_id.get(id))
                .map(|r| {
                    json!({
                        "repoid": r.repoid,
                        "name": r.name,
                        "githubslug": r.githubslug,
                        "roleband": r.roleband
                    })
                })
                .collect();

            let entry = json!({
                "repoid": repo.repoid,
                "name": repo.name,
                "githubslug": repo.githubslug,
                "visibility": repo.visibility,
                "languageprimary": repo.languageprimary,
                "roleband": repo.roleband,
                "description": repo.description,
                "lastupdatedutc": repo.lastupdatedutc,
                "successor_chain": chain
            });

            entries.push(entry);
        }

        Ok(json!({
            "version": "2026-01-01",
            "repos": entries
        }))
    }
}
