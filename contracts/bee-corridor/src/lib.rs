// filename: contracts/bee-corridor/src/lib.rs
// destination: mk-bluebird/Prometheus-Praxis/contracts/bee-corridor/src/lib.rs
// Rust edition: 2024, rust-version = "1.85"
// License: MIT OR Apache-2.0

use cosmwasm_std::{
    entry_point, to_binary, Addr, Binary, Deps, DepsMut, Env, MessageInfo, Order,
    Response, StdError, StdResult,
};
use cw2::set_contract_version;
use cw_storage_plus::{Item, Map};
use serde::{Deserialize, Serialize};

use eco_net_bee::{
    BeeCorridorProvider, BeeCorridorProviderMock, BeeEnvelope, BeeLedgerRecord,
    PlacementJustification, SunflowerPlacementRequest,
};

const CONTRACT_NAME: &str = "bee-corridor";
const CONTRACT_VERSION: &str = "0.1.0";

/// Global configuration for the bee corridor contract.
#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct Config {
    /// Address with authority to update envelopes and emergency stop.
    pub admin: Addr,
    /// Maximum RF budget per cell (mW average), corridor-aligned.
    pub rf_budget_mw: f64,
    /// Optional link to nanoswarm telemetry contract.
    pub nanoswarm_contract: Option<Addr>,
}

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct InstantiateMsg {
    pub admin: String,
    pub rf_budget_mw: f64,
    pub nanoswarm_contract: Option<String>,
}

/// Execute messages supported by the contract.
#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
#[serde(tag = "type", content = "value")]
pub enum ExecuteMsg {
    /// Register or update a BeeEnvelope for a cell.
    UpsertEnvelope {
        envelope_id: String,
        cell_id: String,
        min_habitat_area_m2: f64,
        max_sunflower_density: f64,
        metadata: Option<Vec<(String, String)>>,
    },
    /// Propose a Sunflower placement in a corridor cell.
    ProposeSunflowerPlacement {
        asset_id: String,
        cell_id: String,
        footprint_area_m2: f64,
        delta_habitat_loss_m2: f64,
        metadata: Option<Vec<(String, String)>>,
    },
    /// Emergency stop for nanoswarm RF in a given cell.
    EmergencyStopNanoswarm {
        cell_id: String,
        reason: String,
    },
}

/// Query messages supported by the contract.
#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
#[serde(tag = "type", content = "value")]
pub enum QueryMsg {
    /// Get the current BeeEnvelope for a cell.
    GetEnvelope { cell_id: String },
    /// List all envelopes.
    ListEnvelopes {},
    /// Get ledger entries by logical key (cell_id or asset_id).
    GetLedger { key: String },
    /// Get current config.
    GetConfig {},
}

/// Response types.
#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct EnvelopeResponse {
    pub envelope: Option<BeeEnvelope>,
}

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct EnvelopesResponse {
    pub envelopes: Vec<BeeEnvelope>,
}

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct LedgerResponse {
    pub records: Vec<BeeLedgerRecord>,
}

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct ConfigResponse {
    pub config: Config,
}

// Storage
const CONFIG: Item<Config> = Item::new("config");
const ENVELOPES: Map<String, BeeEnvelope> = Map::new("envelopes");
const LEDGER: Map<String, Vec<BeeLedgerRecord>> = Map::new("ledger");

#[entry_point]
pub fn instantiate(
    deps: DepsMut,
    _env: Env,
    info: MessageInfo,
    msg: InstantiateMsg,
) -> StdResult<Response> {
    let admin = deps.api.addr_validate(&msg.admin)?;
    let nanoswarm_contract = match msg.nanoswarm_contract {
        Some(addr) => Some(deps.api.addr_validate(&addr)?),
        None => None,
    };

    let cfg = Config {
        admin,
        rf_budget_mw: msg.rf_budget_mw,
        nanoswarm_contract,
    };
    set_contract_version(deps.storage, CONTRACT_NAME, CONTRACT_VERSION)?;
    CONFIG.save(deps.storage, &cfg)?;

    Ok(Response::new()
        .add_attribute("action", "instantiate")
        .add_attribute("sender", info.sender)
        .add_attribute("admin", cfg.admin))
}

