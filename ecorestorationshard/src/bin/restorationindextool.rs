// Filename: ecorestorationshard/src/bin/restorationindextool.rs
// Destination: ecorestorationshard/src/bin/restorationindextool.rs
// Repo-target: github.com/mk-bluebird/eco_restoration_shard

#![forbid(unsafe_code)]
#![warn(missing_docs)]

//! Restoration index CLI tool for Phoenix-AZ governance DB.
//!
//! This binary is strictly non-actuating. It opens the
//! `restorationindex.sqlite3` shard in read-only mode and exposes
//! a small, governance-first query surface over lanes, contracts,
//! and PROD-eligible restoration planes. All outputs are advisory,
//! suitable for CI, AI-chat agents, and human stewards.

use std::env;
use std::path::Path;
use std::process::ExitCode;

use chrono::{DateTime, Utc};
use ecorestorationshard::ShardIndex;
use log::{error, info};
use rusqlite::{Connection, OpenFlags};

/// High-level commands supported by this tool.
#[derive(Debug, Clone)]
enum Command {
    /// Print a summary banner and basic health checks.
    Summary,
    /// List active restoration contracts for the primary Bostrom address.
    ContractsPrimary,
    /// List PROD-eligible restoration planes.
    ProdPlanes,
    /// Validate EcoWealth windows ordering.
    ValidateEcoWealth,
}

impl Command {
    fn from_args(args: &[String]) -> Option<Self> {
        if args.is_empty() {
            return Some(Command::Summary);
        }
        match args[0].as_str() {
            "summary" => Some(Command::Summary),
            "contracts:primary" => Some(Command::ContractsPrimary),
            "planes:prod" => Some(Command::ProdPlanes),
            "validate:ecowealth" => Some(Command::ValidateEcoWealth),
            _ => None,
        }
    }
}

/// Read-only wrapper around the Phoenix restoration governance DB.
struct GovernanceDb {
    path: String,
    conn: Connection,
}

impl GovernanceDb {
    /// Open the DB in strict read-only mode using `immutable=1` semantics.
    fn open<P: AsRef<Path>>(db_path: P) -> Result<Self, rusqlite::Error> {
        let path_str = db_path.as_ref().to_string_lossy().to_string();
        let uri = format!("file:{}?mode=ro&immutable=1", path_str);
        let conn = Connection::open_with_flags(
            uri,
            OpenFlags::SQLITE_OPEN_READONLY | OpenFlags::SQLITE_OPEN_URI,
        )?;
        conn.pragma_update(None, "foreign_keys", &"ON")?;
        Ok(Self {
            path: path_str,
            conn,
        })
    }

    /// List active restoration contracts bound to the primary Bostrom address.
    fn list_active_restoration_contracts_primary(&self) -> Result<(), rusqlite::Error> {
        let sql = r#"
            SELECT c.logicalname,
                   c.versiontag,
                   c.region,
                   c.status
            FROM restorationcontract AS c
            JOIN bostromcontractbinding AS b
              ON b.contractid = c.contractid
            JOIN bostromaddress AS a
              ON a.addressid = b.addressid
            WHERE a.addresstext = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
              AND c.status = 'FROZENACTIVE'
            ORDER BY c.logicalname, c.region;
        "#;

        let mut stmt = self.conn.prepare(sql)?;
        let mut rows = stmt.query([])?;

        println!("active_restoration_contracts_primary:");
        while let Some(row) = rows.next()? {
            let logicalname: String = row.get(0)?;
            let versiontag: String = row.get(1)?;
            let region: String = row.get(2)?;
            let status: String = row.get(3)?;
            println!(
                "  - logicalname={logicalname}, version={versiontag}, region={region}, status={status}"
            );
        }

        Ok(())
    }

