// filename: governance/src/sqlite_sovereignty_checker.rs
// destination: eco_restoration_shard/governance/src/sqlite_sovereignty_checker.rs

use std::path::{Path, PathBuf};

use anyhow::{Context, Result};
use rusqlite::{params, Connection};

use crate::sovereignty_checker::{
    AbsoluteDataSovereigntyPolicy,
    CiRunContext,
    CyberCoreMigrationAuthority,
    DiffFile,
    SovereigntyCheckResult,
    SovereigntyChecker,
};

/// Concrete, non-actuating implementation of `SovereigntyChecker` backed by SQLite.
///
/// This skeleton assumes the presence of:
/// - `v_active_sovereignty_policy` view from db_absolute_data_sovereignty_policy.sql
/// - `cyber_core_migration_authority` / `v_active_cyber_core_authority` (to be added)
/// - `contribution_ledger` and `v_contribution_summary` from db_contribution_ledger.sql
pub struct SqliteSovereigntyChecker {
    repo_root: PathBuf,
    conn: Connection,
    policy: AbsoluteDataSovereigntyPolicy,
    authority: CyberCoreMigrationAuthority,
}

impl SqliteSovereigntyChecker {
    /// Open a read/write connection to the given SQLite DB and load active governance shards.
    pub fn new<P: AsRef<Path>>(repo_root: P, db_path: P) -> Result<Self> {
        let conn = Connection::open(db_path).context("opening governance SQLite DB")?;

        let policy = Self::load_active_policy(&conn)
            .context("loading active AbsoluteDataSovereigntyPolicy2026v1")?;

        let authority = Self::load_active_migration_authority(&conn)
            .context("loading active CyberCoreMigrationAuthority2026v1")?;

        Ok(SqliteSovereigntyChecker {
            repo_root: repo_root.as_ref().to_path_buf(),
            conn,
            policy,
            authority,
        })
    }

    fn load_active_policy(conn: &Connection) -> Result<AbsoluteDataSovereigntyPolicy> {
        let mut stmt = conn.prepare(
            r#"
            SELECT
                policyid,
                ownerdid,
                hostdid,
                protected_extensions,
                binding_event_kinds,
                requires_contribution,
                contribution_target
            FROM v_active_sovereignty_policy
            LIMIT 1
            "#,
        )?;

        let policy = stmt
            .query_row([], |row| {
                let protected_extensions: String = row.get(3)?;
                let binding_event_kinds: String = row.get(4)?;
                Ok(AbsoluteDataSovereigntyPolicy {
                    policy_id: row.get(0)?,
                    owner_did: row.get(1)?,
                    host_did: row.get(2)?,
                    protected_extensions: protected_extensions
                        .split(',')
                        .map(|s| s.trim().to_string())
                        .filter(|s| !s.is_empty())
                        .collect(),
                    binding_event_kinds: binding_event_kinds
                        .split(',')
                        .map(|s| s.trim().to_string())
                        .filter(|s| !s.is_empty())
                        .collect(),
                    requires_contribution: row.get::<_, i64>(5)? != 0,
                    contribution_target: row.get(6)?,
                })
            })
            .context("no active sovereignty policy found")?;

        Ok(policy)
    }

    fn load_active_migration_authority(conn: &Connection) -> Result<CyberCoreMigrationAuthority> {
        let mut stmt = conn.prepare(
            r#"
            SELECT
                clauseid,
                ownerdid,
                hostdid,
                required_files_glob,
                forbidden_substitutions,
                require_clause_presence,
                require_literal_ownerdid,
                require_literal_hostdid,
                lock_on_violation,
                notify_director_on_violation
            FROM v_active_cyber_core_authority
            LIMIT 1
            "#,
        )?;

        let authority = stmt
            .query_row([], |row| {
                let forbidden_substitutions: String = row.get(4)?;
                Ok(CyberCoreMigrationAuthority {
                    clause_id: row.get(0)?,
                    owner_did: row.get(1)?,
                    host_did: row.get(2)?,
                    required_files_glob: row.get(3)?,
                    forbidden_substitutions: forbidden_substitutions
                        .split(',')
                        .map(|s| s.trim().to_string())
                        .filter(|s| !s.is_empty())
                        .collect(),
                    require_clause_presence: row.get::<_, i64>(5)? != 0,
                    require_literal_ownerdid: row.get::<_, i64>(6)? != 0,
                    require_literal_hostdid: row.get::<_, i64>(7)? != 0,
                    lock_on_violation: row.get::<_, i64>(8)? != 0,
                    notify_director_on_violation: row.get::<_, i64>(9)? != 0,
                })
            })
            .context("no active migration authority found")?;

        Ok(authority)
    }

