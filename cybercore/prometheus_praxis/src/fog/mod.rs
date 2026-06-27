// filepath: cybercore/prometheus_praxis/src/fog/mod.rs

#![forbid(unsafe_code)]

pub mod node_fog_routing;
pub mod block_stress_guard;

pub use node_fog_routing::{
    BioSurfaceMode,
    CyboVariant,
    MediaClass,
    NodeFogStressSummary,
    NodeShard,
    RouteDecision,
    RoutingContext,
    route_variant,
};

pub use block_stress_guard::{
    FogBlockStressDecision,
    FogBlockStressGuard,
};
