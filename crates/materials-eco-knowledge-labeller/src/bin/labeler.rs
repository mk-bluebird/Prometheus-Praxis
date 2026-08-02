// File: crates/materials-eco-knowledge-labeller/src/bin/labeler.rs
use std::io::{self, Write};
use std::path::PathBuf;

use clap::Parser;

use materials_eco_knowledge_labeller::heuristics::{filter_corpus_for_labelling, HeuristicPatterns};
use materials_eco_knowledge_labeller::io::{append_material_label, read_material_texts};
use materials_eco_knowledge_labeller::rubric::make_label;

/// CLI for interactive eco‑knowledge labelling of material descriptions.
///
/// This is intentionally simple: it reads a JSON‑lines corpus of `MaterialText`,
/// filters it using heuristics, and then prompts the annotator for rubric scores
/// for each candidate, writing `MaterialLabel` entries to an output file.
#[derive(Parser, Debug)]
#[command(
    name = "materials-eco-knowledge-labeler",
    about = "Interactive labeller for eco‑knowledge factors on material descriptions"
)]
struct Cli {
    /// Path to input JSON‑lines file containing `MaterialText` objects.
    #[arg(long)]
    input: PathBuf,
    /// Path to output JSON‑lines file where `MaterialLabel` objects will be appended.
    #[arg(long)]
    output: PathBuf,
    /// Annotator id to record in each label.
    #[arg(long)]
    annotator: String,
    /// Minimum heuristic priority for including a material in the labelling session.
    #[arg(long, default_value_t = 0.2)]
    min_priority: f64,
}

fn read_score(prompt: &str) -> f64 {
    loop {
        print!("{prompt}");
        io::stdout().flush().unwrap();
        let mut buf = String::new();
        if io::stdin().read_line(&mut buf).is_err() {
            eprintln!("Failed to read input; try again.");
            continue;
        }
        match buf.trim().parse::<f64>() {
            Ok(v) if (0.0..=1.0).contains(&v) => return v,
            _ => {
                eprintln!("Please enter a number between 0.0 and 1.0.");
            }
        }
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let cli = Cli::parse();

    let materials = read_material_texts(&cli.input)?;
    let patterns = HeuristicPatterns::default();
    let candidates = filter_corpus_for_labelling(materials, &patterns, cli.min_priority);

    println!(
        "Loaded {} candidates for labelling (min_priority = {}).",
        candidates.len(),
        cli.min_priority
    );

    for mt in candidates {
        println!("\n=== Material ID: {} ===", mt.id);
        println!("Source: {}", mt.source);
        println!("---\n{}\n---", mt.text);

        let evidential_score = read_score("Evidence density [0.0–1.0]: ");
        let quantified_score = read_score("Quantified eco attributes [0.0–1.0]: ");
        let transparency_score = read_score("Risk transparency [0.0–1.0]: ");
        let measurability_score = read_score("Measurability [0.0–1.0]: ");

        let label = make_label(
            mt.id.clone(),
            evidential_score,
            quantified_score,
            transparency_score,
            measurability_score,
            cli.annotator.clone(),
        );

        println!(
            "Computed k_material = {:.3} for material {}",
            label.k_material, label.material_id
        );

        append_material_label(&cli.output, &label)?;
    }

    Ok(())
}
