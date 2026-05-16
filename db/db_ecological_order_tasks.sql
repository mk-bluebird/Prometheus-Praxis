-- filename: db_ecological_order_tasks.sql
-- destination: eco_restoration_shard/db/db_ecological_order_tasks.sql

CREATE TABLE IF NOT EXISTS research_task (
    task_id INTEGER PRIMARY KEY AUTOINCREMENT,
    code TEXT NOT NULL UNIQUE,              -- e.g. "T01_PLANE_WEIGHTS_ENGINE"
    title TEXT NOT NULL,
    description TEXT NOT NULL,
    repo_target TEXT NOT NULL,             -- e.g. "eco_restoration_shard", "Cybercore"
    filename TEXT NOT NULL,                -- primary file to create/extend
    crate_name TEXT NOT NULL,              -- Rust crate or module
    priority INTEGER NOT NULL,             -- 1 (highest) .. 5 (lower)
    ker_k_target REAL NOT NULL,            -- expected K band contribution
    ker_e_target REAL NOT NULL,
    ker_r_target REAL NOT NULL,
    lane_hint TEXT NOT NULL,               -- "RESEARCH", "EXPPROD", "PROD"
    notes TEXT NOT NULL
);

INSERT INTO research_task (
    code, title, description, repo_target, filename, crate_name,
    priority, ker_k_target, ker_e_target, ker_r_target, lane_hint, notes
)
VALUES
-- 1. Freeze residual engine + plane weights into a shared crate
(
    "T01_PLANE_WEIGHTS_ENGINE",
    "Shared kerresidual + plane_weights engine",
    "Lift the residual kernel V_t = Σ_j w_j r_j^2 and frozen plane_weights into a single Rust crate that all ecosafety repos must import, wiring it to the plane_weights SQL and forbidding alternative V_t implementations.",
    "eco_restoration_shard",
    "src/kerresidual.rs",
    "kerresidual",
    1,
    0.95, 0.91, 0.13,
    "PROD",
    "Implements the governance spine described in ecosafety_grammar_core.sql and plane_weights, making K/E/R and residual computation a single, shared dependency for Virta-Sys, Eco-Fort, EcoNet, and ecological-orchestrator."
),

-- 2. Ecoper-joule routing hints for QPU / virtual nodes
(
    "T02_ECOPER_JOULE_ROUTER",
    "Ecoper-joule aware workload router",
    "Add a Rust module that reads EcoperJouleRecord rows and suggests node placements that minimize energy-per-gain J_{w,n}, while respecting nonactuating and continuity contracts from the QPU catalog.",
    "eco_restoration_shard",
    "src/ecoper_router.rs",
    "ecoper-joule",
    1,
    0.94, 0.91, 0.13,
    "EXPPROD",
    "Connects ecoper_joule_logs to routing decisions, allowing AI-chat to ask for 'best node per workload' and receive suggestions backed by J_{w,n} and KER-complete telemetry rather than heuristic placement."
),

-- 3. Regional eco-ledger → eco-wealth portfolio linkage
(
    "T03_LEDGER_TO_PORTFOLIO",
    "Regional eco-ledger to eco-wealth portfolio views",
    "Create SQL views and Rust structs that aggregate RegionalEcoLedgerParticle rows into eco_wealth_portfolio_region/by_steward, exposing K/E/R deltas per region and steward DID.",
    "eco_restoration_shard",
    "db/db_eco_wealth_views.sql",
    "eco-wealth-portfolio",
    1,
    0.95, 0.90, 0.13,
    "EXPPROD",
    "Lets AI-chat answer 'what is Phoenix-West eco-wealth delta for steward X over last quarter?' using shard-ledger data instead of ad hoc summaries."
),

-- 4. Hydrology constraints + restoration radius integration
(
    "T04_HYDRO_RADIUS_PIPELINE",
    "Hydrology constraints to restoration radius pipeline",
    "Wire the hydrology-constraints crate and restoration-radius assets into a single service that validates MAR and groundwater actions, computes r_gw and restoration_radius, and emits KER-scored shards.",
    "eco_restoration_shard",
    "src/hydro_radius_pipeline.rs",
    "restoration-radius",
    1,
    0.94, 0.91, 0.13,
    "EXPPROD",
    "Makes basin-specific hydrology constraints and radius assets a default part of the planning loop so AI-chat cannot propose actions that violate r_GW corridors."
),

-- 5. Blastradius hexdescriptor helpers + neighbor queries
(
    "T05_BLASTRADIUS_HELPERS",
    "Blastradius hexdescriptor + neighbor query helpers",
    "Implement helper functions and SQL views that encode/decode blastradius hex descriptors and expose neighbor-aware queries (by hops, meters, hours) for governance reach analysis.",
    "eco_restoration_shard",
    "src/blastradius_helpers.rs",
    "blastradius",
    2,
    0.94, 0.90, 0.13,
    "EXPPROD",
    "Gives AI-chat a compact way to reason about neighborhood impact and sovereignty constraints without streaming full adjacency graphs."
),

