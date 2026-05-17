// filename: eco_wealth/src/spine.rs

use rusqlite::{params, Connection, Result as SqlResult};
use crate::model::{EcoWealthAmount, EcoWealthSnapshot, EcoWealthUnit, PortfolioId, StewardId};

pub struct SpineConnection {
    conn: Connection,
}

impl SpineConnection {
    pub fn open(path: &str) -> SqlResult<Self> {
        let conn = Connection::open(path)?;
        Ok(Self { conn })
    }
}

pub struct PortfolioQuery<'a> {
    pub portfolio_id: &'a str,
    pub region_code: Option<&'a str>,
}

impl SpineConnection {
    pub fn latest_snapshot(&self, q: &PortfolioQuery) -> SqlResult<Option<EcoWealthSnapshot>> {
        let mut stmt = self.conn.prepare(
            r#"
            SELECT portfolio_id, steward_did, region, tsendutc,
                   wealth_value, wealth_unit,
                   kmetric, emetric, rmetric, vtmax
            FROM eco_wealth_view
            WHERE portfolio_id = ?1
              AND (?2 IS NULL OR region = ?2)
            ORDER BY tsendutc DESC
            LIMIT 1
            "#,
        )?;

        let row_opt = stmt.query_row(
            params![q.portfolio_id, q.region_code.unwrap_or_default()],
            |row| {
                let unit_str: String = row.get(5)?;
                let unit = match unit_str.as_str() {
                    "ECO_WEALTH_POINTS" => EcoWealthUnit::EcoWealthPoints,
                    "TCO2E" => EcoWealthUnit::TCO2e,
                    "KWH_EQ" => EcoWealthUnit::KwhEq,
                    _ => EcoWealthUnit::UsdIndexed,
                };
                Ok(EcoWealthSnapshot {
                    portfolio_id: PortfolioId(row.get(0)?),
                    steward_id: StewardId(row.get(1)?),
                    region_code: row.get(2)?,
                    ts_utc: row.get(3)?,
                    wealth: EcoWealthAmount {
                        value: row.get(4)?,
                        unit,
                    },
                    k: row.get(6)?,
                    e: row.get(7)?,
                    r: row.get(8)?,
                    vt: row.get(9)?,
                })
            },
        ).optional();

        row_opt
    }
}
