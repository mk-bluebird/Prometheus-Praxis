// filename: crates/prometheus_praxis_ai/src/engine/mod.rs

#![forbid(unsafe_code)]

use crate::{
    AiNodeFrame, AiNodeRiskCoords, DrainageFrame, DrainageRiskCoords, ResidualSlice,
    WorkloadFrame, WorkloadRiskCoords,
};

/// Error type for numeric engine calls.
///
/// This keeps all failures explicit and non-panicking.
#[derive(Debug, Clone)]
pub enum EngineError {
    /// Input validation failed before calling the numeric kernel.
    InvalidInput(String),
    /// Underlying numeric kernel reported a non-zero status code.
    KernelFailure(String),
}

/// Result type for engine operations.
pub type EngineResult<T> = Result<T, EngineError>;

/// Façade over the three numeric bands (drainage, workload, AI node).
///
/// This module is purely Rust and non-actuating: it orchestrates calls to
/// C++ numeric kernels via FFI shims (in separate modules) and converts
/// their outputs into the Rust/ALN grammar types used by the ecosafety spine.
///
/// The actual FFI calls are defined in src/ffi/*.rs; this façade only
/// exposes safe, well-typed methods.
pub struct Engine;

impl Engine {
    /// Compute drainage residual slice and risk coordinates from raw
    /// hydraulics inputs, using the drainage kernel band.
    ///
    /// This function:
    /// - validates input ranges (e.g., non-negative BOD/TSS/CEC),
    /// - delegates to the C++ kernel via FFI (in ffi::drainage),
    /// - returns normalized risk coordinates and a residual slice that
    ///   match the DrainageDecayKernel2026v1.aln2 weights and invariants.
    pub fn compute_drainage_residual(
        bod_mg_l: f64,
        tss_mg_l: f64,
        cec_cmol_per_kg: f64,
        flow_rate_m3s: f64,
        water_temp_c: f64,
        elevation_m: f64,
    ) -> EngineResult<(DrainageRiskCoords, ResidualSlice)> {
        // Basic input validation; more detailed validation occurs in ALN and DB.
        if bod_mg_l < 0.0 || bod_mg_l > 80.0 {
            return Err(EngineError::InvalidInput(
                "bod_mg_l must be in [0, 80] mg/L".to_string(),
            ));
        }
        if tss_mg_l < 0.0 || tss_mg_l > 500.0 {
            return Err(EngineError::InvalidInput(
                "tss_mg_l must be in [0, 500] mg/L".to_string(),
            ));
        }
        if cec_cmol_per_kg < 0.0 || cec_cmol_per_kg > 50.0 {
            return Err(EngineError::InvalidInput(
                "cec_cmol_per_kg must be in [0, 50] cmol(+)/kg".to_string(),
            ));
        }
        if flow_rate_m3s < 0.0 {
            return Err(EngineError::InvalidInput(
                "flow_rate_m3s must be >= 0.0".to_string(),
            ));
        }
        if water_temp_c < 0.0 || water_temp_c > 45.0 {
            return Err(EngineError::InvalidInput(
                "water_temp_c must be in [0, 45] °C".to_string(),
            ));
        }
        if elevation_m < -100.0 || elevation_m > 2000.0 {
            return Err(EngineError::InvalidInput(
                "elevation_m must be in [-100, 2000] m".to_string(),
            ));
        }

        // For now, compute normalized risk coordinates and residual purely in Rust,
        // matching ALN weights. Once C++ kernels are ready, this will delegate to FFI.
        let r_bod = (bod_mg_l / 80.0).min(1.0);
        let r_tss = (tss_mg_l / 500.0).min(1.0);
        let r_cec = (cec_cmol_per_kg / 50.0).min(1.0);

        // Simple hydraulics and uncertainty proxies based on flow and temperature.
        let r_hydraulics = if flow_rate_m3s > 10.0 { 0.7 } else { 0.3 };
        let r_uncertainty = if water_temp_c > 35.0 { 0.6 } else { 0.3 };

        let risks = DrainageRiskCoords {
            r_bod,
            r_tss,
            r_cec,
            r_hydraulics,
            r_uncertainty,
        }
        .clamped();

        let vt = risks.residual();
        let residual = ResidualSlice::new(0.0, vt);

        Ok((risks, residual))
    }

