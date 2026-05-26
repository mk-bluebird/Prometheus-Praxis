// ecosafety-core/src/non_actuating_workload.rs

/// Marker: this workload is non‑actuating. It may not depend on crates that
/// expose actuator APIs or fieldbus / PLC bindings. CI must enforce this.
pub trait NonActuatingWorkload {
    /// Input is typically an ALN‑backed shard or telemetry snapshot
    /// that already carries a full RiskVector and KER window.
    type Input;
    /// Output must include a full RiskVector and KER window.
    type Output;

    /// Pure, non‑actuating kernel:
    ///   (X, r, V_t, K, E, R, lane, ker_deployable) -> same space,
    /// with r_j ∈ [0,1] and V_t = Σ_j w_j r_j².
    fn execute(&self, input: Self::Input) -> Self::Output;
}

/// Specialization for hydrology kernels operating on hydrological buffer shards.
pub trait NonActuatingHydraulicKernel: NonActuatingWorkload {}

/// Specialization for material kinetics (biodegradable substrates, FlowVac, etc.).
pub trait NonActuatingMaterialKernel: NonActuatingWorkload {}

/// Specialization for data‑quality / sensor trust pipelines.
pub trait NonActuatingDataQualityKernel: NonActuatingWorkload {}

/// Specialization for governance / topology auditor workloads.
pub trait NonActuatingGovernanceKernel: NonActuatingWorkload {}
