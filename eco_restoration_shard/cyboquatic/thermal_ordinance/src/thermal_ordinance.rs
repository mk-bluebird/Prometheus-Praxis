// Filename: src/thermal_ordinance.rs
// Destination: eco_restoration_shard/cyboquatic/thermal_ordinance/src/thermal_ordinance.rs

#![forbid(unsafe_code)]

/// Corridor parameters for a single micro-zone and season.
/// All temperatures are in degrees Celsius, already adapted for local climate
/// and occupancy group (vulnerable vs general).
#[derive(Clone, Copy, Debug)]
pub struct ThermalBand {
    /// Upper bound of the safe band for heat (no risk).
    pub t_safe_heat: f64,
    /// Hard limit for heat (lethal edge).
    pub t_hard_heat: f64,
    /// Lower bound of the safe band for cold (no risk).
    pub t_safe_cold: f64,
    /// Hard limit for cold (hypothermia edge).
    pub t_hard_cold: f64,
}

/// Normalized thermal risks for one micro-zone.
/// These are the corridor-coordinates used across the ecosafety grammar.
#[derive(Clone, Copy, Debug, Default)]
pub struct ThermalRisk {
    /// Normalized heat risk r_{T,heat} in [0, 1].
    pub r_heat: f64,
    /// Normalized cold risk r_{T,cold} in [0, 1].
    pub r_cold: f64,
}

impl ThermalBand {
    /// Compute normalized heat risk r_{T,heat} from an indoor temperature.
    /// 0.0 => at or below t_safe_heat, 1.0 => at or above t_hard_heat, linear in between.
    pub fn r_heat(&self, t_indoor: f64) -> f64 {
        if t_indoor <= self.t_safe_heat {
            0.0
        } else if t_indoor >= self.t_hard_heat {
            1.0
        } else {
            let num = t_indoor - self.t_safe_heat;
            let den = (self.t_hard_heat - self.t_safe_heat).max(f64::MIN_POSITIVE);
            (num / den).clamp(0.0, 1.0)
        }
    }

    /// Compute normalized cold risk r_{T,cold} from an indoor temperature.
    /// 0.0 => at or above t_safe_cold, 1.0 => at or below t_hard_cold, linear in between.
    pub fn r_cold(&self, t_indoor: f64) -> f64 {
        if t_indoor >= self.t_safe_cold {
            0.0
        } else if t_indoor <= self.t_hard_cold {
            1.0
        } else {
            let num = self.t_safe_cold - t_indoor;
            let den = (self.t_safe_cold - self.t_hard_cold).max(f64::MIN_POSITIVE);
            (num / den).clamp(0.0, 1.0)
        }
    }

    /// Compute both risks at once.
    pub fn risk(&self, t_indoor: f64) -> ThermalRisk {
        ThermalRisk {
            r_heat: self.r_heat(t_indoor),
            r_cold: self.r_cold(t_indoor),
        }
    }
}

/// Minimal state projection needed for the ordinance check.
/// In a full qpudatashard this would be one row per ward or building cluster.
#[derive(Clone, Debug)]
pub struct ThermalState {
    /// Indoor temperatures per micro-zone (Celsius).
    pub t_indoor: Vec<f64>,
    /// Corresponding corridor bands per micro-zone.
    pub bands: Vec<ThermalBand>,
}

impl ThermalState {
    /// Compute normalized risks for all zones.
    pub fn risks(&self) -> Vec<ThermalRisk> {
        self.t_indoor
            .iter()
            .zip(self.bands.iter())
            .map(|(t, band)| band.risk(*t))
            .collect()
    }
}

/// Trait that encodes a local heat-action ordinance as ALN-style corridors
/// over r_{T,heat} and r_{T,cold}.
pub trait HeatActionOrdinance {
    /// Returns true iff the given state satisfies the ordinance thermal corridors
    /// in every monitored micro-zone.
    fn is_state_thermal_safe(&self, state: &ThermalState) -> bool;

    /// Returns true iff the transition from `from` to `to` is ordinance-compliant.
    /// This is the exact predicate used to define the ordinance transition relation:
    /// x ->_{ord} x' iff `is_compliant_transition(x, x')` is true.
    fn is_compliant_transition(&self, from: &ThermalState, to: &ThermalState) -> bool;
}

/// A concrete corridor-based ordinance: "no zone may reach r_{T,heat} > 1
/// or r_{T,cold} > 1 at any time-step". This matches the ecosafety grammar
/// you are already using for cyboquatic nodes.
#[derive(Clone, Debug)]
pub struct CorridorOrdinance;

impl HeatActionOrdinance for CorridorOrdinance {
    fn is_state_thermal_safe(&self, state: &ThermalState) -> bool {
        // State is safe iff all normalized risks are <= 1.0 for both axes.
        state
            .risks()
            .into_iter()
            .all(|r| r.r_heat <= 1.0 && r.r_cold <= 1.0)
    }

    fn is_compliant_transition(&self, _from: &ThermalState, to: &ThermalState) -> bool {
        // For a memoryless thermal ordinance, compliance is equivalent to the
        // successor state being safe. Hourly/cumulative limits can be layered
        // as additional predicates without changing the r_T semantics.
        self.is_state_thermal_safe(to)
    }
}

/// Example usage & basic sanity check.
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ordinance_accepts_safe_state_and_rejects_violation() {
        // Simple two-zone example with symmetric bands.
        let band = ThermalBand {
            t_safe_heat: 30.0,
            t_hard_heat: 35.0,
            t_safe_cold: 18.0,
            t_hard_cold: 15.0,
        };
        let ord = CorridorOrdinance;

        // All zones in the safe interior.
        let state_safe = ThermalState {
            t_indoor: vec![29.0, 20.0],
            bands: vec![band, band],
        };
        assert!(ord.is_state_thermal_safe(&state_safe));

        // One zone violates heat hard limit.
        let state_hot = ThermalState {
            t_indoor: vec![36.0, 20.0],
            bands: vec![band, band],
        };
        assert!(!ord.is_state_thermal_safe(&state_hot));

        // One zone violates cold hard limit.
        let state_cold = ThermalState {
            t_indoor: vec![29.0, 14.0],
            bands: vec![band, band],
        };
        assert!(!ord.is_state_thermal_safe(&state_cold));
    }
}
