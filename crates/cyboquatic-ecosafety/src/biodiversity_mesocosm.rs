// Filename: crates/cyboquatic-ecosafety/src/biodiversity_mesocosm.rs
// Biodiversity integrity and mesocosm-informed invasive-risk frames.

use serde::{Deserialize, Serialize};

use crate::{Frame, FrameContext, FrameError, RiskCoord, RiskVector};

/// Biodiversity integrity diagnostics.[file:22]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BiodiversityIntegrityDiagnostics {
    pub r_biodiv_refined: RiskCoord,
    pub corridor_ok: bool,
}

/// Frame that refines `r_biodiv` using PFAS, CEC, trap-fish, and trap-amphib risk.
///
/// This is a non-actuating frame that maps existing normalized risk
/// coordinates into a composite biodiversity integrity coordinate.[file:22]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BiodiversityIntegrityFrame {
    pub w_pfas: f64,
    pub w_cec: f64,
    pub w_trap_fish: f64,
    pub w_trap_amphib: f64,
}

impl BiodiversityIntegrityFrame {
    fn clamp01(x: f64) -> f64 {
        x.max(0.0).min(1.0)
    }

    fn refine(
        &self,
        r_pfas: RiskCoord,
        r_cec: RiskCoord,
        r_fish: RiskCoord,
        r_amphib: RiskCoord,
        r_biodiv: RiskCoord,
    ) -> (RiskCoord, bool) {
        let mut sum_w = self.w_pfas + self.w_cec + self.w_trap_fish + self.w_trap_amphib;
        if sum_w <= 0.0 {
            sum_w = 1.0;
        }
        let wp = self.w_pfas / sum_w;
        let wc = self.w_cec / sum_w;
        let wf = self.w_trap_fish / sum_w;
        let wa = self.w_trap_amphib / sum_w;

        // Higher PFAS/CEC and trap risks should worsen biodiversity coordinate.[file:22]
        let rb = r_biodiv.value();
        let composite = Self::clamp01(
            rb + wp * r_pfas.value() + wc * r_cec.value() + wf * r_fish.value() + wa * r_amphib.value(),
        );
        let corridor_ok = composite < 1.0 - 1e-9;
        (RiskCoord::new_clamped(composite), corridor_ok)
    }
}

impl Frame<(), BiodiversityIntegrityDiagnostics> for BiodiversityIntegrityFrame {
    fn run(
        &self,
        ctx: &FrameContext,
        _input: (),
    ) -> Result<(RiskVector, crate::LyapunovResidual, BiodiversityIntegrityDiagnostics), FrameError>
    {
        // In this sketch, PFAS/CEC and trap risks are assumed to be present
        // as dedicated RiskCoords in the RiskVector extension fields.[file:22]
        let rv = ctx.risk_in;
        let r_pfas = rv.r_pfas;
        let r_cec = rv.r_cec;
        let r_fish = rv.r_trap_fish;
        let r_amphib = rv.r_trap_amphib;
        let r_biodiv = rv.r_biodiv;

        let (r_refined, corridor_ok) =
            self.refine(r_pfas, r_cec, r_fish, r_amphib, r_biodiv);

        let mut rv_out = rv;
        rv_out.r_biodiv = r_refined;

        let diag = BiodiversityIntegrityDiagnostics {
            r_biodiv_refined: r_refined,
            corridor_ok,
        };

        Ok((rv_out, ctx.residual_in, diag))
    }
}

/// Minimal mesocosm ALN row used to adjust `r_invasive`.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct MesocosmShardRow {
    pub species_id: String,
    pub invasive_score: f64, // normalized 0..1
    pub corridor_ok: bool,
}

/// Frame that updates `r_invasive` based on mesocosm shards without widening corridors.[file:22]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct MesocosmRiskFrame {
    /// Baseline corridor upper bound for `r_invasive`.
    pub r_invasive_hard: f64,
}

impl MesocosmRiskFrame {
    fn clamp01(x: f64) -> f64 {
        x.max(0.0).min(1.0)
    }

    fn update_invasive(
        &self,
        current: RiskCoord,
        mesocosm: &MesocosmShardRow,
    ) -> (RiskCoord, bool) {
        let base = current.value();
        let upd = Self::clamp01(base.max(mesocosm.invasive_score));
        let within = upd <= self.r_invasive_hard;
        (RiskCoord::new_clamped(upd), within)
    }
}

impl Frame<MesocosmShardRow, RiskCoord> for MesocosmRiskFrame {
    fn run(
        &self,
        ctx: &FrameContext,
        input: MesocosmShardRow,
    ) -> Result<(RiskVector, crate::LyapunovResidual, RiskCoord), FrameError> {
        let rv = ctx.risk_in;
        let (r_inv, within) = self.update_invasive(rv.r_invasive, &input);
        if !within {
            return Err(FrameError::InvalidInput(
                "mesocosm update would violate invasive corridor".into(),
            ));
        }
        let mut rv_out = rv;
        rv_out.r_invasive = r_inv;
        Ok((rv_out, ctx.residual_in, r_inv))
    }
}
