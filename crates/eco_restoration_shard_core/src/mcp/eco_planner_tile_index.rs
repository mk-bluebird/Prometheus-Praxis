// File: eco_restoration_shard_core/src/mcp/eco_planner_tile_index.rs
use std::{
    future::Future,
    pin::Pin,
    sync::{Arc, OnceLock},
};

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EcoStationHealthByTileRequest {
    pub tile_id: String,
    pub requested_energy_j: u64,
    pub diagnostic_only: bool,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EcoStationHealth {
    pub station_id: String,
    pub tile_id: String,
    pub health_band: String,
    pub updated_utc: String,
    pub carbon_negative_ready: bool,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EcoStationHealthByTileResponse {
    pub allowed: bool,
    pub decision_reason: String,
    pub station: Option<EcoStationHealth>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum EcoPlannerError {
    InvalidRequest(String),
    ServiceUnavailable(&'static str),
    RoutingFailure(String),
    RegistryFailure(String),
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum NanoSwarmEnergyDecision {
    Accept { reason: String },
    Review { reason: String },
}

pub trait OrganicChainEcoPlanner: Send + Sync {
    fn routenanoswarmenergy(
        &self,
        tile_id: String,
        requested_energy_j: u64,
        diagnostic_only: bool,
    ) -> Pin<Box<dyn Future<Output = Result<NanoSwarmEnergyDecision, EcoPlannerError>> + Send>>;
}

pub trait EcoStationRegistry: Send + Sync {
    fn health_by_tile(
        &self,
        tile_id: String,
    ) -> Pin<Box<dyn Future<Output = Result<Option<EcoStationHealth>, EcoPlannerError>> + Send>>;
}

#[derive(Clone)]
pub struct EcoPlannerTileServices {
    pub planner: Arc<dyn OrganicChainEcoPlanner>,
    pub registry: Arc<dyn EcoStationRegistry>,
}

static SERVICES: OnceLock<EcoPlannerTileServices> = OnceLock::new();

pub fn install_eco_planner_tile_services(
    services: EcoPlannerTileServices,
) -> Result<(), EcoPlannerError> {
    SERVICES
        .set(services)
        .map_err(|_| EcoPlannerError::InvalidRequest(
            "eco planner tile services are already installed".into(),
        ))
}

fn services() -> Result<&'static EcoPlannerTileServices, EcoPlannerError> {
    SERVICES
        .get()
        .ok_or(EcoPlannerError::ServiceUnavailable(
            "eco planner tile services",
        ))
}

pub async fn handle_eco_station_by_tile(
    request: EcoStationHealthByTileRequest,
) -> Result<EcoStationHealthByTileResponse, EcoPlannerError> {
    if request.tile_id.trim().is_empty() {
        return Err(EcoPlannerError::InvalidRequest(
            "tile_id must not be empty".into(),
        ));
    }
    if request.requested_energy_j == 0 {
        return Err(EcoPlannerError::InvalidRequest(
            "requested_energy_j must be positive".into(),
        ));
    }

    let services = services()?;
    let decision = services
        .planner
        .routenanoswarmenergy(
            request.tile_id.clone(),
            request.requested_energy_j,
            request.diagnostic_only,
        )
        .await?;

    let NanoSwarmEnergyDecision::Accept { reason } = decision else {
        let NanoSwarmEnergyDecision::Review { reason } = decision;
        return Ok(EcoStationHealthByTileResponse {
            allowed: false,
            decision_reason: reason,
            station: None,
        });
    };

    let station = services.registry.health_by_tile(request.tile_id).await?;
    let Some(station) = station else {
        return Ok(EcoStationHealthByTileResponse {
            allowed: false,
            decision_reason: "no eco-station is registered for the accepted tile".into(),
            station: None,
        });
    };

    if !station.carbon_negative_ready {
        return Ok(EcoStationHealthByTileResponse {
            allowed: false,
            decision_reason: "registered station is not carbon-negative ready".into(),
            station: None,
        });
    }

    Ok(EcoStationHealthByTileResponse {
        allowed: true,
        decision_reason: reason,
        station: Some(station),
    })
}