#[entry_point]
pub fn execute(
    deps: DepsMut,
    env: Env,
    info: MessageInfo,
    msg: ExecuteMsg,
) -> StdResult<Response> {
    match msg {
        ExecuteMsg::UpsertEnvelope {
            envelope_id,
            cell_id,
            min_habitat_area_m2,
            max_sunflower_density,
            metadata,
        } => execute_upsert_envelope(
            deps,
            info,
            envelope_id,
            cell_id,
            min_habitat_area_m2,
            max_sunflower_density,
            metadata,
        ),
        ExecuteMsg::ProposeSunflowerPlacement {
            asset_id,
            cell_id,
            footprint_area_m2,
            delta_habitat_loss_m2,
            metadata,
        } => execute_propose_sunflower_placement(
            deps,
            env,
            info,
            asset_id,
            cell_id,
            footprint_area_m2,
            delta_habitat_loss_m2,
            metadata,
        ),
        ExecuteMsg::EmergencyStopNanoswarm { cell_id, reason } => {
            execute_emergency_stop_nanoswarm(deps, info, cell_id, reason)
        }
    }
}

fn ensure_admin(deps: &DepsMut, info: &MessageInfo) -> StdResult<()> {
    let cfg = CONFIG.load(deps.storage)?;
    if info.sender != cfg.admin {
        return Err(StdError::generic_err("unauthorized"));
    }
    Ok(())
}

fn execute_upsert_envelope(
    deps: DepsMut,
    info: MessageInfo,
    envelope_id: String,
    cell_id: String,
    min_habitat_area_m2: f64,
    max_sunflower_density: f64,
    metadata: Option<Vec<(String, String)>>,
) -> StdResult<Response> {
    ensure_admin(&deps, &info)?;

    let meta_map = metadata
        .unwrap_or_default()
        .into_iter()
        .collect::<std::collections::HashMap<_, _>>();

    let envelope = BeeEnvelope {
        envelope_id,
        cell_id: cell_id.clone(),
        min_habitat_area_m2,
        max_sunflower_density,
        metadata: meta_map,
    };

    ENVELOPES.save(deps.storage, cell_id.clone(), &envelope)?;

    Ok(Response::new()
        .add_attribute("action", "upsert_envelope")
        .add_attribute("cell_id", cell_id))
}

fn execute_propose_sunflower_placement(
    deps: DepsMut,
    env: Env,
    info: MessageInfo,
    asset_id: String,
    cell_id: String,
    footprint_area_m2: f64,
    delta_habitat_loss_m2: f64,
    metadata: Option<Vec<(String, String)>>,
) -> StdResult<Response> {
    // Build an in-contract BeeCorridorProvider over current storage.
    let provider = BeeCorridorProviderMock::new(
        load_envelopes_map(&deps)?,
        load_ledger_map(&deps)?,
    );

    let meta_map = metadata
        .unwrap_or_default()
        .into_iter()
        .collect::<std::collections::HashMap<_, _>>();

    let req = SunflowerPlacementRequest {
        asset_id: asset_id.clone(),
        cell_id: cell_id.clone(),
        footprint_area_m2,
        delta_habitat_loss_m2,
        metadata: meta_map,
    };

    let justification = provider.validate_sunflower_placement(&req);

    if !justification.allowed {
        // Store a rejected ledger record with justification summary.
        let mut ledger_records = LEDGER
            .may_load(deps.storage, cell_id.clone())?
            .unwrap_or_default();

        let record = BeeLedgerRecord {
            asset_id: asset_id.clone(),
            region: "unknown-region".to_string(),
            cell_id: cell_id.clone(),
            lane: "REJECTED".to_string(),
            ker_band: "BLOCKED".to_string(),
            eco_bee_impact_score: 0.0,
            evidence_hex: String::new(),
            signing_did: info.sender.to_string(),
        };
        ledger_records.push(record);
        LEDGER.save(deps.storage, cell_id.clone(), &ledger_records)?;

        return Ok(Response::new()
            .add_attribute("action", "reject_sunflower_placement")
            .add_attribute("cell_id", cell_id)
            .add_attribute("asset_id", asset_id)
            .add_attribute("reason", justification.summary));
    }

    // Placement allowed – store ledger entry.
    let mut ledger_records = LEDGER
        .may_load(deps.storage, cell_id.clone())?
        .unwrap_or_default();

    for rec in justification.prospective_ledger_entries.iter() {
        ledger_records.push(rec.clone());
    }

    LEDGER.save(deps.storage, cell_id.clone(), &ledger_records)?;

    Ok(Response::new()
        .add_attribute("action", "accept_sunflower_placement")
        .add_attribute("cell_id", cell_id)
        .add_attribute("asset_id", asset_id)
        .add_attribute("block_height", env.block.height.to_string()))
}