    /// Compute workload residual slice and risk coordinates for a cyboquatic
    /// workload sample, using the workload energetics kernel band.
    ///
    /// Inputs:
    /// - energyreq_j: required energy [J] for the workload.
    /// - energysurplus_j: available surplus energy [J].
    /// - hydraulicrisk: proxy from drainage band.
    /// - uncertaintyrisk: proxy from telemetry/model quality.
    pub fn compute_workload_residual(
        energyreq_j: f64,
        energysurplus_j: f64,
        hydraulicrisk: f64,
        uncertaintyrisk: f64,
    ) -> EngineResult<(WorkloadRiskCoords, ResidualSlice)> {
        if energyreq_j < 0.0 || energyreq_j > 1.0e9 {
            return Err(EngineError::InvalidInput(
                "energyreq_j must be in [0, 1e9] J".to_string(),
            ));
        }
        if energysurplus_j < 0.0 {
            return Err(EngineError::InvalidInput(
                "energysurplus_j must be >= 0.0 J".to_string(),
            ));
        }
        if hydraulicrisk < 0.0 || hydraulicrisk > 1.0 {
            return Err(EngineError::InvalidInput(
                "hydraulicrisk must be in [0, 1]".to_string(),
            ));
        }
        if uncertaintyrisk < 0.0 || uncertaintyrisk > 1.0 {
            return Err(EngineError::InvalidInput(
                "uncertaintyrisk must be in [0, 1]".to_string(),
            ));
        }

        // Energy tailwind ratio Rt = surplus / required.
        let rt = if energyreq_j > 0.0 {
            (energysurplus_j / energyreq_j).min(2.5)
        } else {
            0.0
        };

        // Map Rt qualitatively to r_energy, consistent with ALN invariants:
        // strong tailwind => low risk, shortfall => high risk.
        let r_energy = if energyreq_j <= 0.0 {
            1.0
        } else if rt >= 1.2 {
            0.2
        } else if rt <= 0.0 {
            0.9
        } else {
            0.5
        };

        let risks = WorkloadRiskCoords {
            r_energy,
            r_hydraulics: hydraulicrisk,
            r_uncertainty: uncertaintyrisk,
        }
        .clamped();

        let vt = risks.residual();
        let residual = ResidualSlice::new(0.0, vt);

        Ok((risks, residual))
    }

    /// Compute AI datacenter node residual slice and risk coordinates
    /// for a single AI node telemetry sample, using the AI node energetics band.
    ///
    /// Inputs:
    /// - pue, cue: efficiency metrics.
    /// - power_kw, cooling_kw, thermal_output_kw: instantaneous draws and waste heat.
    /// - carbon_intensity: normalized [0,1] carbon score for the current energy mix.
    /// - biodiversity_risk: normalized [0,1] local siting impact risk.
    /// - uncertainty_risk: normalized [0,1] telemetry/model quality risk.
    pub fn compute_ai_node_residual(
        pue: f64,
        cue: f64,
        power_kw: f64,
        cooling_kw: f64,
        thermal_output_kw: f64,
        carbon_intensity: f64,
        biodiversity_risk: f64,
        uncertainty_risk: f64,
    ) -> EngineResult<(AiNodeRiskCoords, ResidualSlice)> {
        if pue < 1.0 || pue > 3.5 {
            return Err(EngineError::InvalidInput(
                "pue must be in [1.0, 3.5]".to_string(),
            ));
        }
        if cue < 0.5 || cue > 5.0 {
            return Err(EngineError::InvalidInput(
                "cue must be in [0.5, 5.0]".to_string(),
            ));
        }
        if power_kw < 0.0 || power_kw > 100_000.0 {
            return Err(EngineError::InvalidInput(
                "power_kw must be in [0.0, 100000.0]".to_string(),
            ));
        }
        if cooling_kw < 0.0 || cooling_kw > 100_000.0 {
            return Err(EngineError::InvalidInput(
                "cooling_kw must be in [0.0, 100000.0]".to_string(),
            ));
        }
        if thermal_output_kw < 0.0 {
            return Err(EngineError::InvalidInput(
                "thermal_output_kw must be >= 0.0".to_string(),
            ));
        }
        if carbon_intensity < 0.0 || carbon_intensity > 1.0 {
            return Err(EngineError::InvalidInput(
                "carbon_intensity must be in [0, 1]".to_string(),
            ));
        }
        if biodiversity_risk < 0.0 || biodiversity_risk > 1.0 {
            return Err(EngineError::InvalidInput(
                "biodiversity_risk must be in [0, 1]".to_string(),
            ));
        }
        if uncertainty_risk < 0.0 || uncertainty_risk > 1.0 {
            return Err(EngineError::InvalidInput(
                "uncertainty_risk must be in [0, 1]".to_string(),
            ));
        }

        // Energy compute risk: higher when PUE is poor and power draw is large.
        let r_energy_compute = {
            let pue_norm = (pue - 1.0) / (3.5 - 1.0);
            let power_norm = (power_kw / 100_000.0).min(1.0);
            ((pue_norm + power_norm) / 2.0).min(1.0)
        };

        // Cooling/water risk: based on CUE and cooling draw.
        let r_cooling_water = {
            let cue_norm = (cue - 0.5) / (5.0 - 0.5);
            let cooling_norm = (cooling_kw / 100_000.0).min(1.0);
            ((cue_norm + cooling_norm) / 2.0).min(1.0)
        };

        let risks = AiNodeRiskCoords {
            r_energy_compute,
            r_cooling_water,
            r_carbon: carbon_intensity,
            r_biodiversity: biodiversity_risk,
            r_uncertainty: uncertainty_risk,
        }
        .clamped();

        let vt_ai = risks.residual();
        let residual = ResidualSlice::new(0.0, vt_ai);

        Ok((risks, residual))
    }

