// File: crates/materials-eco-knowledge-labeller/src/io.rs
//! Simple JSON line IO for materials and labels.

use std::fs::File;
use std::io::{BufRead, BufReader, BufWriter, Write};
use std::path::Path;

use serde_json::Deserializer;
use thiserror::Error;

use crate::model::{MaterialLabel, MaterialText};

/// Errors that can occur when reading or writing corpora.
#[derive(Debug, Error)]
pub enum CorpusIoError {
    /// Underlying IO error.
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
    /// JSON deserialization error.
    #[error("JSON error: {0}")]
    Json(#[from] serde_json::Error),
}

/// Read a corpus of `MaterialText` from a JSON‑lines file.
///
/// Each line is expected to be a JSON object matching `MaterialText`.
pub fn read_material_texts<P: AsRef<Path>>(path: P) -> Result<Vec<MaterialText>, CorpusIoError> {
    let file = File::open(path)?;
    let reader = BufReader::new(file);

    let stream = Deserializer::from_reader(reader).into_iter::<MaterialText>();
    let mut out = Vec::new();
    for item in stream {
        let mt = item?;
        out.push(mt);
    }
    Ok(out)
}

/// Append a labelled `MaterialLabel` as a JSON line to the given file.
///
/// This function creates the file if it does not exist.
pub fn append_material_label<P: AsRef<Path>>(
    path: P,
    label: &MaterialLabel,
) -> Result<(), CorpusIoError> {
    let file = File::options().create(true).append(true).open(path)?;
    let mut writer = BufWriter::new(file);
    let json = serde_json::to_string(label)?;
    writer.write_all(json.as_bytes())?;
    writer.write_all(b"\n")?;
    writer.flush()?;
    Ok(())
}
