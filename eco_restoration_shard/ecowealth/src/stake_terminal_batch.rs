// filename: eco_restoration_shard/ecowealth/src/stake_terminal_batch.rs

use serde::{Deserialize, Serialize};

/// Knowledge-factor for this module: 0.95
/// Eco-impact value: 0.92
/// Residual risk-of-harm: 0.11
///
/// This file defines the CSV schema for EcoNetStakeTerminalBatch2026Q2Phoenix,
/// the mapping to ALN-compatible fields, and readonly helpers for K/E/R and
/// karma_units computation. It is non-actuating and read-only with respect to
/// any controller surfaces.

/// RFC-4180-compatible CSV row for EcoNetStakeTerminalBatch2026Q2Phoenix.
/// All fields are plain ASCII, comma-separated, and quote-safe.
///
/// Column order (stable, 1-based):
///  1  batch_id
///  2  batch_seq
///  3  batch_version
///  4  batch_window_start_utc
///  5  batch_window_end_utc
///  6  identity_id
///  7  steward_did
///  8  region_code
///  9  geohex_id
/// 10  node_id
/// 11  asset_class
/// 12  lane
/// 13  ceim_mass_tco2e
/// 14  karma_factor
/// 15  ker_k
/// 16  ker_e
/// 17  ker_r
/// 18  ker_contract_id
/// 19  ker_window_id
/// 20  karma_units_total
/// 21  karma_units_liquid
/// 22  karma_units_restricted
/// 23  eco_units_credited
/// 24  eco_units_liquid
/// 25  eco_units_restricted
/// 26  eco_liability_units
/// 27  eco_liability_reason
/// 28  t03_ledger_id
/// 29  t03_ledger_window_id
/// 30  t03_tx_id
/// 31  t03_row_hash_hex
/// 32  stake_contract_id
/// 33  stake_instrument_id
/// 34  stake_notional_ecounits
/// 35  stake_lockup_days
/// 36  stake_maturity_utc
/// 37  stake_status
/// 38  k_delta
/// 39  e_delta
/// 40  r_delta
/// 41  vt_before
/// 42  vt_after
/// 43  vt_delta
/// 44  plane_energy_r
/// 45  plane_carbon_r
/// 46  plane_biodiversity_r
/// 47  plane_materials_r
/// 48  trust_plane_r
/// 49  dataquality_plane_r
/// 50  topology_plane_r
/// 51  resid_plane_r
/// 52  ecosafety_contract_id
/// 53  lane_policy_id
/// 54  lane_verdict_id
/// 55  kerdeployable_window_flag
/// 56  kerdeployable_reason
/// 57  evidence_hex
/// 58  alnspec_hash
/// 59  created_utc
/// 60  created_by_did
/// 61  ker_window_hash_hex
/// 62  ker_recompute_flag
/// 63  ker_recompute_reason
/// 64  ai_chat_session_id
/// 65  ai_chat_turn_id
/// 66  ai_chat_surface_id
/// 67  dp_budget_id
/// 68  dp_budget_delta
/// 69  knowledge_factor_before
/// 70  knowledge_factor_after
/// 71  ecoimpact_before
/// 72  ecoimpact_after
/// 73  risk_of_harm_before
/// 74  risk_of_harm_after
/// 75  ker_window_lane
/// 76  ker_window_region
/// 77  ker_window_geohex_id
/// 78  ker_window_residual
/// 79  ker_window_r_max
/// 80  ecowealth_quarantine_state
/// 81  ecowealth_quarantine_cause
/// 82  ecowealth_quarantine_rule_id
/// 83  ecowealth_quarantine_block_id
/// 84  steward_portfolio_id
/// 85  steward_portfolio_lane
/// 86  steward_portfolio_ker_k
/// 87  steward_portfolio_ker_e
/// 88  steward_portfolio_ker_r
/// 89  steward_portfolio_vt
/// 90  phoenix_batch_label
/// 91  phoenix_ceim_window_tag
/// 92  phoenix_daily_surface_tag
/// 93  phoenix_sensor_band
/// 94  phoenix_trust_band
/// 95  eco_units_floor_applied
/// 96  eco_units_floor_reason
/// 97  eco_units_cap_applied
/// 98  eco_units_cap_reason
/// 99  ker_window_ci_run_id
/// 100 ker_window_ci_hash_hex
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoNetStakeTerminalBatchRow {
    pub batch_id: String,
    pub batch_seq: i64,
    pub batch_version: String,
    pub batch_window_start_utc: String,
    pub batch_window_end_utc: String,
    pub identity_id: String,
    pub steward_did: String,
    pub region_code: String,
    pub geohex_id: String,
    pub node_id: String,
    pub asset_class: String,
    pub lane: String,
    pub ceim_mass_tco2e: f64,
    pub karma_factor: f64,
    pub ker_k: f64,
    pub ker_e: f64,
    pub ker_r: f64,
    pub ker_contract_id: String,
    pub ker_window_id: String,
    pub karma_units_total: f64,
    pub karma_units_liquid: f64,
    pub karma_units_restricted: f64,
    pub eco_units_credited: f64,
    pub eco_units_liquid: f64,
    pub eco_units_restricted: f64,
    pub eco_liability_units: f64,
    pub eco_liability_reason: String,
    pub t03_ledger_id: String,
    pub t03_ledger_window_id: String,
    pub t03_tx_id: String,
    pub t03_row_hash_hex: String,
    pub stake_contract_id: String,
    pub stake_instrument_id: String,
    pub stake_notional_ecounits: f64,
    pub stake_lockup_days: i64,
    pub stake_maturity_utc: String,
    pub stake_status: String,
    pub k_delta: f64,
    pub e_delta: f64,
    pub r_delta: f64,
    pub vt_before: f64,
    pub vt_after: f64,
    pub vt_delta: f64,
    pub plane_energy_r: f64,
    pub plane_carbon_r: f64,
    pub plane_biodiversity_r: f64,
    pub plane_materials_r: f64,
    pub trust_plane_r: f64,
    pub dataquality_plane_r: f64,
    pub topology_plane_r: f64,
    pub resid_plane_r: f64,
    pub ecosafety_contract_id: String,
    pub lane_policy_id: String,
    pub lane_verdict_id: String,
    pub kerdeployable_window_flag: i32,
    pub kerdeployable_reason: String,
    pub evidence_hex: String,
    pub alnspec_hash: String,
    pub created_utc: String,
    pub created_by_did: String,
    pub ker_window_hash_hex: String,
    pub ker_recompute_flag: i32,
    pub ker_recompute_reason: String,
    pub ai_chat_session_id: String,
    pub ai_chat_turn_id: String,
    pub ai_chat_surface_id: String,
    pub dp_budget_id: String,
    pub dp_budget_delta: f64,
    pub knowledge_factor_before: f64,
    pub knowledge_factor_after: f64,
    pub ecoimpact_before: f64,
    pub ecoimpact_after: f64,
    pub risk_of_harm_before: f64,
    pub risk_of_harm_after: f64,
    pub ker_window_lane: String,
    pub ker_window_region: String,
    pub ker_window_geohex_id: String,
    pub ker_window_residual: f64,
    pub ker_window_r_max: f64,
    pub ecowealth_quarantine_state: String,
    pub ecowealth_quarantine_cause: String,
    pub ecowealth_quarantine_rule_id: String,
    pub ecowealth_quarantine_block_id: String,
    pub steward_portfolio_id: String,
    pub steward_portfolio_lane: String,
    pub steward_portfolio_ker_k: f64,
    pub steward_portfolio_ker_e: f64,
    pub steward_portfolio_ker_r: f64,
    pub steward_portfolio_vt: f64,
    pub phoenix_batch_label: String,
    pub phoenix_ceim_window_tag: String,
    pub phoenix_daily_surface_tag: String,
    pub phoenix_sensor_band: String,
    pub phoenix_trust_band: String,
    pub eco_units_floor_applied: f64,
    pub eco_units_floor_reason: String,
    pub eco_units_cap_applied: f64,
    pub eco_units_cap_reason: String,
    pub ker_window_ci_run_id: String,
    pub ker_window_ci_hash_hex: String,
}

