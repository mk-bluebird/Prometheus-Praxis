#![forbid(unsafe_code)]

use std::env;
use std::fs;
use std::io::{self, Write};
use std::path::Path;
use std::process::exit;

use always_improve_core::{
    WindowMetrics, compute_always_improve_score, last_window_satisfies_minimum_trend,
    windows_satisfy_monotonicity,
};
use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
struct WindowFile {
    windows: Vec<WindowMetrics>,
}

fn main() {
    let path = env::args().nth(1).unwrap_or_else(|| {
        eprintln!("[AI-META] Expected path to Always-Improve windows JSON.");
        exit(1);
    });

    let path_obj = Path::new(&path);

    // Load existing windows file or start empty.
    let mut windows: Vec<WindowMetrics> = if path_obj.exists() {
        let data = fs::read_to_string(path_obj).expect("read windows file");
        let wf: WindowFile = serde_json::from_str(&data).expect("parse windows file");
        wf.windows
    } else {
        Vec::new()
    };

    // In a real pipeline, you would compute these deltas from metrics systems.
    // For now, accept placeholder environment variables for CI integration.
    let l = env::var("AI_LYAPUNOV_IMPROVEMENT")
        .ok()
        .and_then(|s| s.parse::<f32>().ok())
        .unwrap_or(0.0);
    let k = env::var("AI_KER_IMPROVEMENT")
        .ok()
        .and_then(|s| s.parse::<f32>().ok())
        .unwrap_or(0.0);
    let v = env::var("AI_VIOLATION_LATENCY_IMPROVEMENT")
        .ok()
        .and_then(|s| s.parse::<f32>().ok())
        .unwrap_or(0.0);

    let score = compute_always_improve_score(l, k, v);

    let window_id = env::var("AI_WINDOW_ID").unwrap_or_else(|_| "local-window".to_string());
    let release_id = env::var("GITHUB_SHA").unwrap_or_else(|_| "local".to_string());

    let new_window = WindowMetrics {
        window_id,
        releases_included: vec![release_id],
        lyapunov_stability_improvement: l,
        ker_fairness_improvement: k,
        violation_latency_improvement: v,
        always_improve_score: score,
    };

    windows.push(new_window);

    // Enforce ALN constraints.
    if !windows_satisfy_monotonicity(&windows) {
        eprintln!("[AI-META] Always-Improve score is not non-decreasing across windows.");
        exit(1);
    }

    if !last_window_satisfies_minimum_trend(&windows) {
        eprintln!("[AI-META] Last Always-Improve score is below 0.0.");
        exit(1);
    }

    // Persist updated windows file.
    let wf = WindowFile { windows };
    let json = serde_json::to_string_pretty(&wf).expect("serialize windows file");
    if let Err(e) = fs::create_dir_all(
        path_obj.parent().unwrap_or_else(|| Path::new(".")),
    ) {
        eprintln!("[AI-META] Failed to create directory: {e}");
        exit(1);
    }
    let mut f = fs::File::create(path_obj).expect("create windows file");
    if let Err(e) = f.write_all(json.as_bytes()) {
        eprintln!("[AI-META] Failed to write windows file: {e}");
        exit(1);
    }

    println!(
        "[AI-META] Always-Improve window updated. score={:.4}",
        score
    );
}
