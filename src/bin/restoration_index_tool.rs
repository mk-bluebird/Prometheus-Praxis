// filename: src/bin/restoration_index_tool.rs
// destination: eco_restoration_shard/src/bin/restoration_index_tool.rs

use std::env;
use std::error::Error;
use std::fmt;
use std::path::Path;

use rusqlite::{params, Connection};

#[derive(Debug)]
enum ToolError {
    Usage,
    Sqlite(rusqlite::Error),
    OpenPath(String),
}

impl fmt::Display for ToolError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ToolError::Usage => write!(
                f,
                "Usage:\n  restoration_index_tool contracts\n  restoration_index_tool planes"
            ),
            ToolError::Sqlite(e) => write!(f, "SQLite error: {}", e),
            ToolError::OpenPath(p) => write!(f, "Failed to open restorationindex.sqlite3 at {}", p),
        }
    }
}

impl Error for ToolError {}

impl From<rusqlite::Error> for ToolError {
    fn from(e: rusqlite::Error) -> Self {
        ToolError::Sqlite(e)
    }
}

fn main() {
    if let Err(e) = run() {
        eprintln!("{}", e);
        std::process::exit(1);
    }
}

fn run() -> Result<(), ToolError> {
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        return Err(ToolError::Usage);
    }

    let command = args[1].as_str();
    let db_path = Path::new("db").join("restorationindex.sqlite3");
    if !db_path.exists() {
        return Err(ToolError::OpenPath(
            db_path.to_string_lossy().to_string(),
        ));
    }

    let conn = Connection::open_with_flags(
        db_path,
        rusqlite::OpenFlags::SQLITE_OPEN_READ_ONLY,
    )?;

    match command {
        "contracts" => list_active_restoration_contracts_primary(&conn)?,
        "planes" => list_prod_eligible_restoration_planes(&conn)?,
        _ => return Err(ToolError::Usage),
    }

    Ok(())
}

fn list_active_restoration_contracts_primary(conn: &Connection) -> Result<(), ToolError> {
    let mut stmt = conn.prepare(
        "SELECT contractid,
                contractname,
                versiontag,
                status,
                bostrom_address,
                region,
                scope,
                kerdeployable,
                prodeligible,
                createdutc,
                updatedutc
         FROM v_active_restoration_contracts_primary
         ORDER BY contractname, versiontag",
    )?;

    let mut rows = stmt.query(params![])?;
    println!("Active restoration contracts bound to PRIMARY Bostrom address:");
    println!("----------------------------------------------------------------");
    while let Some(row) = rows.next()? {
        let contractid: i64 = row.get(0)?;
        let contractname: String = row.get(1)?;
        let versiontag: String = row.get(2)?;
        let status: String = row.get(3)?;
        let bostrom_address: String = row.get(4)?;
        let region: String = row.get(5)?;
        let scope: String = row.get(6)?;
        let kerdeployable: i64 = row.get(7)?;
        let prodeligible: i64 = row.get(8)?;
        let createdutc: String = row.get(9)?;
        let updatedutc: String = row.get(10)?;

        println!(
            "contractid={} name={} version={} status={} region={} scope={} kerdeployable={} prodeligible={} bostrom={} created={} updated={}",
            contractid,
            contractname,
            versiontag,
            status,
            region,
            scope,
            kerdeployable,
            prodeligible,
            bostrom_address,
            createdutc,
            updatedutc
        );
    }

    Ok(())
}

fn list_prod_eligible_restoration_planes(conn: &Connection) -> Result<(), ToolError> {
    let mut stmt = conn.prepare(
        "SELECT planeid,
                plane_name,
                region,
                scope,
                lane,
                kmetric,
                emetric,
                rmetric,
                vtresidual,
                kerdeployable,
                prodeligible,
                createdutc,
                updatedutc
         FROM v_prod_eligible_restoration_planes
         ORDER BY region, plane_name",
    )?;

    let mut rows = stmt.query(params![])?;
    println!("PROD-eligible restoration planes:");
    println!("----------------------------------");
    while let Some(row) = rows.next()? {
        let planeid: i64 = row.get(0)?;
        let plane_name: String = row.get(1)?;
        let region: String = row.get(2)?;
        let scope: String = row.get(3)?;
        let lane: String = row.get(4)?;
        let kmetric: f64 = row.get(5)?;
        let emetric: f64 = row.get(6)?;
        let rmetric: f64 = row.get(7)?;
        let vtresidual: f64 = row.get(8)?;
        let kerdeployable: i64 = row.get(9)?;
        let prodeligible: i64 = row.get(10)?;
        let createdutc: String = row.get(11)?;
        let updatedutc: String = row.get(12)?;

        println!(
            "planeid={} plane={} region={} scope={} lane={} K={:.3} E={:.3} R={:.3} Vt={:.6} kerdeployable={} prodeligible={} created={} updated={}",
            planeid,
            plane_name,
            region,
            scope,
            lane,
            kmetric,
            emetric,
            rmetric,
            vtresidual,
            kerdeployable,
            prodeligible,
            createdutc,
            updatedutc
        );
    }

    Ok(())
}