    /// List PROD-eligible restoration planes via `vprodeligiblerestorationplanes`.
    fn list_prod_eligible_restoration_planes(&self) -> Result<(), rusqlite::Error> {
        let sql = r#"
            SELECT planeid,
                   planename,
                   region,
                   lane,
                   kmetric,
                   emetric,
                   rmetric,
                   vtresidual
            FROM vprodeligiblerestorationplanes
            ORDER BY region, planename;
        "#;

        let mut stmt = self.conn.prepare(sql)?;
        let mut rows = stmt.query([])?;

        println!("prod_eligible_restoration_planes:");
        while let Some(row) = rows.next()? {
            let planeid: i64 = row.get(0)?;
            let planename: String = row.get(1)?;
            let region: String = row.get(2)?;
            let lane: String = row.get(3)?;
            let kmetric: f64 = row.get(4)?;
            let emetric: f64 = row.get(5)?;
            let rmetric: f64 = row.get(6)?;
            let vtresidual: f64 = row.get(7)?;
            println!(
                "  - planeid={planeid}, name={planename}, region={region}, lane={lane}, \
                 K={kmetric:.3}, E={emetric:.3}, R={rmetric:.3}, Vt={vtresidual:.5}"
            );
        }

        Ok(())
    }
}

fn print_usage() {
    eprintln!(
        "usage: restorationindextool <db_path> [command]\n\
         \n\
         commands:\n\
         \n\
         summary               Health-check DefinitionRegistry and EcoWealth windows.\n\
         contracts:primary     List active restoration contracts for primary Bostrom.\n\
         planes:prod           List PROD-eligible restoration planes.\n\
         validate:ecowealth    Validate EcoWealth window ordering.\n"
    );
}

fn main() -> ExitCode {
    env_logger::init();

    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        print_usage();
        return ExitCode::from(1);
    }

    let db_path = &args[1];
    let cmd = match Command::from_args(&args[2..]) {
        Some(c) => c,
        None => {
            eprintln!("unknown or missing command\n");
            print_usage();
            return ExitCode::from(1);
        }
    };

    // Use ShardIndex for shared EcoWealth/DefinitionRegistry checks.
    let shard_index = match ShardIndex::open_readonly(db_path) {
        Ok(idx) => idx,
        Err(err) => {
            error!("Failed to open governance DB via ShardIndex: {err}");
            return ExitCode::from(1);
        }
    };

    let db = match GovernanceDb::open(db_path) {
        Ok(db) => db,
        Err(err) => {
            error!("Failed to open governance DB in read-only mode: {err}");
            return ExitCode::from(1);
        }
    };

    match cmd {
        Command::Summary => {
            info!("Running summary over restorationindex.sqlite3 at {}", db.path);
            match shard_index.active_definition_count() {
                Ok(count) => println!("activedefinitioncount={}", count),
                Err(err) => {
                    error!("Failed to count active definitions: {err}");
                    return ExitCode::from(1);
                }
            }

            match shard_index.validate_ecowealth_windows() {
                Ok(true) => println!("ecowealthwindowsok=true"),
                Ok(false) => {
                    println!("ecowealthwindowsok=false");
                    return ExitCode::from(2);
                }
                Err(err) => {
                    error!("Failed to validate EcoWealth windows: {err}");
                    return ExitCode::from(1);
                }
            }

            // Simple timestamp banner for CI/agents.
            let now: DateTime<Utc> = Utc::now();
            println!("timestamp_utc={}", now.to_rfc3339());
            ExitCode::SUCCESS
        }
        Command::ContractsPrimary => {
            info!(
                "Listing active restoration contracts for primary Bostrom on DB {}",
                db.path
            );
            if let Err(err) = db.list_active_restoration_contracts_primary() {
                error!("Failed to list contracts: {err}");
                return ExitCode::from(1);
            }
            ExitCode::SUCCESS
        }
        Command::ProdPlanes => {
            info!(
                "Listing PROD-eligible restoration planes from vprodeligiblerestorationplanes on {}",
                db.path
            );
            if let Err(err) = db.list_prod_eligible_restoration_planes() {
                error!("Failed to list PROD-eligible planes: {err}");
                return ExitCode::from(1);
            }
            ExitCode::SUCCESS
        }
        Command::ValidateEcoWealth => {
            info!(
                "Validating EcoWealth window ordering in StewardEcoWealthStatement on {}",
                db.path
            );
            match shard_index.validate_ecowealth_windows() {
                Ok(true) => {
                    println!("ecowealthwindowsok=true");
                    ExitCode::SUCCESS
                }
                Ok(false) => {
                    println!("ecowealthwindowsok=false");
                    ExitCode::from(2)
                }
                Err(err) => {
                    error!("Failed to validate EcoWealth windows: {err}");
                    ExitCode::from(1)
                }
            }
        }
    }
}