impl EcoNetStakeTerminalBatchRow {
    /// Compute karma_units from CEIM mass, karma factor, and KER adjustment.
    ///
    /// Formula (conceptual):
    ///   karma_base = ceim_mass_tco2e * karma_factor
    ///   ker_adjust = ker_k * ker_e * (1.0 - ker_r)
    ///   karma_units_total = karma_base * ker_adjust
    ///
    /// Liquid / restricted split:
    ///   liquid fraction is lane- and quarantine-dependent; here:
    ///     - If kerdeployable_window_flag == 1 and lane in {RESEARCH, EXPPROD}:
    ///         liquid_fraction = 0.30
    ///     - If kerdeployable_window_flag == 1 and lane == "PROD":
    ///         liquid_fraction = 0.50
    ///     - Else (non-deployable or quarantined):
    ///         liquid_fraction = 0.0 (all restricted)
    ///
    /// Liability:
    ///   eco_liability_units is set to max(0, -eco_units_credited) and
    ///   eco_liability_reason is a short code string.
    pub fn recompute_karma_and_ecowealth(&mut self) {
        let karma_base = self.ceim_mass_tco2e * self.karma_factor;
        let ker_adjust = self.ker_k * self.ker_e * (1.0 - self.ker_r).max(0.0);
        let karma_units_total = karma_base * ker_adjust;

        let liquid_fraction = if self.kerdeployable_window_flag == 1 {
            match self.lane.as_str() {
                "PROD" => 0.50,
                "RESEARCH" | "EXPPROD" => 0.30,
                _ => 0.0,
            }
        } else {
            0.0
        };

        let karma_units_liquid = karma_units_total * liquid_fraction;
        let karma_units_restricted = karma_units_total - karma_units_liquid;

        self.karma_units_total = karma_units_total;
        self.karma_units_liquid = karma_units_liquid;
        self.karma_units_restricted = karma_units_restricted;

        // For now, we map karma_units_total 1:1 into eco_units_credited,
        // with the same liquid/restricted partitioning.
        self.eco_units_credited = karma_units_total;
        self.eco_units_liquid = karma_units_liquid;
        self.eco_units_restricted = karma_units_restricted;

        if self.eco_units_credited < 0.0 {
            self.eco_liability_units = -self.eco_units_credited;
            self.eco_liability_reason = "NEGATIVE_ECOWEALTH".to_string();
        } else {
            self.eco_liability_units = 0.0;
            if self.eco_liability_reason.is_empty() {
                self.eco_liability_reason = "NONE".to_string();
            }
        }
    }
}
