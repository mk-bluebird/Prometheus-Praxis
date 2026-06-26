//! Binary: ppx_continuity_mcp_server
//!
//! Simple stdin/stdout JSON loop that exposes the continuity kernel as
//! MCP-style tools. Transport and authentication are handled by the
//! surrounding MCP runtime; this binary only:
//! - Opens the SQLite continuity DB in read-only mode.
//! - Reads newline-delimited JSON `McpRequest` objects from stdin.
//! - Writes newline-delimited JSON `McpResponse` objects to stdout.

use std::env;
use std::io::{self, BufRead, Write};

use ppx_continuity_kernel::ContinuityKernel;
use ppx_continuity_mcp::{error_response, handle_request, McpError, McpRequest, McpResponse};

fn main() {
    // Expect the SQLite DB path as the first CLI argument or via env var.
    // This keeps the binary configuration simple and explicit.
    let db_path = env::args()
        .nth(1)
        .or_else(|| env::var("PPX_CONTINUITY_DB_PATH").ok())
        .unwrap_or_else(|| {
            eprintln!("Usage: ppx_continuity_mcp_server <path-to-sqlite-db>");
            std::process::exit(1);
        });

    let kernel = match ContinuityKernel::open_read_only(&db_path) {
        Ok(k) => k,
        Err(e) => {
            eprintln!("Failed to open continuity DB at {}: {}", db_path, e);
            std::process::exit(1);
        }
    };

    let stdin = io::stdin();
    let mut stdout = io::stdout();
    let mut line_buf = String::new();

    // Read newline-delimited JSON requests.
    for line in stdin.lock().lines() {
        line_buf.clear();
        let line = match line {
            Ok(l) => l,
            Err(e) => {
                eprintln!("Error reading stdin: {e}");
                break;
            }
        };
        if line.trim().is_empty() {
            continue;
        }

        let req: Result<McpRequest, _> = serde_json::from_str(&line);
        let resp: McpResponse = match req {
            Ok(r) => match handle_request(&kernel, r.clone()) {
                Ok(ok) => ok,
                Err(err) => error_response(r.id, err),
            },
            Err(e) => {
                // No valid id to echo; use a synthetic one.
                error_response("invalid".to_string(), McpError::InvalidRequest(e.to_string()))
            }
        };

        let out = match serde_json::to_string(&resp) {
            Ok(s) => s,
            Err(e) => {
                eprintln!("Failed to serialize response: {e}");
                continue;
            }
        };

        if let Err(e) = writeln!(stdout, "{}", out) {
            eprintln!("Error writing stdout: {e}");
            break;
        }
        if let Err(e) = stdout.flush() {
            eprintln!("Error flushing stdout: {e}");
            break;
        }
    }
}
