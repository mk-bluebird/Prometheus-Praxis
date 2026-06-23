use rusqlite::{Connection, params};
use serde::{Deserialize, Serialize};
use time::{OffsetDateTime, format_description::well_known::Rfc3339};

/// Append-only revocation record.
/// Design (D): Forward-only; no delete operations supported.
/// NR: 0 — Only capability hex and reason.
/// EE: Local SQLite avoids remote revocation calls per tap.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RevocationRecord {
    pub id: i64,
    pub capability_hex: String,
    pub reason: RevocationReason,
    pub revoked_at_utc: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum RevocationReason {
    Compromised,
    EcoViolation,
    NeurorightsViolation,
    UserRequested,
    Administrative,
}

pub struct RevocationStore {
    conn: Connection,
}

impl RevocationStore {
    /// Create a new revocation store backed by a local SQLite file.
    /// Schema is append-only by design (no ON DELETE CASCADE, no delete API).
    pub fn new(path: &str) -> rusqlite::Result<Self> {
        let conn = Connection::open(path)?;
        conn.execute_batch(
            r#"
            CREATE TABLE IF NOT EXISTS revocations (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                capability_hex TEXT NOT NULL,
                reason TEXT NOT NULL,
                revoked_at_utc TEXT NOT NULL
            );
            "#,
        )?;
        Ok(Self { conn })
    }

    pub fn append(
        &self,
        capability_hex: &str,
        reason: RevocationReason,
        revoked_at: OffsetDateTime,
    ) -> rusqlite::Result<()> {
        let ts = revoked_at.format(&Rfc3339).unwrap();
        let reason_str = serde_json::to_string(&reason).unwrap();
        self.conn.execute(
            "INSERT INTO revocations (capability_hex, reason, revoked_at_utc) VALUES (?1, ?2, ?3)",
            params![capability_hex, reason_str, ts],
        )?;
        Ok(())
    }

    pub fn is_revoked(&self, capability_hex: &str) -> rusqlite::Result<bool> {
        let mut stmt = self.conn.prepare(
            "SELECT EXISTS(SELECT 1 FROM revocations WHERE capability_hex = ?1 LIMIT 1)",
        )?;
        let exists: i64 = stmt.query_row([capability_hex], |row| row.get(0))?;
        Ok(exists == 1)
    }
}
