// filename: crates/cross-constellation-index/src/main.rs
// destination: eco_restoration_shard/crates/cross-constellation-index/src/main.rs

//! CLI entry point for the cross-constellation indexer.
//!
//! Usage (from workspace root):
//!   cargo run -p cross-constellation-index -- \
//!       --root . \
//!       > cross-constellation-index.json

#![forbid(unsafe_code)]

use std::env;
use std::path::PathBuf;

use anyhow::{bail, Context, Result};
use cross_constellation_index::CrossConstellationIndex;

fn print_usage() {
    eprintln!(
        "Usage: cross-constellation-index --root <path>\n\
         Scans the given root for *.repo.json and *aln-index.json files and \
         prints a consolidated JSON index to stdout."
    );
}

fn main() -> Result<()> {
    let mut args = env::args().skip(1);
    let mut root: Option<PathBuf> = None;

    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--root" => {
                let value = args
                    .next()
                    .context("expected a path argument after --root")?;
                root = Some(PathBuf::from(value));
            }
            "--help" | "-h" => {
                print_usage();
                return Ok(());
            }
            other => {
                bail!("unrecognized argument: {}", other);
            }
        }
    }

    let root = match root {
        Some(r) => r,
        None => {
            print_usage();
            bail!("missing required --root argument");
        }
    };

    let index = CrossConstellationIndex::scan_root(&root)
        .with_context(|| format!("scanning root {:?}", root))?;

    let json = index.to_json()?;
    println!("{json}");

    Ok(())
}
