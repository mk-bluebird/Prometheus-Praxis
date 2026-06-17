// filename: crates/eco_guard_spine/src/tx_monotone_guard.rs
// destination: eco_restoration_shard/crates/eco_guard_spine/src/tx_monotone_guard.rs
// repo-target: github.com/mk-bluebird/eco_restoration_shard
//
// Rust 2024-style interfaces for:
// (1) SQLite transaction validation with Rust drivers.
// (2) Monotonic, forward-only state progression with invariants.
// (3) Daily rate limiting of execution loops and session limits.
// (4) Duress checking and identity drift constraints.
// (5) Boundary-signal veto patterns that cannot feed reward nets.
// (6) Ecological/karma/thermo metrics mapped to transactional DB rows.
//

#![forbid(unsafe_code)]

use std::sync::Arc;
use std::time::{Duration, SystemTime};

use chrono::{DateTime, Utc};
use parking_lot::Mutex;
use rusqlite::{params, Connection, OptionalExtension, Transaction, TransactionBehavior};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum GuardError {
    #[error("SQLite error: {0}")]
    Sqlite(#[from] rusqlite::Error),

    #[error("Invariant violation: {0}")]
    Invariant(String),

    #[error("Rate limit exceeded: {0}")]
    RateLimit(String),

    #[error("Identity drift or duress detected: {0}")]
    Identity(String),
}

/// Represents a monotonically increasing state sequence for an entity.
/// Every committed transition must strictly increase `state_seq` and must not
/// relax invariants encoded in the DB (KER, RoH, eco-wealth, thermodynamic budget).
pub struct MonotoneStateGuard {
    conn: Connection,
}

impl MonotoneStateGuard {
    pub fn new(conn: Connection) -> Self {
        Self { conn }
    }

    /// Begin a DEFERRED transaction with invariants checked before commit.
    /// - `entity_id`: logical id for shard, steward, or node.
    /// - `next_state_seq`: requested next sequence number (must be > current).
    /// The caller performs domain writes inside `tx_body`, which must use the passed
    /// `Transaction` handle instead of the underlying `Connection`.
    pub fn with_monotone_tx<F, T>(
        &mut self,
        entity_id: &str,
        next_state_seq: i64,
        tx_body: F,
    ) -> Result<T, GuardError>
    where
        F: FnOnce(&Transaction<'_>) -> Result<T, GuardError>,
    {
        let mut tx = self
            .conn
            .transaction_with_behavior(TransactionBehavior::Deferred)?;

        let current_seq: i64 = tx
            .query_row(
                "SELECT state_seq FROM monotone_state WHERE entity_id = ?1",
                params![entity_id],
                |row| row.get(0),
            )
            .optional()?
            .unwrap_or(0);

        if next_state_seq <= current_seq {
            return Err(GuardError::Invariant(format!(
                "non-monotone sequence: current={}, requested={}",
                current_seq, next_state_seq
            )));
        }

        let result = tx_body(&tx)?;

        // Run invariant checks inside the same transaction.
        self.check_invariants(&tx, entity_id)?;

        tx.execute(
            "INSERT INTO monotone_state (entity_id, state_seq, updated_utc)
             VALUES (?1, ?2, strftime('%Y-%m-%dT%H:%M:%SZ','now'))
             ON CONFLICT(entity_id) DO UPDATE SET state_seq = excluded.state_seq,
                                                  updated_utc = excluded.updated_utc",
            params![entity_id, next_state_seq],
        )?;

        tx.commit()?;
        Ok(result)
    }

    /// Example invariant checks that prevent backward movement or eco / thermo regression.
    fn check_invariants(&self, tx: &Transaction<'_>, entity_id: &str) -> Result<(), GuardError> {
        // Check that residual Lyapunov potential does not increase beyond allowed band.
        let (vt_prev, vt_next): (f64, f64) = tx
            .prepare(
                "SELECT
                     COALESCE(prev.vt_residual, 0.0),
                     COALESCE(next.vt_residual, 0.0)
                 FROM eco_state_prev AS prev
                 JOIN eco_state_next AS next
                   ON prev.entity_id = next.entity_id
                WHERE prev.entity_id = ?1",
            )?
            .query_row(params![entity_id], |row| Ok((row.get(0)?, row.get(1)?)))
            .optional()?
            .unwrap_or((0.0, 0.0));

        if vt_next > vt_prev && (vt_next - vt_prev) > 1e-6 {
            return Err(GuardError::Invariant(format!(
                "Vt residual increased: prev={}, next={}",
                vt_prev, vt_next
            )));
        }

        // Check K, E, R monotonic constraints (no relaxing risk or eco thresholds).
        let (k_prev, k_next, r_prev, r_next): (f64, f64, f64, f64) = tx
            .prepare(
                "SELECT
                     COALESCE(prev.k_factor, 0.0),
                     COALESCE(next.k_factor, 0.0),
                     COALESCE(prev.r_factor, 1.0),
                     COALESCE(next.r_factor, 1.0)
                 FROM ker_state_prev AS prev
                 JOIN ker_state_next AS next
                   ON prev.entity_id = next.entity_id
                WHERE prev.entity_id = ?1",
            )?
            .query_row(params![entity_id], |row| {
                Ok((row.get(0)?, row.get(1)?, row.get(2)?, row.get(3)?))
            })
            .optional()?
            .unwrap_or((0.0, 0.0, 1.0, 1.0));

        if k_next + 1e-9 < k_prev {
            return Err(GuardError::Invariant(format!(
                "knowledge factor decreased: prev={}, next={}",
                k_prev, k_next
            )));
        }
        if r_next - 1e-9 > r_prev {
            return Err(GuardError::Invariant(format!(
                "risk factor increased: prev={}, next={}",
                r_prev, r_next
            )));
        }

        Ok(())
    }
}

/// In-memory rate limiter for daily and per-session limits in a distributed system.
///
/// In production this would be backed by SQLite or a shared store; here we provide
/// the interface and a simple in-process implementation that agents can mirror.
#[derive(Clone)]
pub struct RateLimiter {
    inner: Arc<Mutex<RateLimiterState>>,
}

#[derive(Debug)]
struct RateLimiterState {
    daily_limit: u32,
    session_limit: u32,
    daily_count: u32,
    session_count: u32,
    daily_window_start: DateTime<Utc>,
    session_start: DateTime<Utc>,
    daily_window: Duration,
    session_window: Duration,
}

impl RateLimiter {
    pub fn new(daily_limit: u32, session_limit: u32) -> Self {
        let now = Utc::now();
        Self {
            inner: Arc::new(Mutex::new(RateLimiterState {
                daily_limit,
                session_limit,
                daily_count: 0,
                session_count: 0,
                daily_window_start: now,
                session_start: now,
                daily_window: Duration::from_secs(24 * 60 * 60),
                session_window: Duration::from_secs(60 * 60),
            })),
        }
    }

    /// Attempt to acquire permission for a single execution.
    /// Returns Ok(()) if allowed, or GuardError::RateLimit if rejected.
    pub fn acquire(&self) -> Result<(), GuardError> {
        let mut state = self.inner.lock();
        let now = Utc::now();

        // Reset daily window if needed.
        if now.signed_duration_since(state.daily_window_start).num_seconds()
            >= state.daily_window.as_secs() as i64
        {
            state.daily_count = 0;
            state.daily_window_start = now;
        }

        // Reset session window if needed.
        if now.signed_duration_since(state.session_start).num_seconds()
            >= state.session_window.as_secs() as i64
        {
            state.session_count = 0;
            state.session_start = now;
        }

        if state.daily_count >= state.daily_limit {
            return Err(GuardError::RateLimit(format!(
                "daily limit reached: {}/{}",
                state.daily_count, state.daily_limit
            )));
        }
        if state.session_count >= state.session_limit {
            return Err(GuardError::RateLimit(format!(
                "session limit reached: {}/{}",
                state.session_count, state.session_limit
            )));
        }

        state.daily_count += 1;
        state.session_count += 1;
        Ok(())
    }
}

/// Lightweight duress and identity-drift check.
/// This expects the surrounding system to feed it signals derived from
/// neuro-psychological indicators and access-pattern analysis.
#[derive(Debug, Clone)]
pub struct IdentitySignals {
    pub steward_did: String,
    pub session_id: String,
    pub device_fingerprint: String,
    pub geo_region: String,
    pub last_auth_utc: DateTime<Utc>,
    pub behaviour_score: f32,      // 0..1 modelled trust in behavioural pattern
    pub duress_probability: f32,   // 0..1 probability from upper-layer model
    pub drift_score: f32,          // 0..1 distance from long-term identity profile
}

pub struct IdentityGuard {
    min_behaviour_score: f32,
    max_duress_probability: f32,
    max_drift_score: f32,
    max_session_age: Duration,
}

impl IdentityGuard {
    pub fn new() -> Self {
        Self {
            min_behaviour_score: 0.75,
            max_duress_probability: 0.25,
            max_drift_score: 0.35,
            max_session_age: Duration::from_secs(8 * 60 * 60),
        }
    }

    /// Evaluate whether this session is acceptable for sensitive actions.
    /// This should be called before any actuation or reward-related computation.
    pub fn ensure_safe(&self, signals: &IdentitySignals) -> Result<(), GuardError> {
        let now = Utc::now();
        let age_secs = now
            .signed_duration_since(signals.last_auth_utc)
            .num_seconds()
            .max(0) as u64;

        if age_secs > self.max_session_age.as_secs() {
            return Err(GuardError::Identity(format!(
                "session expired: age_secs={}",
                age_secs
            )));
        }

        if signals.behaviour_score < self.min_behaviour_score {
            return Err(GuardError::Identity(format!(
                "behaviour score too low: {} < {}",
                signals.behaviour_score, self.min_behaviour_score
            )));
        }

        if signals.duress_probability > self.max_duress_probability {
            return Err(GuardError::Identity(format!(
                "duress probability too high: {} > {}",
                signals.duress_probability, self.max_duress_probability
            )));
        }

        if signals.drift_score > self.max_drift_score {
            return Err(GuardError::Identity(format!(
                "identity drift too high: {} > {}",
                signals.drift_score, self.max_drift_score
            )));
        }

        Ok(())
    }
}

/// Boundary-signal veto interface. This type never exposes raw boundary signals
/// to reward or optimization layers. Instead it exposes a boolean veto only.
pub struct BoundaryVetoPolicy {
    roh_ceiling: f32,
    vt_ceiling: f32,
    thermo_budget_floor: f32,
}

#[derive(Debug)]
pub struct BoundarySignals {
    pub roh_after: f32,     // projected risk-of-harm after action
    pub vt_after: f32,      // projected Lyapunov residual
    pub thermo_budget: f32, // remaining thermodynamic / metabolic budget 0..1
    pub karma_delta: f32,   // signed karmic / ecoimpact change
}

impl BoundaryVetoPolicy {
    pub fn new() -> Self {
        Self {
            roh_ceiling: 0.30,
            vt_ceiling: 1.00,
            thermo_budget_floor: 0.10,
        }
    }

    /// Returns true if the proposed step must be vetoed.
    pub fn veto(&self, s: &BoundarySignals) -> bool {
        if s.roh_after > self.roh_ceiling {
            return true;
        }
        if s.vt_after > self.vt_ceiling {
            return true;
        }
        if s.thermo_budget < self.thermo_budget_floor {
            return true;
        }
        if s.karma_delta < 0.0 && s.thermo_budget < 0.5 {
            return true;
        }
        false
    }

    /// Guard wrapper that prevents the caller from seeing boundary signals;
    /// they only see Ok or a GuardError, so they cannot backpropagate gradients.
    pub fn ensure_not_violated(&self, s: &BoundarySignals) -> Result<(), GuardError> {
        if self.veto(s) {
            Err(GuardError::Invariant(
                "boundary-signal veto: step rejected".to_string(),
            ))
        } else {
            Ok(())
        }
    }
}

/// Mapping ecological restoration, karma, and thermodynamic boundaries into
/// transactional database updates. This function shows a pattern for a single
/// "eco-transaction" that must respect KER, karma, and thermodynamic limits.
pub fn apply_eco_transaction(
    guard: &mut MonotoneStateGuard,
    identity_guard: &IdentityGuard,
    boundary_policy: &BoundaryVetoPolicy,
    limiter: &RateLimiter,
    entity_id: &str,
    next_state_seq: i64,
    identity_signals: &IdentitySignals,
    planned_signals: &BoundarySignals,
    eco_delta: f64,
) -> Result<(), GuardError> {
    limiter.acquire()?;
    identity_guard.ensure_safe(identity_signals)?;
    boundary_policy.ensure_not_violated(planned_signals)?;

    guard.with_monotone_tx(entity_id, next_state_seq, |tx| {
        tx.execute(
            "INSERT INTO eco_transaction_ledger (
                 entity_id,
                 applied_utc,
                 eco_delta,
                 roh_after,
                 vt_after,
                 thermo_budget,
                 karma_delta
             ) VALUES (
                 ?1,
                 strftime('%Y-%m-%dT%H:%M:%SZ','now'),
                 ?2,
                 ?3,
                 ?4,
                 ?5,
                 ?6
             )",
            params![
                entity_id,
                eco_delta,
                planned_signals.roh_after,
                planned_signals.vt_after,
                planned_signals.thermo_budget,
                planned_signals.karma_delta
            ],
        )?;

        tx.execute(
            "UPDATE eco_state_next
                SET eco_balance = eco_balance + ?2
              WHERE entity_id = ?1",
            params![entity_id, eco_delta],
        )?;

        Ok(())
    })
}
