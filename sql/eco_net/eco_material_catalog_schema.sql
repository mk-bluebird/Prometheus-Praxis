-- File: sql/eco_net/eco_material_catalog_schema.sql

PRAGMA foreign_keys = ON;

-- Eco material catalog:
-- Stores material identity, ISO/OECD biodegradability metrics, and eco-impact bands
-- for quick lookup by C++/Java/Kotlin tools.[66][72][75]

CREATE TABLE IF NOT EXISTS eco_material_catalog (
    material_id INTEGER PRIMARY KEY AUTOINCREMENT,
    material_name TEXT NOT NULL UNIQUE,
    iso_14851_o2_percent REAL NOT NULL CHECK (iso_14851_o2_percent >= 0.0 AND iso_14851_o2_percent <= 100.0),
    iso_14855_co2_percent REAL NOT NULL CHECK (iso_14855_co2_percent >= 0.0 AND iso_14855_co2_percent <= 100.0),
    oecd_301_bod_percent REAL NOT NULL CHECK (oecd_301_bod_percent >= 0.0 AND oecd_301_bod_percent <= 100.0),
    oecd_301_doc_percent REAL NOT NULL CHECK (oecd_301_doc_percent >= 0.0 AND oecd_301_doc_percent <= 100.0),
    days_to_pass_window REAL NOT NULL CHECK (days_to_pass_window >= 0.0),
    toxicity_score REAL NOT NULL CHECK (toxicity_score >= 0.0 AND toxicity_score <= 1.0),
    pfas_presence REAL NOT NULL CHECK (pfas_presence >= 0.0 AND pfas_presence <= 1.0),
    biodegradability_score REAL NOT NULL CHECK (biodegradability_score >= 0.0 AND biodegradability_score <= 1.0),
    eco_impact_band TEXT NOT NULL CHECK (eco_impact_band IN ('LOW', 'MEDIUM', 'HIGH')),
    createdutc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updatedutc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

-- Index for fast lookup by material name.
CREATE INDEX IF NOT EXISTS idx_eco_material_name
    ON eco_material_catalog (material_name);

-- Index for fast filtering by eco-impact band.
CREATE INDEX IF NOT EXISTS idx_eco_material_band
    ON eco_material_catalog (eco_impact_band);
