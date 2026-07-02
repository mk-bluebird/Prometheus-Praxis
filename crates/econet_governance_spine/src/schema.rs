// Filename: crates/econet_governance_spine/src/schema.rs
// Destination: crates/econet_governance_spine/src/schema.rs

#![forbid(unsafe_code)]

use std::collections::HashMap;

use rusqlite::{Connection, Row};

#[derive(Debug, Clone)]
pub struct ExpectedColumn {
    pub name: String,
    pub data_type: String,
    pub not_null: bool,
}

#[derive(Debug, Clone)]
pub struct ExpectedForeignKey {
    pub table: String,
    pub from_column: String,
    pub to_column: String,
}

#[derive(Debug, Clone)]
pub struct ExpectedTable {
    pub name: String,
    pub columns: Vec<ExpectedColumn>,
    pub foreign_keys: Vec<ExpectedForeignKey>,
}

#[derive(Debug, Clone)]
pub struct ExpectedSchema {
    pub tables: HashMap<String, ExpectedTable>,
}

pub struct SchemaVerifier<'a> {
    conn: &'a Connection,
    expected: ExpectedSchema,
}

impl ExpectedTable {
    pub fn lanestatusshard() -> Self {
        ExpectedTable {
            name: "lanestatusshard".to_string(),
            columns: vec![
                ExpectedColumn {
                    name: "shard_id".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "region".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "lane".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "verdict".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "ker_k".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "ker_e".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "ker_r".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "residual_vt".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "max_staleness_hours".to_string(),
                    data_type: "INTEGER".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "expires_utc".to_string(),
                    data_type: "INTEGER".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "carbonnegativeok".to_string(),
                    data_type: "INTEGER".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "restorationok".to_string(),
                    data_type: "INTEGER".to_string(),
                    not_null: true,
                },
            ],
            foreign_keys: Vec::new(),
        }
    }

    pub fn definitionregistry() -> Self {
        ExpectedTable {
            name: "definitionregistry".to_string(),
            columns: vec![
                ExpectedColumn {
                    name: "defid".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "logicalname".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "repopath".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "category".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "frozen".to_string(),
                    data_type: "INTEGER".to_string(),
                    not_null: true,
                },
            ],
            foreign_keys: Vec::new(),
        }
    }

