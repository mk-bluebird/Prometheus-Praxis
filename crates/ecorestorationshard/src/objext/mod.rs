// filename: src/objext/mod.rs
// destination: ecorestorationshard/src/objext/mod.rs
// repo-target: github.com/mk-bluebird/eco_restoration_shard

use std::time::SystemTime;
use rusqlite::{params, Connection};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ObjextSession {
    pub session_id: String,
    pub user_id: Option<String>,
    pub source: String,
    pub created_at: String,
    pub input_summary: String,
    pub profile: String,
    pub shard_id: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ObjextObject {
    pub session_id: String,
    pub section_index: u8,
    pub obj_index: u8,
    pub obj_type: String,
    pub title: String,
    pub prompt: String,
    pub complexity_reason: String,
    pub priority: u8,
    pub tags: Vec<String>,
}

#[derive(Debug, Clone)]
pub struct ObjextPattern {
    pub pattern_id: String,
    pub profile: String,
    pub min_objects: u8,
    pub max_objects: u8,
    pub section_size: u8,
    pub allowed_types: Vec<String>,
}

pub fn objext_generate_100(
    conn: &Connection,
    mut session: ObjextSession,
    pattern: ObjextPattern,
    user_input: &str,
) -> anyhow::Result<(ObjextSession, Vec<ObjextObject>)> {
    if pattern.min_objects != 100 || pattern.max_objects != 100 || pattern.section_size != 10 {
        anyhow::bail!("pattern must be fixed at 100 objects in 10 sections");
    }

    if session.created_at.is_empty() {
        session.created_at = rfc3339_now();
    }
    if session.shard_id.is_empty() {
        session.shard_id = "OBJEXT-MAP-V1".to_string();
    }

    let objects = hostfn_objext_build_objects(&session, &pattern, user_input)?;
    hostfn_objext_save_sqlite(conn, &session, &objects)?;
    Ok((session, objects))
}

fn hostfn_objext_build_objects(
    session: &ObjextSession,
    pattern: &ObjextPattern,
    user_input: &str,
) -> anyhow::Result<Vec<ObjextObject>> {
    let mut out = Vec::with_capacity(100);
    let mut idx: u8 = 1;
    let mut section: u8 = 1;

    while idx <= 100 {
        let obj_type = pattern
            .allowed_types
            .get(((idx - 1) as usize) % pattern.allowed_types.len())
            .cloned()
            .unwrap_or_else(|| "topic".to_string());

        let title = format!("{} object {}", pattern.pattern_id, idx);
        let prompt = format!("{} :: {} :: {}", session.input_summary, user_input, idx);
        let complexity_reason = format!(
            "Non-trivial {} derived from session {}, index {}",
            obj_type, session.session_id, idx
        );

        let obj = ObjextObject {
            session_id: session.session_id.clone(),
            section_index: section,
            obj_index: idx,
            obj_type,
            title,
            prompt,
            complexity_reason,
            priority: 1,
            tags: vec![session.profile.clone()],
        };
        out.push(obj);

        if idx % pattern.section_size == 0 {
            section += 1;
        }
        idx += 1;
    }

    Ok(out)
}

fn hostfn_objext_save_sqlite(
    conn: &Connection,
    session: &ObjextSession,
    objects: &[ObjextObject],
) -> anyhow::Result<()> {
    let tx = conn.transaction()?;

    tx.execute(
        "INSERT OR REPLACE INTO objext_session \
         (session_id, user_id, source, created_at, input_summary, profile, shard_id) \
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)",
        params![
            session.session_id,
            session.user_id,
            session.source,
            session.created_at,
            session.input_summary,
            session.profile,
            session.shard_id,
        ],
    )?;

    let mut stmt = tx.prepare(
        "INSERT OR REPLACE INTO objext_object \
         (session_id, section_index, obj_index, obj_type, title, prompt, \
          complexity_reason, priority, tags) \
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)",
    )?;

    for o in objects {
        let tags_json = serde_json::to_string(&o.tags)?;
        stmt.execute(params![
            o.session_id,
            o.section_index as i64,
            o.obj_index as i64,
            o.obj_type,
            o.title,
            o.prompt,
            o.complexity_reason,
            o.priority as i64,
            tags_json,
        ])?;
    }

    tx.commit()?;
    Ok(())
}

fn rfc3339_now() -> String {
    let now = SystemTime::now();
    let datetime: chrono::DateTime<chrono::Utc> = now.into();
    datetime.to_rfc3339()
}
