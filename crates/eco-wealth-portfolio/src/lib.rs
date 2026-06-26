// filename: crates/eco-wealth-portfolio/src/lib.rs
// role: Library entrypoint for eco-wealth-portfolio, exposing healthdata TCR core utilities.

pub mod healthdata_tcr_core;

pub use healthdata_tcr_core::{
    HealthTcrError,
    HealthDatasetRow,
    HealthDatasetChallengeRow,
    HealthDataLaborCreditRow,
    HealthQFPoolRow,
    HealthQFCampaignRow,
    HealthQFContributionRow,
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
