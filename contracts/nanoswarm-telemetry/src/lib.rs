// filename: contracts/nanoswarm-telemetry/src/lib.rs
// destination: mk-bluebird/Prometheus-Praxis/contracts/nanoswarm-telemetry/src/lib.rs
// Rust edition: 2024, rust-version = "1.85"
// License: MIT OR Apache-2.0

use cosmwasm_std::{
    entry_point, to_binary, Addr, Binary, Deps, DepsMut, Env, MessageInfo, Order,
    Response, StdError, StdResult,
};
use cw2::set_contract_version;
use cw_storage_plus::{Item, Map};
use serde::{Deserialize, Serialize};

const CONTRACT_NAME: &str = "nanoswarm-telemetry";
const CONTRACT_VERSION: &str = "0.1.0";

/// Configuration for nanoswarm telemetry ingestion.
#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct Config {
    /// Admin address.
    pub admin: Addr,
    /// Bee corridor contract address to notify (optional).
    pub bee_corridor_contract: Option<Addr>,
    /// RF corridor ceiling (mW average) per cell.
    pub rf_ceiling_mw: f64,
}

/// EMF + flower density summary reported by a nanoswarm cluster.
#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct TelemetrySummary {
    pub cell_id: String,
    /// Average EMF (mW/m^2) over the reporting window.
    pub emf_avg_mw_per_m2: f64,
    /// Maximum EMF (mW/m^2) over the window.
    pub emf_max_mw_per_m2: f64,
    /// Flower density (normalized 0..1).
    pub flower_density: f64,
    /// Number of robots contributing to this summary.
    pub robot_count: u32,
}

/// Storage record for telemetry.
#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct TelemetryRecord {
    pub cell_id: String,
    pub block_height: u64,
    pub emf_avg_mw_per_m2: f64,
    pub emf_max_mw_per_m2: f64,
    pub flower_density: f64,
    pub robot_count: u32,
    /// Derived bee risk coordinate (0..1) for RF stress.
    pub r_bee_rf: f64,
}

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct InstantiateMsg {
    pub admin: String,
    pub bee_corridor_contract: Option<String>,
    pub rf_ceiling_mw: f64,
}

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
#[serde(tag = "type", content = "value")]
pub enum ExecuteMsg {
    /// Submit nanoswarm telemetry summary.
    SubmitTelemetry { summary: TelemetrySummary },
}

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
#[serde(tag = "type", content = "value")]
pub enum QueryMsg {
    /// Get latest telemetry record for a cell.
    GetLatest { cell_id: String },
    /// List telemetry records for a cell.
    List { cell_id: String },
    /// Get config.
    GetConfig {},
}

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct TelemetryResponse {
    pub record: Option<TelemetryRecord>,
}

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct TelemetryListResponse {
    pub records: Vec<TelemetryRecord>,
}

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct ConfigResponse {
    pub config: Config,
}

// Storage
const CONFIG: Item<Config> = Item::new("config");
const TELEMETRY: Map<(String, u64), TelemetryRecord> = Map::new("telemetry");

#[entry_point]
pub fn instantiate(
    deps: DepsMut,
    _env: Env,
    info: MessageInfo,
    msg: InstantiateMsg,
) -> StdResult<Response> {
    let admin = deps.api.addr_validate(&msg.admin)?;
    let bee_corridor_contract = match msg.bee_corridor_contract {
        Some(addr) => Some(deps.api.addr_validate(&addr)?),
        None => None,
    };

    let cfg = Config {
        admin,
        bee_corridor_contract,
        rf_ceiling_mw: msg.rf_ceiling_mw,
    };
    set_contract_version(deps.storage, CONTRACT_NAME, CONTRACT_VERSION)?;
    CONFIG.save(deps.storage, &cfg)?;

    Ok(Response::new()
        .add_attribute("action", "instantiate")
        .add_attribute("sender", info.sender))
}

#[entry_point]
pub fn execute(
    deps: DepsMut,
    env: Env,
    info: MessageInfo,
    msg: ExecuteMsg,
) -> StdResult<Response> {
    match msg {
        ExecuteMsg::SubmitTelemetry { summary } => {
            execute_submit_telemetry(deps, env, info, summary)
        }
    }
}

fn execute_submit_telemetry(
    deps: DepsMut,
    env: Env,
    _info: MessageInfo,
    summary: TelemetrySummary,
) -> StdResult<Response> {
    let cfg = CONFIG.load(deps.storage)?;

    if summary.emf_avg_mw_per_m2 < 0.0
        || summary.emf_max_mw_per_m2 < 0.0
        || summary.flower_density < 0.0
        || summary.flower_density > 1.0
    {
        return Err(StdError::generic_err(
            "invalid telemetry values (negative or out-of-range)",
        ));
    }

    // Compute normalized RF risk coordinate (0..1) relative to rf_ceiling.
    let r_bee_rf = (summary.emf_max_mw_per_m2 / cfg.rf_ceiling_mw)
        .max(0.0)
        .min(1.0);

    let record = TelemetryRecord {
        cell_id: summary.cell_id.clone(),
        block_height: env.block.height,
        emf_avg_mw_per_m2: summary.emf_avg_mw_per_m2,
        emf_max_mw_per_m2: summary.emf_max_mw_per_m2,
        flower_density: summary.flower_density,
        robot_count: summary.robot_count,
        r_bee_rf,
    };

    TELEMETRY.save(
        deps.storage,
        (summary.cell_id.clone(), env.block.height),
        &record,
    )?;

    let mut resp = Response::new()
        .add_attribute("action", "submit_telemetry")
        .add_attribute("cell_id", summary.cell_id)
        .add_attribute("block_height", env.block.height.to_string())
        .add_attribute("r_bee_rf", format!("{:.6}", r_bee_rf));

    // Optionally, future work: emit a message to bee corridor contract to
    // tighten or relax nanoswarm duty schedules based on r_bee_rf.
    if let Some(_bee_corridor) = cfg.bee_corridor_contract {
        // For now, we keep this non-actuating and purely evidentiary; any
        // follow-up action must be decided by external orchestrators.
    }

    Ok(resp)
}

#[entry_point]
pub fn query(deps: Deps, _env: Env, msg: QueryMsg) -> StdResult<Binary> {
    match msg {
        QueryMsg::GetLatest { cell_id } => {
            let record = latest_record_for_cell(deps, &cell_id)?;
            to_binary(&TelemetryResponse { record })
        }
        QueryMsg::List { cell_id } => {
            let records = list_records_for_cell(deps, &cell_id)?;
            to_binary(&TelemetryListResponse { records })
        }
        QueryMsg::GetConfig {} => {
            let cfg = CONFIG.load(deps.storage)?;
            to_binary(&ConfigResponse { config: cfg })
        }
    }
}

fn latest_record_for_cell(deps: Deps, cell_id: &str) -> StdResult<Option<TelemetryRecord>> {
    let mut latest: Option<TelemetryRecord> = None;
    let prefix = TELEMETRY.prefix(cell_id.to_string());
    let iter = prefix.range(deps.storage, None, None, Order::Descending);
    for item in iter {
        let (_k, v) = item?;
        latest = Some(v);
        break;
    }
    Ok(latest)
}

fn list_records_for_cell(deps: Deps, cell_id: &str) -> StdResult<Vec<TelemetryRecord>> {
    let mut out = Vec::new();
    let prefix = TELEMETRY.prefix(cell_id.to_string());
    let iter = prefix.range(deps.storage, None, None, Order::Ascending);
    for item in iter {
        let (_k, v) = item?;
        out.push(v);
    }
    Ok(out)
}
