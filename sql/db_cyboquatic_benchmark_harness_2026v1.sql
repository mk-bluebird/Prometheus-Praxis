-- Filename: sql/db_cyboquatic_benchmark_harness_2026v1.sql
-- Destination: sql/db_cyboquatic_benchmark_harness_2026v1.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS cyboquatic_benchmark (
    node_id TEXT NOT NULL PRIMARY KEY,
    shard_id TEXT NOT NULL,
    region TEXT NOT NULL,
    target_lane TEXT NOT NULL,
    expected_carbonnegativeok INTEGER NOT NULL CHECK (expected_carbonnegativeok IN (0, 1)),
    expected_restorationok INTEGER NOT NULL CHECK (expected_restorationok IN (0, 1))
);
