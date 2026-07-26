// crates/bostrom-cli/src/main.rs

//! Bostrom CLI entrypoint.
//!
//! This binary provides a thin, non-actuating CLI interface for the `bostrom-cli`
//! library. It is designed to be safe for EcoNet / Eco-Fort style tooling:
//! - No network or disk side-effects beyond stdout/stderr.
//! - Pure, inspectable JSON-RPC loop when `--chat` is enabled.
//!
//! The default mode simply prints a greeting, which is useful for CI and
//! workspace health checks.

use std::io::{self, Read, Write};

use bostrom_cli::hello;

/// Simple struct representing a minimal JSON-RPC request.
///
/// This keeps the CLI surface small and predictable, and avoids guessing
/// at broader protocol semantics. It is enough for AI-chat tools to send
/// "ping" or "identify" style messages.
///
/// Example input:
/// {"jsonrpc":"2.0","method":"ping","params":[],"id":1}
#[derive(Debug)]
struct JsonRpcRequest {
    jsonrpc: String,
    method: String,
    params: String,
    id: String,
}

/// Simple struct representing a minimal JSON-RPC response.
///
/// The `result` field is a human-readable string; errors and complex
/// payloads are intentionally out of scope for this stub.
#[derive(Debug)]
struct JsonRpcResponse {
    jsonrpc: String,
    result: String,
    id: String,
}

/// Parse a very small subset of JSON-RPC from a raw string.
///
/// This is intentionally conservative:
/// - It does not implement a full JSON parser.
/// - It extracts `"method"` and `"id"` using substring searches.
/// - It treats `"params"` as an opaque string.
/// This keeps the stub safe and easy to reason about while remaining
/// usable for test harnesses and AI-chat shells.
fn parse_json_rpc_request(input: &str) -> Option<JsonRpcRequest> {
    // Very small, defensive helpers: find the value for a given JSON key.
    fn extract_field(s: &str, key: &str) -> Option<String> {
        let needle = format!("\"{}\"", key);
        let idx = s.find(&needle)?;
        let rest = &s[idx + needle.len()..];
        let colon_idx = rest.find(':')?;
        let after_colon = &rest[colon_idx + 1..].trim_start();

        // Accept either a quoted string or a bare value until the next comma/brace.
        if after_colon.starts_with('\"') {
            let after_quote = &after_colon[1..];
            let end_quote = after_quote.find('\"')?;
            Some(after_quote[..end_quote].to_string())
        } else {
            let end = after_colon
                .find(|c: char| c == ',' || c == '}' || c == '\n')
                .unwrap_or(after_colon.len());
            Some(after_colon[..end].trim().to_string())
        }
    }

    let jsonrpc = extract_field(input, "jsonrpc")?;
    let method = extract_field(input, "method")?;
    let params = extract_field(input, "params").unwrap_or_else(|| "null".to_string());
    let id = extract_field(input, "id").unwrap_or_else(|| "0".to_string());

    Some(JsonRpcRequest {
        jsonrpc,
        method,
        params,
        id,
    })
}

/// Build a JSON-RPC response string for the given request.
///
/// In this stub, we support:
/// - "ping"           => result: "pong"
/// - "identify"       => result: "bostrom_cli"
/// - any other method => result: "unknown_method:<name>"
fn handle_json_rpc(req: JsonRpcRequest) -> JsonRpcResponse {
    let result = match req.method.as_str() {
        "ping" => "pong".to_string(),
        "identify" => hello().to_string(),
        other => format!("unknown_method:{}", other),
    };

    JsonRpcResponse {
        jsonrpc: req.jsonrpc,
        result,
        id: req.id,
    }
}

/// Serialize a `JsonRpcResponse` to a compact JSON string.
///
/// This stays in lockstep with the minimal fields we support, so it is
/// easy to inspect and log.
fn serialize_json_rpc_response(resp: JsonRpcResponse) -> String {
    format!(
        "{{\"jsonrpc\":\"{}\",\"result\":\"{}\",\"id\":\"{}\"}}",
        resp.jsonrpc, resp.result, resp.id
    )
}

/// Main entrypoint.
///
/// Modes:
/// - No arguments: print the plain `hello()` string.
/// - `--chat`: read a single JSON-RPC request from stdin and emit a
///   single JSON-RPC response to stdout.
///
/// Both modes are side-effect free beyond stdout/stderr, matching the
/// non-actuating requirement for governance and ecosafety tooling.
fn main() {
    let mut args = std::env::args();
    // Skip executable name.
    let _exe = args.next();

    match args.next().as_deref() {
        None => {
            // Default mode: just print the library greeting.
            println!("{}", hello());
        }
        Some("--chat") => {
            // JSON-RPC stub mode: read entire stdin into a buffer.
            let mut stdin_buf = String::new();
            if let Err(e) = io::stdin().read_to_string(&mut stdin_buf) {
                let _ = writeln!(
                    io::stderr(),
                    "bostrom-cli: failed to read stdin in --chat mode: {}",
                    e
                );
                std::process::exit(1);
            }

            match parse_json_rpc_request(&stdin_buf) {
                Some(req) => {
                    let resp = handle_json_rpc(req);
                    let out = serialize_json_rpc_response(resp);
                    if let Err(e) = writeln!(io::stdout(), "{}", out) {
                        let _ = writeln!(
                            io::stderr(),
                            "bostrom-cli: failed to write JSON-RPC response: {}",
                            e
                        );
                        std::process::exit(1);
                    }
                }
                None => {
                    let _ = writeln!(
                        io::stderr(),
                        "bostrom-cli: invalid or unsupported JSON-RPC request in --chat mode"
                    );
                    std::process::exit(1);
                }
            }
        }
        Some(other) => {
            // Unknown flag: explain usage and fail.
            let _ = writeln!(
                io::stderr(),
                "bostrom-cli: unknown flag '{}'. Use either no args or '--chat'.",
                other
            );
            std::process::exit(1);
        }
    }
}
