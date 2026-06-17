-- filename: dbcyboquatic_researchtasks.sql
-- destination: ecorestorationshard/sql/dbcyboquatic_researchtasks.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS cybo_research_task (
    taskid       INTEGER PRIMARY KEY AUTOINCREMENT,
    code         TEXT NOT NULL UNIQUE,
    title        TEXT NOT NULL,
    description  TEXT NOT NULL,
    repotarget   TEXT NOT NULL,   -- e.g. ecorestorationshard
    filename     TEXT NOT NULL,   -- primary file to create/extend
    cratename    TEXT NOT NULL,   -- Rust crate or module
    priority     INTEGER NOT NULL, -- 1 highest .. 5 lower
    ker_k_target REAL NOT NULL,
    ker_e_target REAL NOT NULL,
    ker_r_target REAL NOT NULL,
    lanehint     TEXT NOT NULL,   -- RESEARCH, EXPPROD, PROD
    notes        TEXT NOT NULL
);

INSERT INTO cybo_research_task (
    code, title, description, repotarget, filename, cratename,
    priority, ker_k_target, ker_e_target, ker_r_target, lanehint, notes
) VALUES
-- 1. Biodegradable tray-line material KER kernel
(
    'C01_TRAY_MATERIAL_KERNEL',
    'Cyboquatic biodegradable tray material KER kernel',
    'Implement a Rust module that computes rmassloss, rtox, rmicro, rcarbon, and rbiodiv for Cyboquatic tray-line substrates, wired to ecosafety corridors and exposed via the cyboquatic_spine cdylib.',
    'ecorestorationshard',
    'src/trayline/material_ker.rs',
    'cyboquatic_spine',
    1,
    0.95, 0.91, 0.13,
    'RESEARCH',
    'Adds explicit risk coordinates for biodegradable tray substrates so AI-chat can query material choices with KER-backed safety bands instead of heuristics.'
),
-- 2. Cyboquatic energy-per-ecogain router
(
    'C02_ECOPERJ_ROUTER',
    'Cyboquatic eco-per-joule router',
    'Create a read-only routing helper that ranks Cyboquatic nodes by eco-impact per joule using cybo_workload_ledger and blast-radius metrics, returning JSON envelopes for planning without actuation.',
    'ecorestorationshard',
    'src/router/ecoperj_cybo.rs',
    'cyboquatic_spine',
    1,
    0.94, 0.91, 0.13,
    'EXPPROD',
    'Lets AI-chat ask for best nodes to run a workload with minimal energy per eco-gain, grounded in real KER and blast-radius telemetry.'
),
-- 3. Blast-radius corridor tightening analysis
(
    'C03_BLASTRADIUS_TIGHTEN',
    'Cyboquatic blast-radius corridor tightening analyzer',
    'Add a Rust analysis tool that scans v_cybo_node_blastradius and v_cybo_workload_window for nodes where mean_delta_vt < 0 and risk coordinates are shrinking, emitting LaneStatus shards for corridor tightening.',
    'ecorestorationshard',
    'src/analysis/cybo_blastradius_tighten.rs',
    'cyboquatic_spine',
    2,
    0.95, 0.90, 0.13,
    'EXPPROD',
    'Gives AI-chat a direct way to identify Cyboquatic machinery bundles that are provably improving safety over time, focusing human attention where eco-restorative returns are highest.'
),
-- 4. Eco-restorative MAR + Cyboquatic linkage
(
    'C04_MAR_CYBO_LINK',
    'MAR recharge to Cyboquatic workload linkage',
    'Design SQL views and Rust structs that tie MAR recharge shards to cybo_workload_ledger entries at the same node/region, exposing combined water and energy/biodiversity risk bands as JSON.',
    'ecorestorationshard',
    'sql/dbmar_cybo_link.sql',
    'cyboquatic_spine',
    2,
    0.94, 0.91, 0.13,
    'EXPPROD',
    'Enables AI-chat to reason about MAR vaults and Cyboquatic machinery as a joint eco-restoration bundle rather than isolated systems.'
),
-- 5. AI-chat vocabulary and template generator
(
    'C05_AI_CHAT_VOCAB',
    'Cyboquatic eco-restoration AI-chat vocabulary templates',
    'Create metadata-only JSON templates that describe Cyboquatic blast-radius, workload, and biodegradable material fields in natural language, to guide AI-chat toward corridor-aligned queries and answers.',
    'ecorestorationshard',
    'docs/cybo_ai_chat_vocab.json',
    'cyboquatic_spine',
    3,
    0.95, 0.89, 0.13,
    'RESEARCH',
    'Improves future AI-chat responses by binding terms like Vt, rmassloss, rcarbon, and eco-per-joule to explicit schema fields and governance rules.'
);
