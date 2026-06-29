// Filename: crates/nanoswarm-consent-kernel/src/lib.rs
// rust-version = "1.85", edition = "2024", license = "MIT OR Apache-2.0"

use rusqlite::{Connection, params};
use std::time::{SystemTime, Duration};

#[derive(Clone, Copy, Debug)]
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
    pub fn new(db_path: &str) -> rusqlite::Result<Self> {
        let conn = Connection::open(db_path)?;
        Ok(Self { conn })
    }

    fn load_plan_binding(&self, plan_id: &str) -> rusqlite::Result<Option<FpicPlanBinding>> {
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

    fn load_token(&self, token_id: &str) -> rusqlite::Result<Option<FpicToken>> {
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

    fn load_child_tokens(&self, parent_id: &str) -> rusqlite::Result<Vec<FpicToken>> {
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

    fn now_utc_secs() -> i64 {
        SystemTime::now()
            .duration_since(SystemTime::UNIX_EPOCH)
            .unwrap_or(Duration::from_secs(0))
            .as_secs() as i64
    }

    fn chain_latest_leaf(&self, root: &FpicToken) -> rusqlite::Result<Option<FpicToken>> {
        let mut current = root.clone();
        loop {
            let children = self.load_child_tokens(&current.id)?;
            if children.is_empty() {
                return Ok(Some(current));
            }
            // Choose child with latest issued_ts
            let mut latest = None;
            for c in children {
                if latest
                    .as_ref()
                    .map_or(true, |best: &FpicToken| c.issued_ts > best.issued_ts)
                {
                    latest = Some(c);
                }
            }
            if let Some(next) = latest {
                current = next;
            } else {
                return Ok(Some(current));
            }
        }
    }

    fn scope_covers(required: &str, token_scope: &str) -> bool {
        if required == token_scope {
            return true;
        }
        // Simple example: MIXED covers any specific remediation/excavation scope.
        if token_scope == "MIXED" {
            return true;
        }
        false
    }

    pub fn evaluate(&self, plan_id: &str) -> ConsentDecision {
        let now = Self::now_utc_secs();
        let binding = match self.load_plan_binding(plan_id) {
            Ok(Some(b)) => b,
            _ => return ConsentDecision::StopNoPlanBinding,
        };

        // Check plan freshness
        let age_secs = now - binding.min_ts;
        let max_age_secs = binding.max_age_hours * 3600;
        if age_secs > max_age_secs {
            return ConsentDecision::StopPlanStale;
        }

        // Load root token
        let root = match self.load_token(&binding.fpic_root_id) {
            Ok(Some(t)) => t,
            _ => return ConsentDecision::StopBrokenChain,
        };

        // Walk chain to latest leaf
        let leaf = match self.chain_latest_leaf(&root) {
            Ok(Some(t)) => t,
            _ => return ConsentDecision::StopBrokenChain,
        };

        // Check scope and status/time window
        if !Self::scope_covers(&binding.required_scope, &leaf.scope) {
            return ConsentDecision::StopScopeMismatch;
        }

        if leaf.status != "ACTIVE" {
            return ConsentDecision::StopBrokenChain;
        }

        if now < leaf.issued_ts || now > leaf.expiry_ts {
            return ConsentDecision::StopBrokenChain;
        }

        ConsentDecision::Allow {
            plan_id: binding.plan_id,
            site_id: binding.site_id,
            leaf_token_id: leaf.id,
        }
    }
}