    fn insert_ledger_event(
        &self,
        ctx: &CiRunContext,
        file: &DiffFile,
        event_kind: &str,
        contribution_type: &str,
        contribution_detail: Option<&str>,
        violation_detail: Option<&str>,
        director_notified: bool,
    ) -> Result<()> {
        let timestamp_utc = chrono::Utc::now().to_rfc3339();
        let policyid = &self.policy.policy_id;
        let file_extension = Path::new(&file.path)
            .extension()
            .and_then(|s| s.to_str())
            .unwrap_or("")
            .to_string();

        let director_notified_i64 = if director_notified { 1_i64 } else { 0_i64 };

        let mut stmt = self.conn.prepare(
            r#"
            INSERT INTO contribution_ledger (
                policyid,
                party_did,
                party_bostrom_address,
                hostdid,
                event_kind,
                file_path,
                file_extension,
                workflow_id,
                contribution_type,
                contribution_detail,
                contribution_target,
                violation_detail,
                director_notified,
                timestamp_utc,
                evidencehex,
                signinghex
            ) VALUES (
                ?1, ?2, NULL, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, '', ''
            )
            "#,
        )?;

        // For CI-side events, party_did can be a dedicated CI DID; here we use owner_did as a placeholder.
        stmt.execute(params![
            policyid,
            self.policy.owner_did,
            self.policy.host_did,
            event_kind,
            file.path,
            file_extension,
            ctx.workflow_name,
            contribution_type,
            contribution_detail.unwrap_or(""),
            self.policy.contribution_target,
            violation_detail.unwrap_or(""),
            director_notified_i64,
            timestamp_utc,
        ])?;

        Ok(())
    }
}

impl SovereigntyChecker for SqliteSovereigntyChecker {
    fn sovereignty_policy(&self) -> &AbsoluteDataSovereigntyPolicy {
        &self.policy
    }

    fn migration_authority(&self) -> &CyberCoreMigrationAuthority {
        &self.authority
    }

    fn repo_root(&self) -> &Path {
        &self.repo_root
    }

    fn check_data_sovereignty(
        &self,
        ctx: &CiRunContext,
        diffs: &[DiffFile],
    ) -> Result<SovereigntyCheckResult> {
        let mut ok = true;
        let mut details = Vec::new();
        let mut should_fail = false;
        let mut should_lock_sessions = false;

        let protected_exts = &self.policy.protected_extensions;

        for file in diffs {
            let ext = Path::new(&file.path)
                .extension()
                .and_then(|s| s.to_str())
                .unwrap_or("")
                .to_string();

            let is_protected = protected_exts.iter().any(|p| {
                if let Some(stripped) = p.strip_prefix('.') {
                    stripped.eq_ignore_ascii_case(&ext)
                } else {
                    p.eq_ignore_ascii_case(&ext)
                }
            });

            if !is_protected {
                continue;
            }

            let has_new_contents = file.new_contents.is_some();
            let mut msg = format!("protected file touched: {}", file.path);

            if self.policy.requires_contribution && has_new_contents {
                // Skeleton: record that a contribution is required; actual linkage to a
                // ContributionLedger2026v1 "ContributionEnforced" entry can be added later.
                msg.push_str(" (contribution required)");
            }

            // For now, touching a protected file always creates a binding event.
            self.insert_ledger_event(
                ctx,
                file,
                "AgreementEnforced",
                if self.policy.requires_contribution {
                    "data"
                } else {
                    "none"
                },
                Some(&msg),
                None,
                false,
            )?;

            details.push(msg);
        }

        let summary = if ok {
            "data sovereignty checks passed".to_string()
        } else {
            "data sovereignty checks found violations".to_string()
        };

        Ok(SovereigntyCheckResult {
            ok,
            summary,
            details,
            should_fail_pipeline: should_fail,
            should_lock_sessions,
        })
    }

