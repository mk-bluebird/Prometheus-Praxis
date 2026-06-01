-- EcoNet Constellation Index Schema
-- Production-grade cross-repository tracking for KER governance, blast-radius,
-- corridor enforcement, and energy-cost optimization
--
-- Bostrom Anchor: 0xECO_2026_RESTORATION_SHARD_bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
-- Hex-stamp: 0x07ff88aa_constellation_index_2026
-- KER: K=0.95, E=0.92, R=0.10

PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;

CREATE TABLE IF NOT EXISTS repositories (
    repo_id INTEGER PRIMARY KEY AUTOINCREMENT,
    repo_name TEXT UNIQUE NOT NULL,
    github_url TEXT,
    primary_language TEXT CHECK(primary_language IN ('Rust', 'CPP', 'Lua', 'Kotlin', 'ALN', 'Java', 'C')),
    description TEXT,
    bostrom_anchor TEXT DEFAULT '0xECO_2026_RESTORATION_SHARD_bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    created_at TEXT DEFAULT (datetime('now')),
    updated_at TEXT DEFAULT (datetime('now'))
);

CREATE INDEX idx_repositories_name ON repositories(repo_name);
CREATE INDEX idx_repositories_language ON repositories(primary_language);

CREATE TABLE IF NOT EXISTS artifacts (
    artifact_id INTEGER PRIMARY KEY AUTOINCREMENT,
    repo_id INTEGER NOT NULL,
    artifact_path TEXT NOT NULL,
    artifact_type TEXT CHECK(artifact_type IN ('source', 'schema', 'shard', 'kernel', 'test', 'documentation')),
    language TEXT,
    hex_stamp TEXT,
    loc INTEGER DEFAULT 0,
    created_at TEXT DEFAULT (datetime('now')),
    updated_at TEXT DEFAULT (datetime('now')),
    FOREIGN KEY(repo_id) REFERENCES repositories(repo_id) ON DELETE CASCADE,
    UNIQUE(repo_id, artifact_path)
);

CREATE INDEX idx_artifacts_repo ON artifacts(repo_id);
CREATE INDEX idx_artifacts_type ON artifacts(artifact_type);
CREATE INDEX idx_artifacts_hex_stamp ON artifacts(hex_stamp);
CREATE INDEX idx_artifacts_path ON artifacts(artifact_path);

CREATE TABLE IF NOT EXISTS ker_scores (
    score_id INTEGER PRIMARY KEY AUTOINCREMENT,
    artifact_id INTEGER,
    repo_id INTEGER,
    knowledge_factor REAL NOT NULL CHECK(knowledge_factor >= 0.0 AND knowledge_factor <= 1.0),
    eco_impact REAL NOT NULL CHECK(eco_impact >= 0.0 AND eco_impact <= 1.0),
    risk_of_harm REAL NOT NULL CHECK(risk_of_harm >= 0.0 AND risk_of_harm <= 0.30),
    evaluation_date TEXT DEFAULT (datetime('now')),
    evaluator TEXT,
    notes TEXT,
    FOREIGN KEY(artifact_id) REFERENCES artifacts(artifact_id) ON DELETE CASCADE,
    FOREIGN KEY(repo_id) REFERENCES repositories(repo_id) ON DELETE CASCADE
);

CREATE INDEX idx_ker_artifact ON ker_scores(artifact_id);
CREATE INDEX idx_ker_repo ON ker_scores(repo_id);
CREATE INDEX idx_ker_roh ON ker_scores(risk_of_harm);
CREATE INDEX idx_ker_compliance ON ker_scores(knowledge_factor, eco_impact, risk_of_harm);

CREATE TABLE IF NOT EXISTS blast_radius (
    blast_id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_artifact_id INTEGER NOT NULL,
    target_artifact_id INTEGER,
    target_repo_id INTEGER,
    dependency_type TEXT CHECK(dependency_type IN ('import', 'link', 'data_flow', 'schema_ref')),
    impact_severity TEXT NOT NULL CHECK(impact_severity IN ('critical', 'high', 'medium', 'low')),
    import_count INTEGER DEFAULT 1,
    is_public_api BOOLEAN DEFAULT 0,
    notes TEXT,
    created_at TEXT DEFAULT (datetime('now')),
    FOREIGN KEY(source_artifact_id) REFERENCES artifacts(artifact_id) ON DELETE CASCADE,
    FOREIGN KEY(target_artifact_id) REFERENCES artifacts(artifact_id) ON DELETE CASCADE,
    FOREIGN KEY(target_repo_id) REFERENCES repositories(repo_id) ON DELETE CASCADE,
    UNIQUE(source_artifact_id, target_artifact_id)
);