    pub fn ecounitshard2026v1() -> Self {
        ExpectedTable {
            name: "ecounitshard2026v1".to_string(),
            columns: vec![
                ExpectedColumn {
                    name: "unit_id".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "steward_did".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "shard_id".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
            ],
            foreign_keys: Vec::new(),
        }
    }

    pub fn stewardecowealthstatement() -> Self {
        ExpectedTable {
            name: "stewardecowealthstatement".to_string(),
            columns: vec![
                ExpectedColumn {
                    name: "steward_did".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "shard_id".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "wealth_score".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "k_effective".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "e_effective".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "r_effective".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
            ],
            foreign_keys: Vec::new(),
        }
    }

    pub fn kerresidual() -> Self {
        ExpectedTable {
            name: "kerresidual".to_string(),
            columns: vec![
                ExpectedColumn {
                    name: "shard_id".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "region".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "ker_k".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "ker_e".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "ker_r".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "residual_vt".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
            ],
            foreign_keys: Vec::new(),
        }
    }

    pub fn vresidualkernel() -> Self {
        ExpectedTable {
            name: "vresidualkernel".to_string(),
            columns: vec![
                ExpectedColumn {
                    name: "shard_id".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "region".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "ker_k".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "ker_e".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "ker_r".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "residual_vt".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
            ],
            foreign_keys: Vec::new(),
        }
    }

    pub fn vshardblast() -> Self {
        ExpectedTable {
            name: "vshardblast".to_string(),
            columns: vec![
                ExpectedColumn {
                    name: "nodeid".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "maxcarbonradius".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "maxbiodivradius".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "vtradiussum".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
            ],
            foreign_keys: Vec::new(),
        }
    }

    pub fn vlaneadmissibility() -> Self {
        ExpectedTable {
            name: "vlaneadmissibility".to_string(),
            columns: vec![
                ExpectedColumn {
                    name: "shard_id".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "region".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "lane".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "verdict".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "ker_k".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "ker_e".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "ker_r".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "residual_vt".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "max_staleness_hours".to_string(),
                    data_type: "INTEGER".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "expires_utc".to_string(),
                    data_type: "INTEGER".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "carbonnegativeok".to_string(),
                    data_type: "INTEGER".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "restorationok".to_string(),
                    data_type: "INTEGER".to_string(),
                    not_null: true,
                },
            ],
            foreign_keys: Vec::new(),
        }
    }

    pub fn vcyboquaticecoperjoule() -> Self {
        ExpectedTable {
            name: "vcyboquaticecoperjoule".to_string(),
            columns: vec![
                ExpectedColumn {
                    name: "node_id".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "shard_id".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "eco_per_joule".to_string(),
                    data_type: "REAL".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "carbonnegativeok".to_string(),
                    data_type: "INTEGER".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "restorationok".to_string(),
                    data_type: "INTEGER".to_string(),
                    not_null: true,
                },
            ],
            foreign_keys: Vec::new(),
        }
    }

    pub fn veconet_repo_manifest_agent() -> Self {
        ExpectedTable {
            name: "veconet_repo_manifest_agent".to_string(),
            columns: vec![
                ExpectedColumn {
                    name: "reponame".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "roleband".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "lane".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "description".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
            ],
            foreign_keys: Vec::new(),
        }
    }

    pub fn vagentsafecatalog() -> Self {
        ExpectedTable {
            name: "vagentsafecatalog".to_string(),
            columns: vec![
                ExpectedColumn {
                    name: "objectname".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "kind".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "roleband".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "lanes".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
                ExpectedColumn {
                    name: "reponame".to_string(),
                    data_type: "TEXT".to_string(),
                    not_null: true,
                },
            ],
            foreign_keys: Vec::new(),
        }
    }
}

impl<'a> SchemaVerifier<'a> {
    pub fn new(conn: &'a Connection, expected: ExpectedSchema) -> Self {
        SchemaVerifier { conn, expected }
    }

    pub fn verify(&self) -> Result<(), String> {
        for (name, table) in &self.expected.tables {
            self.verify_table(table).map_err(|e| {
                format!("table '{}': {}", name, e)
            })?;
        }
        Ok(())
    }

    fn verify_table(&self, table: &ExpectedTable) -> Result<(), String> {
        let pragma_sql = format!("PRAGMA table_info({})", table.name);
        let mut stmt = self.conn.prepare(&pragma_sql).map_err(|e| e.to_string())?;
        let mut rows = stmt
            .query([])
            .map_err(|e| e.to_string())?;
        let mut actual_cols = Vec::new();
        while let Some(row) = rows.next().map_err(|e| e.to_string())? {
            actual_cols.push(Self::map_table_info(row)?);
        }

        for expected_col in &table.columns {
            let found = actual_cols.iter().find(|c| c.name == expected_col.name);
            let actual = match found {
                Some(c) => c,
                None => {
                    return Err(format!(
                        "missing column '{}' on table '{}'",
                        expected_col.name, table.name
                    ))
                }
            };
            if actual.not_null != expected_col.not_null {
                return Err(format!(
                    "column '{}' on table '{}' has not_null={}, expected={}",
                    expected_col.name, table.name, actual.not_null, expected_col.not_null
                ));
            }
        }

        Ok(())
    }

    fn map_table_info(row: Row) -> Result<ExpectedColumn, String> {
        let name: String = row
            .get(1)
            .map_err(|e| e.to_string())?;
        let data_type: String = row
            .get(2)
            .map_err(|e| e.to_string())?;
        let not_null_flag: i64 = row
            .get(3)
            .map_err(|e| e.to_string())?;
        Ok(ExpectedColumn {
            name,
            data_type,
            not_null: not_null_flag != 0,
        })
    }
}