fn execute_emergency_stop_nanoswarm(
    deps: DepsMut,
    info: MessageInfo,
    cell_id: String,
    reason: String,
) -> StdResult<Response> {
    ensure_admin(&deps, &info)?;

    // Record an emergency stop event in the ledger.
    let mut ledger_records = LEDGER
        .may_load(deps.storage, cell_id.clone())?
        .unwrap_or_default();

    let record = BeeLedgerRecord {
        asset_id: format!("nanoswarm-stop-{}", cell_id),
        region: "unknown-region".to_string(),
        cell_id: cell_id.clone(),
        lane: "EMERGENCY".to_string(),
        ker_band: "BLOCKED".to_string(),
        eco_bee_impact_score: 0.0,
        evidence_hex: String::new(),
        signing_did: info.sender.to_string(),
    };

    ledger_records.push(record);
    LEDGER.save(deps.storage, cell_id.clone(), &ledger_records)?;

    Ok(Response::new()
        .add_attribute("action", "emergency_stop_nanoswarm")
        .add_attribute("cell_id", cell_id)
        .add_attribute("reason", reason))
}

fn load_envelopes_map(
    deps: &DepsMut,
) -> StdResult<std::collections::HashMap<String, BeeEnvelope>> {
    let mut out = std::collections::HashMap::new();
    let iter = ENVELOPES.range(deps.storage, None, None, Order::Ascending);
    for item in iter {
        let (k, v) = item?;
        out.insert(k, v);
    }
    Ok(out)
}

fn load_ledger_map(
    deps: &DepsMut,
) -> StdResult<std::collections::HashMap<String, Vec<BeeLedgerRecord>>> {
    let mut out = std::collections::HashMap::new();
    let iter = LEDGER.range(deps.storage, None, None, Order::Ascending);
    for item in iter {
        let (k, v) = item?;
        out.insert(k, v);
    }
    Ok(out)
}

#[entry_point]
pub fn query(deps: Deps, _env: Env, msg: QueryMsg) -> StdResult<Binary> {
    match msg {
        QueryMsg::GetEnvelope { cell_id } => {
            let env = ENVELOPES.may_load(deps.storage, cell_id)?;
            to_binary(&EnvelopeResponse { envelope: env })
        }
        QueryMsg::ListEnvelopes {} => {
            let mut envelopes = Vec::new();
            let iter = ENVELOPES.range(deps.storage, None, None, Order::Ascending);
            for item in iter {
                let (_k, v) = item?;
                envelopes.push(v);
            }
            to_binary(&EnvelopesResponse { envelopes })
        }
        QueryMsg::GetLedger { key } => {
            let recs = LEDGER.may_load(deps.storage, key)?.unwrap_or_default();
            to_binary(&LedgerResponse { records: recs })
        }
        QueryMsg::GetConfig {} => {
            let cfg = CONFIG.load(deps.storage)?;
            to_binary(&ConfigResponse { config: cfg })
        }
    }
}
