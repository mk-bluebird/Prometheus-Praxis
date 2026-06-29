// Filename: crates/ecosafety-nanoswarm-urban-core/src/fpic_consent_kernel.rs

use rusqlite::{params, Connection};
use serde::{Deserialize, Serialize};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum ConsentError {
    #[error("SQLite error: {0}")]
    Sqlite(#[from] rusqlite::Error),
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum ConsentDecision {
    Allow { plan_id: String, site_id: String, leaf_token_id: String },
    StopNoPlanBinding,
    StopPlanStale,
    StopBrokenChain,
    StopScopeMismatch,
}

#[derive(Clone, Debug)]
struct FpicToken {
    id: String,
    community_id: String,
    site_id: String,
    scope: String,
    issued_ts: i64,
    expiry_ts: i64,
    parent_token_id: Option<String>,
    status: String,
}

#[derive(Clone, Debug)]
struct FpicPlanBinding {
    plan_id: String,
    site_id: String,
    required_scope: String,
    min_ts: i64,
    max_age_hours: i64,
    fpic_root_id: String,
}

pub struct ConsentKernel {
    conn: Connection,
}

impl ConsentKernel {
    pub fn new(db_path: &str) -> Result<Self, ConsentError> {
        let conn = Connection::open(db_path)?;
        Ok(Self { conn })
    }

    fn load_plan_binding(&self, plan_id: &str) -> Result<Option<FpicPlanBinding>, ConsentError> {
        let mut stmt = self.conn.prepare(
            "SELECT plan_id, site_id, required_scope, min_ts, max_age_hours, fpic_root_id \
             FROM fpic_plan_binding WHERE plan_id = ?1",
        )?;
        let mut rows = stmt.query(params![plan_id])?;
        if let Some(row) = rows.next()? {
            Ok(Some(FpicPlanBinding {
                plan_id: row.get(0)?,
                site_id: row.get(1)?,
                required_scope: row.get(2)?,
                min_ts: row.get(3)?,
                max_age_hours: row.get(4)?,
                fpic_root_id: row.get(5)?,
            }))
        } else {
            Ok(None)
        }
    }

    fn load_token(&self, token_id: &str) -> Result<Option<FpicToken>, ConsentError> {
        let mut stmt = self.conn.prepare(
            "SELECT id, community_id, site_id, scope, issued_ts, expiry_ts, parent_token_id, status \
             FROM fpic_token WHERE id = ?1",
        )?;
        let mut rows = stmt.query(params![token_id])?;
        if let Some(row) = rows.next()? {
            Ok(Some(FpicToken {
                id: row.get(0)?,
                community_id: row.get(1)?,
                site_id: row.get(2)?,
                scope: row.get(3)?,
                issued_ts: row.get(4)?,
                expiry_ts: row.get(5)?,
                parent_token_id: row.get(6)?,
                status: row.get(7)?,
            }))
        } else {
            Ok(None)
        }
    }

    fn load_children(&self, parent_id: &str) -> Result<Vec<FpicToken>, ConsentError> {
        let mut stmt = self.conn.prepare(
            "SELECT id, community_id, site_id, scope, issued_ts, expiry_ts, parent_token_id, status \
             FROM fpic_token WHERE parent_token_id = ?1",
        )?;
        let mut rows = stmt.query(params![parent_id])?;
        let mut tokens = Vec::new();
        while let Some(row) = rows.next()? {
            tokens.push(FpicToken {
                id: row.get(0)?,
                community_id: row.get(1)?,
                site_id: row.get(2)?,
                scope: row.get(3)?,
                issued_ts: row.get(4)?,
                expiry_ts: row.get(5)?,
                parent_token_id: row.get(6)?,
                status: row.get(7)?,
            });
        }
        Ok(tokens)
    }

    fn chain_latest_leaf(&self, root: &FpicToken) -> Result<FpicToken, ConsentError> {
        let mut current = root.clone();
        loop {
            let children = self.load_children(&current.id)?;
            if children.is_empty() {
                return Ok(current);
            }
            let mut latest = children[0].clone();
            for c in children.into_iter().skip(1) {
                if c.issued_ts > latest.issued_ts {
                    latest = c;
                }
            }
            current = latest;
        }
    }

    fn scope_covers(required: &str, token_scope: &str) -> bool {
        if required == token_scope {
            return true;
        }
        if token_scope == "MIXED" {
            return true;
        }
        false
    }

    fn now_utc_secs() -> i64 {
        use std::time::{Duration, SystemTime};
        SystemTime::now()
            .duration_since(SystemTime::UNIX_EPOCH)
            .unwrap_or(Duration::from_secs(0))
            .as_secs() as i64
    }

    pub fn evaluate(&self, plan_id: &str) -> Result<ConsentDecision, ConsentError> {
        let now = Self::now_utc_secs();
        let binding = match self.load_plan_binding(plan_id)? {
            Some(b) => b,
            None => return Ok(ConsentDecision::StopNoPlanBinding),
        };

        let age_secs = now - binding.min_ts;
        let max_age_secs = binding.max_age_hours * 3600;
        if age_secs > max_age_secs {
            return Ok(ConsentDecision::StopPlanStale);
        }

        let root = match self.load_token(&binding.fpic_root_id)? {
            Some(t) => t,
            None => return Ok(ConsentDecision::StopBrokenChain),
        };

        let leaf = self.chain_latest_leaf(&root)?;

        if !Self::scope_covers(&binding.required_scope, &leaf.scope) {
            return Ok(ConsentDecision::StopScopeMismatch);
        }

        if leaf.status != "ACTIVE" {
            return Ok(ConsentDecision::StopBrokenChain);
        }

        if now < leaf.issued_ts || now > leaf.expiry_ts {
            return Ok(ConsentDecision::StopBrokenChain);
        }

        Ok(ConsentDecision::Allow {
            plan_id: binding.plan_id,
            site_id: binding.site_id,
            leaf_token_id: leaf.id,
        })
    }
}
