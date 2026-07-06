// File: Eco-Fort/src/decision_log_core.rs
// DirClass: SRC

use rusqlite::{params, Connection, Result as SqlResult};
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use uuid::Uuid;

/// The core non-actuating decision record.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DecisionLog {
    pub decisionid: String,
    pub taskid: String,
    pub allowed: bool,
    pub reasons: String,
    pub hextrace: String,
    pub timestamputc: String,
    pub kervector: [f64; 3], // [K, E, R]
    pub janus_veritas_ref: String,
    pub lyapunov_residual: f64,
    pub tsafe_margin: f64,
}

/// Trait for the immutable append-only ledger (e.g., Veritas Chain).
pub trait VeritasChainClient {
    fn append(&self, payload: &[u8]) -> Result<String, String>;
}

/// The non-actuating logger. It ONLY records and proves; it NEVER triggers physical actuators.
pub struct DecisionLogger<C: VeritasChainClient> {
    client: C,
    db_path: String,
}

impl<C: VeritasChainClient> DecisionLogger<C> {
    pub fn new(client: C, db_path: String) -> Self {
        Self { client, db_path }
    }

    pub fn log_decision(&self, log: &mut DecisionLog) -> Result<(), String> {
        // 1. Cryptographic Sealing (Janus Face)
        let mut hasher = Sha256::new();
        hasher.update(format!("{}:{}:{}", log.taskid, log.allowed, log.reasons).as_bytes());
        let computed_hex = format!("{:x}", hasher.finalize());
        
        if computed_hex != log.hextrace {
            return Err("Hextrace mismatch: Payload tampering detected".to_string());
        }

        // 2. Immutable Ledger Append (Veritas Face)
        let payload = serde_json::to_vec(log).map_err(|e| e.to_string())?;
        let proof_ref = self.client.append(&payload)?;
        log.janus_veritas_ref = proof_ref;

        // 3. SQLite Spine Registration
        let conn = Connection::open(&self.db_path).map_err(|e| e.to_string())?;
        conn.execute(
            "INSERT INTO decision_log_shard 
             (decisionid, taskid, allowed, reasons, hextrace, timestamputc, k_ker, e_ker, r_ker, janus_veritas_ref, lyapunov_residual, tsafe_margin)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12)",
            params![
                log.decisionid, log.taskid, log.allowed as i32, log.reasons, 
                log.hextrace, log.timestamputc, 
                log.kervector[0], log.kervector[1], log.kervector[2], 
                log.janus_veritas_ref, log.lyapunov_residual, log.tsafe_margin,
            ],
        ).map_err(|e| e.to_string())?;

        Ok(())
    }
}

// --- FFI Exports for Edge Nodes (Lua/C++) ---

#[no_mangle]
pub extern "C" fn econet_compute_hextrace(
    taskid: *const c_char, 
    allowed: i32, 
    reasons: *const c_char
) -> *mut c_char {
    let t = unsafe { CStr::from_ptr(taskid).to_str().unwrap_or("") };
    let r = unsafe { CStr::from_ptr(reasons).to_str().unwrap_or("") };
    
    let mut hasher = Sha256::new();
    hasher.update(format!("{}:{}:{}", t, allowed != 0, r).as_bytes());
    let hex = format!("{:x}", hasher.finalize());
    
    CString::new(hex).unwrap().into_raw()
}
