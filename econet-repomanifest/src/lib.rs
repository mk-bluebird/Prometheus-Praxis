// filename: econet-repomanifest/src/lib.rs
// Purpose: Load .econet/econet_repoindex.sql into an in-memory SQLite DB
//          and expose a RepoManifest for Rust and FFI clients. Non-actuating.

use rusqlite::{Connection, NO_PARAMS};
use serde::{Deserialize, Serialize};
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RepoIndex {
    pub reponame: String,
    pub githubslug: String,
    pub roleband: String,
    pub visibility: String,
    pub languageprimary: String,
    pub description: Option<String>,
    pub ecosafetybinding: String,
    pub shardprotocol: String,
    pub lanedefault: String,
    pub kertargetk: f64,
    pub kertargete: f64,
    pub kertargetr: f64,
    pub nonactuatingonly: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Layer {
    pub layername: String,
    pub layertier: String,
    pub languages: String,
    pub description: Option<String>,
    pub contracts: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RoleHint {
    pub key: String,
    pub value: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RepoManifest {
    pub index: RepoIndex,
    pub layers: Vec<Layer>,
    pub hints: Vec<RoleHint>,
}

fn load_sql_from_repo(repo_root: &Path) -> std::io::Result<String> {
    let shard_path: PathBuf = repo_root.join(".econet").join("econet_repoindex.sql");
    fs::read_to_string(shard_path)
}

pub fn load_manifest_from_repo(repo_root: &Path) -> anyhow::Result<RepoManifest> {
    let sql = load_sql_from_repo(repo_root)?;
    let conn = Connection::open_in_memory()?;
    conn.execute_batch(&sql)?;

    let index = conn.query_row(
        "SELECT reponame, githubslug, roleband, visibility, languageprimary,
                description, ecosafetybinding, shardprotocol, lanedefault,
                kertargetk, kertargete, kertargetr, nonactuatingonly
         FROM econetrepoindex
         LIMIT 1;",
        NO_PARAMS,
        |row| {
            Ok(RepoIndex {
                reponame: row.get(0)?,
                githubslug: row.get(1)?,
                roleband: row.get(2)?,
                visibility: row.get(3)?,
                languageprimary: row.get(4)?,
                description: row.get(5)?,
                ecosafetybinding: row.get(6)?,
                shardprotocol: row.get(7)?,
                lanedefault: row.get(8)?,
                kertargetk: row.get(9)?,
                kertargete: row.get(10)?,
                kertargetr: row.get(11)?,
                nonactuatingonly: {
                    let v: i64 = row.get(12)?;
                    v != 0
                },
            })
        },
    )?;

    let mut layers_stmt = conn.prepare(
        "SELECT layername, layertier, languages, description, contracts
         FROM econetlayer
         WHERE reponame = ?1;",
    )?;
    let layers_iter = layers_stmt.query_map(&[&index.reponame], |row| {
        Ok(Layer {
            layername: row.get(0)?,
            layertier: row.get(1)?,
            languages: row.get(2)?,
            description: row.get(3)?,
            contracts: row.get(4)?,
        })
    })?;

    let mut layers: Vec<Layer> = Vec::new();
    for l in layers_iter {
        layers.push(l?);
    }

    let mut hints_stmt = conn.prepare(
        "SELECT key, value
         FROM econetrolehint
         WHERE reponame = ?1;",
    )?;
    let hints_iter = hints_stmt.query_map(&[&index.reponame], |row| {
        Ok(RoleHint {
            key: row.get(0)?,
            value: row.get(1)?,
        })
    })?;

    let mut hints: Vec<RoleHint> = Vec::new();
    for h in hints_iter {
        hints.push(h?);
    }

    Ok(RepoManifest {
        index,
        layers,
        hints,
    })
}

// FFI surface for Lua/Kotlin/C: get manifest as JSON.
#[no_mangle]
pub extern "C" fn econet_manifest_json(
    repo_root_utf8: *const std::os::raw::c_char,
) -> *mut std::os::raw::c_char {
    use std::ffi::{CStr, CString};

    if repo_root_utf8.is_null() {
        return std::ptr::null_mut();
    }

    let c_str = unsafe { CStr::from_ptr(repo_root_utf8) };
    let repo_root_str = match c_str.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
        };

    let repo_root = Path::new(repo_root_str);
    let manifest = match load_manifest_from_repo(repo_root) {
        Ok(m) => m,
        Err(_) => return std::ptr::null_mut(),
    };

    let json = match serde_json::to_string(&manifest) {
        Ok(j) => j,
        Err(_) => return std::ptr::null_mut(),
    };

    let c_string = match CString::new(json) {
        Ok(cs) => cs,
        Err(_) => return std::ptr::null_mut(),
    };

    c_string.into_raw()
}
