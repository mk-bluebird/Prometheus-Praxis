// filename: healthdata_tcr_core.rs
// repo: mk-bluebird/eco_restoration_shard
// destination_hint: PATH_TO_BE_CHOSEN_IN_REPO_ROOT

use rusqlite::{params, Connection, Result};
use std::time::{SystemTime, UNIX_EPOCH};

#[derive(Debug)]
pub struct Config {
    pub base_eco_credit_rate: f64,
    pub responsibility_alpha: f64,
    pub plutocracy_alpha: f64,
}

pub struct HealthDataTcr {
    conn: Connection,
    config: Config,
}

impl HealthDataTcr {
    pub fn new(db_path: &str, config: Config) -> Result<Self> {
        let conn = Connection::open(db_path)?;
        Ok(Self { conn, config })
    }

    pub fn issue_eco_credits_for_labor_event(
        &self,
        brain_id: i64,
        labor_event_id: i64,
        effort_score_local: f64,
        responsibility_scalar: f64,
    ) -> Result<()> {
        let now = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("time went backwards")
            .as_secs()
            .to_string();

        let cumulative: f64 = self.conn.query_row(
            "SELECT IFNULL(MAX(cumulative_eco_credit), 0.0)
             FROM eco_credit_ledger WHERE brain_id = ?1",
            params![brain_id],
            |row| row.get(0),
        )?;

        let amount = self.config.base_eco_credit_rate * effort_score_local * responsibility_scalar;
        let new_cumulative = cumulative + amount;

        self.conn.execute(
            "INSERT INTO eco_credit_ledger
             (brain_id, labor_event_id, issued_at, amount,
              non_transferable, responsibility_scalar, cumulative_eco_credit)
             VALUES (?1, ?2, ?3, ?4, 1, ?5, ?6)",
            params![brain_id, labor_event_id, now, amount, responsibility_scalar, new_cumulative],
        )?;

        Ok(())
    }

    pub fn record_qf_allocation(
        &self,
        project_id: i64,
        brain_id: i64,
        allocated_eco_credits: f64,
    ) -> Result<()> {
        let now = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("time went backwards")
            .as_secs()
            .to_string();

        let (responsibility_scalar, cumulative_eco_credit): (f64, f64) = self.conn.query_row(
            "SELECT responsibility_scalar, cumulative_eco_credit
             FROM eco_credit_ledger
             WHERE brain_id = ?1
             ORDER BY id DESC LIMIT 1",
            params![brain_id],
            |row| Ok((row.get(0)?, row.get(1)?)),
        )?;

        let alpha = self.config.plutocracy_alpha;
        let attenuation = 1.0_f64 / (1.0 + alpha * (1.0 + cumulative_eco_credit).ln());
        let effective_weight = allocated_eco_credits * responsibility_scalar * attenuation;

        self.conn.execute(
            "INSERT INTO qf_allocation
             (project_id, brain_id, allocated_eco_credits,
              responsibility_scalar, effective_weight, allocated_at)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6)",
            params![
                project_id,
                brain_id,
                allocated_eco_credits,
                responsibility_scalar,
                effective_weight,
                now
            ],
        )?;

        Ok(())
    }

    pub fn finalize_qf_round(&self, round_id: i64) -> Result<()> {
        // Caller ensures round_id is valid and time-bounded.
        let mut stmt = self.conn.prepare(
            "SELECT project_id, brain_id, effective_weight
             FROM qf_allocation"
        )?;

        let allocation_iter = stmt.query_map([], |row| {
            Ok((row.get::<_, i64>(0)?, row.get::<_, i64>(1)?, row.get::<_, f64>(2)?))
        })?;

        use std::collections::HashMap;
        let mut per_project: HashMap<i64, Vec<f64>> = HashMap::new();

        for row in allocation_iter {
            let (project_id, _brain_id, effective_weight) = row?;
            per_project.entry(project_id).or_default().push(effective_weight);
        }

        let matching_pool: f64 = self.conn.query_row(
            "SELECT matching_pool FROM qf_round WHERE id = ?1",
            params![round_id],
            |row| row.get(0),
        )?;

        let mut project_scores: HashMap<i64, f64> = HashMap::new();
        let mut total_score = 0.0_f64;

        for (project_id, weights) in &per_project {
            let sum_sqrt: f64 = weights.iter().map(|w| w.sqrt()).sum();
            let score = sum_sqrt * sum_sqrt;
            project_scores.insert(*project_id, score);
            total_score += score;
        }

        for (project_id, score) in project_scores {
            let share = if total_score > 0.0 {
                score / total_score
            } else {
                0.0
            };
            let matched_amount = matching_pool * share;
            let total_effective_support: f64 = per_project.get(&project_id)
                .map(|v| v.iter().sum())
                .unwrap_or(0.0);

            self.conn.execute(
                "INSERT INTO qf_round_result
                 (round_id, project_id, matched_amount, total_effective_support)
                 VALUES (?1, ?2, ?3, ?4)",
                params![round_id, project_id, matched_amount, total_effective_support],
            )?;
        }

        Ok(())
    }
}
