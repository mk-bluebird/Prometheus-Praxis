// filename: ppx_minimal_identity_kernel/src/bin/ci_guard.rs

use ppx_minimal_identity_kernel::ci_guard::run_ci_guard;
use std::env;
use std::path::Path;

fn main() {
    let root = env::args().nth(1).unwrap_or_else(|| ".".to_string());
    let result = run_ci_guard(Path::new(&root));
    if !result.ok {
        eprintln!("PPX minimal identity kernel CI guard failed:");
        for v in result.violations {
            eprintln!(" - {v}");
        }
        std::process::exit(1);
    }
    println!("PPX minimal identity kernel CI guard passed.");
}