    /// Construct a `DrainageFrame` from raw telemetry and computed risks/residual.
    ///
    /// This is a convenience builder for callers that already have
    /// frame-level identifiers / anchors and want a fully-populated frame.
    pub fn build_drainage_frame(
        frame_id: String,
        yyyymmdd: String,
        canal_segment_id: String,
        node_id: String,
        bod_mg_l: f64,
        tss_mg_l: f64,
        cec_cmol_per_kg: f64,
        flow_rate_m3s: f64,
        water_temp_c: f64,
        elevation_m: f64,
        phoenix_hex_anchor: String,
        prior_frame_id: String,
    ) -> EngineResult<DrainageFrame> {
        let (risks, residual) =
            Self::compute_drainage_residual(bod_mg_l, tss_mg_l, cec_cmol_per_kg, flow_rate_m3s, water_temp_c, elevation_m)?;

        let ker = crate::KerTriad {
            k: 0.9,
            e: 0.9,
            r: residual.vt_after.min(1.0),
        }
        .clamped();

        Ok(DrainageFrame {
            frame_id,
            yyyymmdd,
            canal_segment_id,
            node_id,
            bod_mg_l,
            tss_mg_l,
            cec_cmol_per_kg,
            flow_rate_m3s,
            water_temp_c,
            elevation_m,
            risks,
            residual,
            ker,
            phoenix_hex_anchor,
            prior_frame_id,
        })
    }

    /// Construct a `WorkloadFrame` from raw telemetry and computed risks/residual.
    pub fn build_workload_frame(
        frame_id: String,
        yyyymmdd: String,
        workload_id: String,
        node_id: String,
        task_type: String,
        timestamputc: String,
        energyreq_j: f64,
        energysurplus_j: f64,
        hydraulicrisk: f64,
        uncertaintyrisk: f64,
        phoenix_hex_anchor: String,
        prior_frame_id: String,
        lane: crate::Lane,
    ) -> EngineResult<WorkloadFrame> {
        let (risks, residual) =
            Self::compute_workload_residual(energyreq_j, energysurplus_j, hydraulicrisk, uncertaintyrisk)?;

        let ker = crate::compute_ker_from_workload(risks, residual);

        Ok(WorkloadFrame {
            frame_id,
            yyyymmdd,
            workload_id,
            node_id,
            task_type,
            timestamputc,
            energyreq_j,
            energysurplus_j,
            hydraulicrisk,
            uncertaintyrisk,
            risks,
            residual,
            ker,
            lane,
            phoenix_hex_anchor,
            prior_frame_id,
        })
    }

    /// Construct an `AiNodeFrame` from raw telemetry and computed risks/residual.
    pub fn build_ai_node_frame(
        frame_id: String,
        yyyymmdd: String,
        facility_id: String,
        rack_id: String,
        tile_id: String,
        timestamputc: String,
        pue: f64,
        cue: f64,
        power_kw: f64,
        cooling_kw: f64,
        thermal_output_kw: f64,
        throughput_qps: f64,
        joules_per_inference: f64,
        eco_quota_kwh: f64,
        eco_quota_window_start_utc: String,
        eco_quota_window_end_utc: String,
        heat_governance_event_id: String,
        ai_load_schedule_event_id: String,
        carbon_intensity: f64,
        biodiversity_risk: f64,
        uncertainty_risk: f64,
        phoenix_hex_anchor: String,
        prior_frame_id: String,
        lane: crate::Lane,
    ) -> EngineResult<AiNodeFrame> {
        let (risks, residual_ai) = Self::compute_ai_node_residual(
            pue,
            cue,
            power_kw,
            cooling_kw,
            thermal_output_kw,
            carbon_intensity,
            biodiversity_risk,
            uncertainty_risk,
        )?;

        let ker = crate::compute_ker_from_ai_node(risks, residual_ai);

        Ok(AiNodeFrame {
            frame_id,
            yyyymmdd,
            facility_id,
            rack_id,
            tile_id,
            timestamputc,
            pue,
            cue,
            power_kw,
            cooling_kw,
            thermal_output_kw,
            throughput_qps,
            joules_per_inference,
            eco_quota_kwh,
            eco_quota_window_start_utc,
            eco_quota_window_end_utc,
            heat_governance_event_id,
            ai_load_schedule_event_id,
            risks,
            residual_ai,
            ker,
            lane,
            phoenix_hex_anchor,
            prior_frame_id,
        })
    }
}
