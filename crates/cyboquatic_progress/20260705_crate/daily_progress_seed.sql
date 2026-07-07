-- eco_restoration_shard/crates/cyboquatic_progress/20260705_crate/daily_progress_seed.sql
-- Seed script for inserting the 2026-07-05 cyboquatic workload shard into daily_progress.
-- This assumes the daily_progress table exists as created by src/lib.rs.

INSERT INTO daily_progress (
    date_yyyymmdd,
    hex_evidence,
    phoenix_location,
    domain,
    sub_task,
    ker_k,
    ker_e,
    ker_r,
    prev_pointer,
    workload_id
) VALUES (
    '20260705',
    '0x20260705phxlin20260705',
    'Phoenix-AZ-Canal-33.45N-112.07W',
    'cyboquatic_workload_energyreqJ_ΔVt',
    'Phx canal biodegradable liner workload shard',
    0.93,
    0.90,
    0.12,
    NULL,
    'cwk-20260705-PhxLin-20260705'
);