    fn check_migration_authority(
        &self,
        ctx: &CiRunContext,
        diffs: &[DiffFile],
    ) -> Result<SovereigntyCheckResult> {
        let mut ok = true;
        let mut details = Vec::new();
        let mut should_fail = false;
        let mut should_lock_sessions = false;

        // Skeleton: enforce literal ownerdid/hostdid retention and forbidden substitutions.
        for file in diffs {
            // Only apply to files that match the glob pattern at the call site; this
            // skeleton assumes the caller has already filtered diffs if desired.
            let old_text = file.old_contents.as_deref().unwrap_or("");
            let new_text = file.new_contents.as_deref().unwrap_or("");

            // Check for removal of owner_did / host_did literals.
            if self.authority.require_literal_ownerdid
                && old_text.contains(&self.authority.owner_did)
                && !new_text.contains(&self.authority.owner_did)
            {
                ok = false;
                should_fail = self.authority.lock_on_violation || should_fail;
                should_lock_sessions = self.authority.lock_on_violation || should_lock_sessions;

                let msg = format!(
                    "migration authority violation: owner_did literal removed from {}",
                    file.path
                );
                self.insert_ledger_event(
                    ctx,
                    file,
                    "ViolationDetected",
                    "none",
                    None,
                    Some(&msg),
                    self.authority.notify_director_on_violation,
                )?;
                details.push(msg);
            }

            if self.authority.require_literal_hostdid
                && old_text.contains(&self.authority.host_did)
                && !new_text.contains(&self.authority.host_did)
            {
                ok = false;
                should_fail = self.authority.lock_on_violation || should_fail;
                should_lock_sessions = self.authority.lock_on_violation || should_lock_sessions;

                let msg = format!(
                    "migration authority violation: host_did literal removed from {}",
                    file.path
                );
                self.insert_ledger_event(
                    ctx,
                    file,
                    "ViolationDetected",
                    "none",
                    None,
                    Some(&msg),
                    self.authority.notify_director_on_violation,
                )?;
                details.push(msg);
            }

            // Check forbidden substitutions.
            for forbidden in &self.authority.forbidden_substitutions {
                if !forbidden.is_empty() && new_text.contains(forbidden) {
                    ok = false;
                    should_fail = self.authority.lock_on_violation || should_fail;
                    should_lock_sessions = self.authority.lock_on_violation || should_lock_sessions;

                    let msg = format!(
                        "migration authority violation: forbidden pattern '{}' detected in {}",
                        forbidden, file.path
                    );
                    self.insert_ledger_event(
                        ctx,
                        file,
                        "ViolationDetected",
                        "none",
                        None,
                        Some(&msg),
                        self.authority.notify_director_on_violation,
                    )?;
                    details.push(msg);
                }
            }
        }

        let summary = if ok {
            "migration authority checks passed".to_string()
        } else {
            "migration authority checks found violations".to_string()
        };

        Ok(SovereigntyCheckResult {
            ok,
            summary,
            details,
            should_fail_pipeline: should_fail,
            should_lock_sessions,
        })
    }

    fn check_all(
        &self,
        ctx: &CiRunContext,
        diffs: &[DiffFile],
    ) -> Result<SovereigntyCheckResult> {
        let ds = self.check_data_sovereignty(ctx, diffs)?;
        let ma = self.check_migration_authority(ctx, diffs)?;

        let mut details = ds.details;
        details.extend(ma.details);

        let ok = ds.ok && ma.ok;

        let summary = if ok {
            "sovereignty checks passed".to_string()
        } else {
            "sovereignty checks found violations".to_string()
        };

        Ok(SovereigntyCheckResult {
            ok,
            summary,
            details,
            should_fail_pipeline: ds.should_fail_pipeline || ma.should_fail_pipeline,
            should_lock_sessions: ds.should_lock_sessions || ma.should_lock_sessions,
        })
    }
}