CREATE INDEX idx_blast_source ON blast_radius(source_artifact_id);
CREATE INDEX idx_blast_target ON blast_radius(target_artifact_id);
CREATE INDEX idx_blast_severity ON blast_radius(impact_severity);

CREATE TABLE IF NOT EXISTS energy_metrics (
    metric_id INTEGER PRIMARY KEY AUTOINCREMENT,
    artifact_id INTEGER,
    repo_id INTEGER,
    energy_model TEXT,
    joules_per_cycle REAL NOT NULL CHECK(joules_per_cycle >= 0.0),
    carbon_offset_kg REAL DEFAULT 0.0,
    carbon_intensity_kg_per_kwh REAL,
    measurement_region TEXT DEFAULT 'us-az-phx',
    evaluation_date TEXT DEFAULT (datetime('now')),
    FOREIGN KEY(artifact_id) REFERENCES artifacts(artifact_id) ON DELETE CASCADE,
    FOREIGN KEY(repo_id) REFERENCES repositories(repo_id) ON DELETE CASCADE
);

CREATE INDEX idx_energy_artifact ON energy_metrics(artifact_id);
CREATE INDEX idx_energy_carbon_flag ON energy_metrics(carbon_offset_kg) WHERE carbon_offset_kg >= 0.0;
CREATE INDEX idx_energy_joules ON energy_metrics(joules_per_cycle);

CREATE TABLE IF NOT EXISTS corridor_bands (
    corridor_id INTEGER PRIMARY KEY AUTOINCREMENT,
    parameter_name TEXT NOT NULL UNIQUE,
    safe_min REAL NOT NULL,
    gold_min REAL NOT NULL,
    gold_max REAL NOT NULL,
    hard_max REAL NOT NULL,
    unit TEXT,
    domain TEXT CHECK(domain IN ('hydraulic', 'chemical', 'biological', 'energy', 'materials')),
    notes TEXT,
    version INTEGER DEFAULT 1,
    created_at TEXT DEFAULT (datetime('now')),
    updated_at TEXT DEFAULT (datetime('now')),
    CHECK(safe_min < gold_min AND gold_min < gold_max AND gold_max < hard_max)
);

CREATE INDEX idx_corridor_parameter ON corridor_bands(parameter_name);
CREATE INDEX idx_corridor_domain ON corridor_bands(domain);

CREATE TABLE IF NOT EXISTS corridor_tightening_events (
    event_id INTEGER PRIMARY KEY AUTOINCREMENT,
    corridor_id INTEGER NOT NULL,
    old_hard_max REAL NOT NULL,
    new_hard_max REAL NOT NULL,
    delta_percent REAL NOT NULL,
    trigger_reason TEXT,
    lyapunov_v_before REAL,
    lyapunov_v_after REAL,
    seasonal_lock_until TEXT,
    approved_by TEXT,
    created_at TEXT DEFAULT (datetime('now')),
    FOREIGN KEY(corridor_id) REFERENCES corridor_bands(corridor_id) ON DELETE CASCADE
);

CREATE INDEX idx_tightening_corridor ON corridor_tightening_events(corridor_id);
CREATE INDEX idx_tightening_date ON corridor_tightening_events(created_at);

CREATE VIEW ker_compliance_report AS
SELECT 
    r.repo_name,
    a.artifact_path,
    a.artifact_type,
    k.knowledge_factor AS K,
    k.eco_impact AS E,
    k.risk_of_harm AS R,
    CASE 
        WHEN k.knowledge_factor >= 0.90 AND k.eco_impact >= 0.90 AND k.risk_of_harm <= 0.13 
        THEN 'DEPLOYABLE'
        WHEN k.knowledge_factor >= 0.85 AND k.eco_impact >= 0.85 AND k.risk_of_harm <= 0.20
        THEN 'STAGING'
        ELSE 'RESEARCH'
    END AS lane,
    k.evaluation_date
FROM repositories r
JOIN artifacts a ON r.repo_id = a.repo_id
JOIN ker_scores k ON a.artifact_id = k.artifact_id
ORDER BY k.risk_of_harm DESC;

CREATE VIEW blast_radius_summary AS
SELECT 
    a.artifact_id,
    a.artifact_path,
    r.repo_name,
    COUNT(DISTINCT br_out.target_artifact_id) AS outgoing_dependencies,
    COUNT(DISTINCT br_in.source_artifact_id) AS incoming_dependencies,
    SUM(CASE WHEN br_in.impact_severity = 'critical' THEN 1 ELSE 0 END) AS critical_dependents,
    MAX(CASE WHEN br_in.impact_severity = 'critical' THEN 1 ELSE 0 END) AS has_critical_dependents
FROM artifacts a
JOIN repositories r ON a.repo_id = r.repo_id
LEFT JOIN blast_radius br_out ON a.artifact_id = br_out.source_artifact_id
LEFT JOIN blast_radius br_in ON a.artifact_id = br_in.target_artifact_id
GROUP BY a.artifact_id, a.artifact_path, r.repo_name;

