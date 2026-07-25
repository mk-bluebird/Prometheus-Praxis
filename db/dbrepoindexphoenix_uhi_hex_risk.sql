-- filename: db/dbrepoindexphoenix_uhi_hex_risk.sql
-- destination: https://github.com/mk-bluebird/Prometheus-Praxis/db/dbrepoindexphoenix_uhi_hex_risk.sql
-- license: MIT OR Apache-2.0

PRAGMA foreign_keys = ON;

BEGIN TRANSACTION;

-- 1. Register the ALN shard file in repofile.
INSERT OR IGNORE INTO repofile (
    file_id,
    repo_slug,
    path,
    kind,
    lane,
    region,
    author_did,
    description
) VALUES (
    'phoenix.uhi.hex.risk.v1.aln',
    'mk-bluebird/Prometheus-Praxis',
    'qpudatashards/phoenix.uhi.hex.risk.v1.aln',
    'ALN_SHARD',
    'RESEARCH',
    'Phoenix-AZ',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'Phoenix UHI hex risk shard v1: normalized UHI triad and thermal risk for Lyapunov constitution.'
);

-- 2. Register the ecosafety-core-v2 crate and UHI binary in repofile.
INSERT OR IGNORE INTO repofile (
    file_id,
    repo_slug,
    path,
    kind,
    lane,
    region,
    author_did,
    description
) VALUES (
    'crate.ecosafety-core-v2',
    'mk-bluebird/Prometheus-Praxis',
    'crates/ecosafety-core-v2/Cargo.toml',
    'RUST_CRATE',
    'RESEARCH',
    'Phoenix-AZ',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'KER-Lyapunov constitution core with Phoenix UHI triad builder and hex risk harness.'
);

INSERT OR IGNORE INTO repofile (
    file_id,
    repo_slug,
    path,
    kind,
    lane,
    region,
    author_did,
    description
) VALUES (
    'bin.phoenix_uhi_hex_risk',
    'mk-bluebird/Prometheus-Praxis',
    'crates/ecosafety-core-v2/src/bin/phoenix_uhi_hex_risk.rs',
    'RUST_BIN',
    'RESEARCH',
    'Phoenix-AZ',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'Non-actuating Phoenix UHI hex risk generator binary emitting ALN-ready JSONL shards.'
);

-- 3. Register an AI-safe catalog entry so chat agents know they can call this binary.
INSERT OR IGNORE INTO agentsafecatalog (
    tool_id,
    repo_slug,
    kind,
    lane,
    role_band,
    region,
    file_ref,
    summary,
    aichat_allowed,
    aicapability_level
) VALUES (
    'phoenix_uhi_hex_risk',
    'mk-bluebird/Prometheus-Praxis',
    'BIN',
    'RESEARCH',
    'SPINE',
    'Phoenix-AZ',
    'bin.phoenix_uhi_hex_risk',
    'Compute normalized UHI triad risk (r_T, r_C, r_A, r_thermal) for Phoenix hex telemetry and emit ALN-bound JSONL shards.',
    1,
    'READONLYSPINE'
);

-- 4. Register an EcoNet repo manifest hint for this tool in econetrepoindex.
INSERT OR IGNORE INTO econetrepoindex (
    repo_slug,
    role_band,
    lane_default,
    non_actuating_only,
    ecosafety_binding,
    primary_languages,
    contracts_summary
) VALUES (
    'mk-bluebird/Prometheus-Praxis',
    'SPINE',
    'RESEARCH',
    1,
    'ecosafety-core-v2::phoenix.uhi.hex.risk.v1',
    'Rust, ALN, SQL',
    'Non-actuating UHI risk constitution: Phoenix hex thermal risk shards bound to KER-Lyapunov and AI-safe catalog.'
);

COMMIT;
