
use fog_router_guard::{FogNodeSnapshot, FogRouteDecision, decide_route};
use cyboquatic_ecosafety::{FogGuardConfig, RiskCoord, RiskVector, KERWindow, LyapunovResidual, CyboLane};

pub struct FogRouter {
    guard_cfg: FogGuardConfig,
}

impl FogRouter {
    pub fn new(guard_cfg: FogGuardConfig) -> Self {
        Self { guard_cfg }
    }

    /// Pure decision step: given diagnostic inputs, decide whether to route.
    /// The caller is responsible for mapping AllowRoute / BlockRoute into
    /// ROS2 messages or fieldbus commands in a separate, actuating layer.
    pub fn decide_for_window(
        &self,
        lane: CyboLane,
        risk: RiskVector,
        ker_window: KERWindow,
        prev_residual: LyapunovResidual,
        corridor_present: bool,
        evidencehex: String,
        did: String,
    ) -> FogRouteDecision {
        let snapshot = FogNodeSnapshot {
            lane,
            risk,
            ker_window,
            prev_residual,
            evidencehex,
            did,
            corridor_present,
        };

        decide_route(&snapshot, Some(self.guard_cfg.clone()))
    }
}