CREATE VIEW energy_dashboard AS
SELECT 
    r.repo_name,
    a.artifact_path,
    em.joules_per_cycle,
    em.carbon_offset_kg,
    em.carbon_intensity_kg_per_kwh,
    CASE 
        WHEN em.carbon_offset_kg >= 0.01 THEN 'CRITICAL_REDESIGN'
        WHEN em.carbon_offset_kg >= 0.0 THEN 'REDESIGN_RECOMMENDED'
        WHEN em.joules_per_cycle > 5000.0 THEN 'ENERGY_HIGH'
        ELSE 'ACCEPTABLE'
    END AS status,
    em.evaluation_date
FROM repositories r
JOIN artifacts a ON r.repo_id = a.repo_id
JOIN energy_metrics em ON a.artifact_id = em.artifact_id
ORDER BY em.carbon_offset_kg DESC;

CREATE VIEW circular_dependency_detection AS
WITH RECURSIVE dep_chain(source_id, target_id, depth, path) AS (
    SELECT 
        source_artifact_id,
        target_artifact_id,
        1,
        CAST(source_artifact_id AS TEXT) || ' -> ' || CAST(target_artifact_id AS TEXT)
    FROM blast_radius
    UNION ALL
    SELECT 
        dc.source_id,
        br.target_artifact_id,
        dc.depth + 1,
        dc.path || ' -> ' || CAST(br.target_artifact_id AS TEXT)
    FROM dep_chain dc
    JOIN blast_radius br ON dc.target_id = br.source_artifact_id
    WHERE dc.depth < 30
)
SELECT DISTINCT
    dc.source_id AS cycle_start_artifact_id,
    dc.target_id AS cycle_end_artifact_id,
    dc.depth AS cycle_length,
    dc.path AS cycle_path,
    a_start.artifact_path AS start_path,
    a_end.artifact_path AS end_path
FROM dep_chain dc
JOIN artifacts a_start ON dc.source_id = a_start.artifact_id
JOIN artifacts a_end ON dc.target_id = a_end.artifact_id
WHERE dc.target_id = dc.source_id
ORDER BY dc.depth;

CREATE TRIGGER update_repositories_timestamp
AFTER UPDATE ON repositories
FOR EACH ROW
BEGIN
    UPDATE repositories SET updated_at = datetime('now')
    WHERE repo_id = NEW.repo_id;
END;

CREATE TRIGGER update_artifacts_timestamp
AFTER UPDATE ON artifacts
FOR EACH ROW
BEGIN
    UPDATE artifacts SET updated_at = datetime('now')
    WHERE artifact_id = NEW.artifact_id;
END;

CREATE TRIGGER update_corridors_timestamp
AFTER UPDATE ON corridor_bands
FOR EACH ROW
BEGIN
    UPDATE corridor_bands SET updated_at = datetime('now')
    WHERE corridor_id = NEW.corridor_id;
END;

INSERT OR IGNORE INTO repositories (repo_name, github_url, primary_language, description) VALUES
('eco_restoration_shard', 'https://github.com/mk-bluebird/eco_restoration_shard', 'Rust', 'Primary constellation orchestrator'),
('EcoNet', 'https://github.com/mk-bluebird/EcoNet', 'Kotlin', 'Mobile validator and field deployment tools'),
('Data_Lake', 'https://github.com/mk-bluebird/Data_Lake', 'Rust', 'QPU data shards and corridor schemas'),
('Cybercore', 'https://github.com/mk-bluebird/Cybercore', 'CPP', 'C/C++ simulation kernels'),
('ALN-Blockchain', 'https://github.com/mk-bluebird/ALN-Blockchain', 'Rust', 'ALN schema definitions and parsers');

INSERT OR IGNORE INTO corridor_bands (parameter_name, safe_min, gold_min, gold_max, hard_max, unit, domain) VALUES
('HLR', 0.0, 0.1, 0.5, 1.0, 'm/h', 'hydraulic'),
('PFAS', 0.0, 0.020, 0.050, 0.070, 'ng/L', 'chemical'),
('CEC', 0.0, 10.0, 50.0, 100.0, 'ng/L', 'chemical'),
('T90', 0.0, 0.5, 2.0, 5.0, 'log10', 'biological'),
('Energy', 0.0, 500.0, 2000.0, 5000.0, 'J/cycle', 'energy'),
('Turbidity', 0.0, 1.0, 5.0, 10.0, 'NTU', 'hydraulic'),
('Phosphorus', 0.0, 0.01, 0.05, 0.10, 'mg/L', 'chemical'),
('Nitrogen', 0.0, 0.5, 2.0, 5.0, 'mg/L', 'chemical');
