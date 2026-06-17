// filename: ecorestorationshard/examples/demo_ersi_logger.rs
use ersilogger::log_ersi_event;

fn main() {
    // Example usage for a single AI interaction.
    // db_path can be an existing EcoNet/EcoFort research DB or a dedicated ERSI DB.
    let db_path = "ecorestorationshard_ersi.sqlite3";

    let result = log_ersi_event(
        db_path,
        "ersi-2026-06-16-0001",
        "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7",
        "ecorestorationshard",
        "RESEARCH",
        2048,          // tokens_used
        0.85,          // compute_joules (example small inference step)
        0.96,          // kfactor from KnowledgeFactorKernel
        0.91,          // efactor from EcoImpactKernel
        0.08,          // rfactor from residual kernel
        "biodegradable-substrate-grammar",
    );

    if let Err(e) = result {
        eprintln!("ERSI logging failed: {e}");
    }
}
