// filename: crates/aln-cyboquatic-codegen/src/bin/aln_codegen_main.rs

//! CLI tool to generate C++ headers and SQL schemas from ALN v2 documents.
//!
//! Usage:
//!   aln-cyboquatic-codegen <aln-json-path> <output-dir-cpp> <output-dir-sql>

#![forbid(unsafe_code)]
#![deny(missing_docs)]

use std::env;
use std::fs;
use std::path::Path;

use aln_cyboquatic_codegen::{AlnDocument, generate_cpp_struct, generate_sql_table};

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 4 {
        eprintln!(
            "Usage: {} <aln-json-path> <output-dir-cpp> <output-dir-sql>",
            args.get(0).unwrap_or(&"aln-cyboquatic-codegen".to_string())
        );
        std::process::exit(1);
    }

    let aln_path = &args[1];
    let out_cpp_dir = &args[2];
    let out_sql_dir = &args[3];

    let data = fs::read_to_string(aln_path).expect("failed to read ALN JSON");
    let doc: AlnDocument = serde_json::from_str(&data).expect("failed to parse ALN JSON");

    fs::create_dir_all(out_cpp_dir).expect("failed to create C++ output dir");
    fs::create_dir_all(out_sql_dir).expect("failed to create SQL output dir");

    for particle in &doc.particles {
        let struct_name = format!("{}_struct", particle.name.replace('.', "_"));
        let table_name = particle.name.replace('.', "_");

        let cpp_code = generate_cpp_struct(particle, &struct_name);
        let sql_code = generate_sql_table(particle, &table_name);

        let cpp_path = Path::new(out_cpp_dir)
            .join(format!("{}.hpp", struct_name));
        let sql_path = Path::new(out_sql_dir)
            .join(format!("{}.sql", table_name));

        fs::write(&cpp_path, cpp_code).expect("failed to write C++ header");
        fs::write(&sql_path, sql_code).expect("failed to write SQL schema");
    }
}
