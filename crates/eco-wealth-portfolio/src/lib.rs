// filename: crates/eco-wealth-portfolio/src/lib.rs
// role: Library entrypoint for eco-wealth-portfolio, exposing
//       health-data TCR core types, queries, and mathematically-decodable
//       transformation signatures for Prometheus-Praxis.

pub mod healthdata_tcr_core;

pub use healthdata_tcr_core::{
    // Core domain types
    Address,
    BrainDid,
    LaborSourceKind,
    DataLaborEvent,
    CurationStatus,
    DatasetCategory,
    HealthDataset,
    HealthDatasetChallenge,
    HealthDataLaborCredit,
    HealthQFPool,
    HealthQFCampaign,
    HealthQFContribution,
    HealthQFWeights,
    EcoModelAttestation,

    // Storage-row and query types (DB-facing)
    HealthDatasetRow,
    HealthDatasetChallengeRow,
    HealthDataLaborCreditRow,
    HealthQFPoolRow,
    HealthQFCampaignRow,
    HealthQFContributionRow,

    // Error type
    HealthTcrError,

    // Pure / deterministic constructors and transformers
    new_data_labor_event,
    submit_health_dataset,
    accept_health_dataset,
    reject_health_dataset,
    record_health_data_labor_credit,
    consume_gas_discount,
    open_challenge,
    resolve_challenge_slash_curator,
    compute_qf_weight,
    allocate_qf_matching,

    // DB wiring helpers (to be implemented in healthdata_tcr_core.rs)
    load_health_dataset_by_cid,
    list_health_datasets_for_contributor,
    list_open_challenges_for_dataset,
    insert_health_dataset,
    insert_health_dataset_challenge,
    insert_health_data_labor_credit,
    list_qf_pools_active_at,
    list_qf_campaigns_for_pool,
    list_qf_contributions_for_campaign,
};