-- 6. LaneTrendAnalyzer + promotion CI hook
(
    "T06_LANE_TREND_ANALYZER",
    "LaneTrendAnalyzer and promotion CI integration",
    "Create a LaneTrendAnalyzer that computes residual trends per lane and writes LaneStatus shards, then wire CI gates to block promotions unless K/E/R bands and b≤0 conditions are met.",
    "eco_restoration_shard",
    "src/lanes/lane_trend_analyzer.rs",
    "lane-governance",
    1,
    0.95, 0.91, 0.13,
    "PROD",
    "Aligns lane governance with the residual math: AI-chat can ask 'which lanes are tightening corridors?' and route effort where monotone improvement is proven."
),

-- 7. Topology drift → ProposedFix pipeline
(
    "T07_TOPOLOGY_DRIFT_FIXES",
    "Topology drift to ProposedFix pipeline",
    "Wire topology-risk audits into a message topic that AI-chat agents can consume, generating ProposedFix particles and inserting them into a governance_review_queue.",
    "eco_restoration_shard",
    "src/topology/topology_fix_pipeline.rs",
    "topology-risk",
    2,
    0.94, 0.90, 0.13,
    "RESEARCH",
    "Turns governance drift (missing manifests, mislabelled roles, contract violations) into actionable manifest and contract-fix tasks with DID-signed review trails."
),

-- 8. SensorKerEvaluator and SensorHealth gating
(
    "T08_SENSOR_KER_HEALTH",
    "Sensor KER evaluator and health gating",
    "Implement SensorKerEvaluator and SensorHealth policies that adjust KER trajectories based on multi-sensor consensus, trust weights, and quarantine logic.",
    "eco_restoration_shard",
    "src/sensors/sensor_ker_evaluator.rs",
    "sensor-telemetry",
    1,
    0.94, 0.91, 0.13,
    "EXPPROD",
    "Lets AI-chat reference 'KER backed by healthy sensors' and automatically down-weight or ignore compromised telemetry in eco-wealth decisions."
),

-- 9. EducationPrompt → StewardKnowledgeState wiring
(
    "T09_EDUCATION_KNOWLEDGE_MULTIPLIER",
    "Education prompts to steward knowledge multiplier",
    "Connect EducationPrompt completion events to StewardKnowledgeState updates, and expose a service that computes effective_ker for new ecological particles.",
    "eco_restoration_shard",
    "src/education/knowledge_multiplier.rs",
    "education-prompts",
    2,
    0.95, 0.89, 0.13,
    "RESEARCH",
    "Gives AI-chat a way to factor steward literacy into KER scoring, and to propose lessons that directly raise K for future contributions."
),

-- 10. Eco-pricing ranker service + AI-chat wrapper
(
    "T10_ECO_PRICING_RANK_SERVICE",
    "Eco-pricing ranker service for budget-constrained planning",
    "Expose rank_actions over HTTP as /pricing/rank, taking budget and candidate interventions, and returning a KER-annotated, impact/cost-ranked list.",
    "eco_restoration_shard",
    "src/pricing/pricing_service.rs",
    "eco-pricing",
    2,
    0.94, 0.90, 0.13,
    "EXPPROD",
    "Allows AI-chat to propose portfolios that maximize eco-impact per cost while respecting plane_weights and nonoffsettable bands."
),

-- 11. InteropIndex + CrossSync adapter for external registries
(
    "T11_CROSS_CONSTELLATION_SYNC",
    "Cross-constellation InteropIndex and CrossSync adapter",
    "Implement SyncAdapter and CrossSync service that push/pull EcoNet shards into external carbon/biodiversity/municipal registries with KER bands and DID-based trust anchors.",
    "eco_restoration_shard",
    "src/interop/cross_sync.rs",
    "cross-constellation-index",
    2,
    0.94, 0.90, 0.13,
    "EXPPROD",
    "Lets AI-chat schedule syncs that are provably safe, auditable, and aligned with external trust requirements, expanding eco-wealth into broader systems."
),

-- 12. Large-particle SummaryEngine for metadata-only answers
(
    "T12_LARGE_PARTICLE_SUMMARY_ENGINE",
    "Large-particle SummaryEngine for metadata-first answers",
    "Create a SummaryEngine that maintains aggregate_json for large_particle_blocks and exposes endpoints for answering statistical queries without loading full shards.",
    "eco_restoration_shard",
    "src/large_particle/summary_engine.rs",
    "large-particle-registry",
    3,
    0.93, 0.90, 0.13,
    "EXPPROD",
    "Cuts token and compute cost so AI-chat can answer many eco-queries from summaries, freeing budget for more restoration work."
);
