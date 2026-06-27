//! EcoNet Index Loader
//!
//! This library reads CSV index files from `Data_Lake/index/` and loads them
//! into the SQLite `econet_index` table.
//!
//! ## Usage
//!
//! ```rust,no_run
//! use econet_index_loader::{EcoNetIndexRow, load_csv_files};
//! use rusqlite::Connection;
//!
//! let conn = Connection::open("econet.db").unwrap();
//! let rows = load_csv_files("Data_Lake/index").unwrap();
//! for row in rows {
//!     row.insert_or_update(&conn).unwrap();
//! }
//! ```

use rusqlite::{Connection, params};
use serde::Deserialize;
use std::fs;
use std::path::{Path, PathBuf};
use thiserror::Error;

/// Error types for the index loader.
#[derive(Error, Debug)]
pub enum LoaderError {
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),

    #[error("CSV parse error: {0}")]
    Csv(#[from] csv::Error),

    #[error("SQLite error: {0}")]
    Sqlite(#[from] rusqlite::Error),

    #[error("Invalid brain_identity_relevance value: {value} (must be 0-10)")]
    InvalidBrainRelevance { value: i64 },

    #[error("Duplicate ID detected: {id}")]
    DuplicateId { id: i64 },

    #[error("Missing required field: {field} in file {file}")]
    MissingField { field: String, file: String },
}

/// A row from the EcoNet index CSV.
#[derive(Debug, Clone, Deserialize)]
pub struct EcoNetIndexRow {
    pub id: i64,
    pub filename: String,
    pub repo: String,
    pub destination_hint: String,
    pub primary_role: String,
    pub language: String,
    pub brain_identity_relevance: i64,
    pub eco_impact_focus: String,
}

impl EcoNetIndexRow {
    /// Validate the row data.
    pub fn validate(&self) -> Result<(), LoaderError> {
        if self.brain_identity_relevance < 0 || self.brain_identity_relevance > 10 {
            return Err(LoaderError::InvalidBrainRelevance {
                value: self.brain_identity_relevance,
            });
        }
        Ok(())
    }

    /// Insert or update this row in the database.
    pub fn insert_or_update(&self, conn: &Connection) -> Result<(), LoaderError> {
        // Check for duplicate ID
        let exists: bool = conn.query_row(
            "SELECT EXISTS(SELECT 1 FROM econet_index WHERE id = ?1)",
            params![self.id],
            |row| row.get(0),
        )?;

        if exists {
            // Update existing row
            conn.execute(
                "UPDATE econet_index SET
                    filename = ?1,
                    repo = ?2,
                    destination_hint = ?3,
                    primary_role = ?4,
                    language = ?5,
                    brain_identity_relevance = ?6,
                    eco_impact_focus = ?7
                 WHERE id = ?8",
                params![
                    self.filename,
                    self.repo,
                    self.destination_hint,
                    self.primary_role,
                    self.language,
                    self.brain_identity_relevance,
                    self.eco_impact_focus,
                    self.id,
                ],
            )?;
        } else {
            // Insert new row
            conn.execute(
                "INSERT INTO econet_index (id, filename, repo, destination_hint, primary_role, language, brain_identity_relevance, eco_impact_focus)
                 VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)",
                params![
                    self.id,
                    self.filename,
                    self.repo,
                    self.destination_hint,
                    self.primary_role,
                    self.language,
                    self.brain_identity_relevance,
                    self.eco_impact_focus,
                ],
            )?;
        }

        Ok(())
    }
}

/// Load all CSV files from a directory and parse them into EcoNetIndexRow structs.
pub fn load_csv_files<P: AsRef<Path>>(dir: P) -> Result<Vec<EcoNetIndexRow>, LoaderError> {
    let mut rows = Vec::new();
    let dir_path = dir.as_ref();

    if !dir_path.exists() {
        return Ok(rows);
    }

    for entry in fs::read_dir(dir_path)? {
        let entry = entry?;
        let path = entry.path();

        if path.extension().map_or(false, |ext| ext == "csv") {
            let file_rows = parse_csv_file(&path)?;
            rows.extend(file_rows);
        }
    }

    Ok(rows)
}

/// Parse a single CSV file into EcoNetIndexRow structs.
pub fn parse_csv_file<P: AsRef<Path>>(path: P) -> Result<Vec<EcoNetIndexRow>, LoaderError> {
    let path = path.as_ref();
    let file_name = path
        .file_name()
        .and_then(|n| n.to_str())
        .unwrap_or("unknown")
        .to_string();

    let mut reader = csv::Reader::from_path(path)?;
    let mut rows = Vec::new();

    for result in reader.deserialize() {
        let row: EcoNetIndexRow = result?;
        row.validate()?;
        rows.push(row);
    }

    Ok(rows)
}

/// Initialize the database schema from a SQL file.
pub fn init_schema<P: AsRef<Path>>(conn: &Connection, schema_path: P) -> Result<(), LoaderError> {
    let sql = fs::read_to_string(schema_path)?;
    conn.execute_batch(&sql)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use rusqlite::Connection;
    use std::fs::File;
    use std::io::Write;
    use tempfile::tempdir;

    #[test]
    fn test_validate_brain_relevance() {
        let row = EcoNetIndexRow {
            id: 1,
            filename: "test.csv".to_string(),
            repo: "test".to_string(),
            destination_hint: "test".to_string(),
            primary_role: "test".to_string(),
            language: "Rust".to_string(),
            brain_identity_relevance: 11,
            eco_impact_focus: "test".to_string(),
        };
        assert!(row.validate().is_err());

        let row = EcoNetIndexRow {
            brain_identity_relevance: -1,
            ..row
        };
        assert!(row.validate().is_err());

        let row = EcoNetIndexRow {
            brain_identity_relevance: 5,
            ..row
        };
        assert!(row.validate().is_ok());
    }

    #[test]
    fn test_insert_and_update() {
        let dir = tempdir().unwrap();
        let db_path = dir.path().join("test.db");
        let conn = Connection::open(&db_path).unwrap();

        // Create table
        conn.execute(
            "CREATE TABLE econet_index (
                id INTEGER PRIMARY KEY,
                filename TEXT NOT NULL,
                repo TEXT NOT NULL,
                destination_hint TEXT NOT NULL,
                primary_role TEXT NOT NULL,
                language TEXT NOT NULL,
                brain_identity_relevance INTEGER NOT NULL,
                eco_impact_focus TEXT NOT NULL
            )",
            [],
        )
        .unwrap();

        let row = EcoNetIndexRow {
            id: 1,
            filename: "test.csv".to_string(),
            repo: "test".to_string(),
            destination_hint: "test".to_string(),
            primary_role: "eco_reward_design".to_string(),
            language: "Rust".to_string(),
            brain_identity_relevance: 8,
            eco_impact_focus: "energy_reduction".to_string(),
        };

        // Insert
        row.insert_or_update(&conn).unwrap();

        // Verify insert
        let count: i64 = conn
            .query_row("SELECT COUNT(*) FROM econet_index", [], |row| row.get(0))
            .unwrap();
        assert_eq!(count, 1);

        // Update
        let updated_row = EcoNetIndexRow {
            filename: "updated.csv".to_string(),
            ..row.clone()
        };
        updated_row.insert_or_update(&conn).unwrap();

        // Verify update
        let filename: String = conn
            .query_row("SELECT filename FROM econet_index WHERE id = 1", [], |row| {
                row.get(0)
            })
            .unwrap();
        assert_eq!(filename, "updated.csv");
    }
}
