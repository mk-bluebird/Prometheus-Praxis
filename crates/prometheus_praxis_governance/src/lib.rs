// filename: crates/prometheus_praxis_governance/src/lib.rs

#![forbid(unsafe_code)]

pub mod governance_flag;
pub mod steward_veto;

pub use steward_veto::{
    workload_is_admissible, StewardAuthority, StewardVeto, StewardVetoRegistry, VetoError,
    VetoPolicy, WorkloadFrameRef,
};
