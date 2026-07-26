// crates/bostrom-cli/src/lib.rs

//! Bostrom CLI library.
//!
//! This library intentionally stays minimal and non-actuating:
//! - `hello()` exposes a stable identifier string for the CLI.
//! - Additional helpers can be added later without changing the
//!   side-effect profile.
//!
//! The binary `bostrom-cli` uses this library to provide both a
//! simple greeting mode and a JSON-RPC stub for AI-chat integration.

/// Return the canonical CLI identifier string.
///
/// This is used by both tests and the `--chat` JSON-RPC "identify"
/// method in `main.rs`.
pub fn hello() -> &'static str {
    "bostrom_cli"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hello() {
        assert_eq!(hello(), "bostrom_cli");
    }
}
