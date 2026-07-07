// crates/cyboquatic-qpudatashard/src/lib.rs
#![forbid(unsafe_code)]

//! Cyboquatic qpudatashard core types.
//!
//! This crate defines a normalized risk-vector for Cyboquatic eco-restoration
//! workloads and a macro for generating typed structs from ALN-style field
//! definitions. All coordinates are scalar f64 values expected in [0.0, 1.0].

use serde::{Deserialize, Serialize};

/// Macro to generate a risk-vector struct from an ALN-style definition.
///
/// The syntax is:
///
/// ```ignore
/// make_risk_vector_struct! {
///     /// Documentation for the risk vector.
///     pub struct CyboquaticRiskVector {
///         r_heat_local               => "r.heat.local",
///         r_heat_rate                => "r.heat.rate",
///         r_cold_local               => "r.cold.local",
///         r_cold_rate                => "r.cold.rate",
///         r_surcharge_head           => "r.hydraulic.surcharge.head",
///         r_surcharge_duration       => "r.hydraulic.surcharge.duration",
///         r_biofilm_surface          => "r.biofilm.surface",
///         r_biofilm_growth           => "r.biofilm.growth",
///         r_pathogen_conc            => "r.pathogen.concentration",
///         r_pathogen_trend           => "r.pathogen.trend",
///         r_cec_conc                 => "r.cec.concentration",
///         r_cec_pulse                => "r.cec.pulse",
///         r_sat_infiltration_deficit => "r.sat.infiltration.deficit",
///         r_sat_clogging             => "r.sat.clogging",
///         r_thermal_plume_extent     => "r.thermal.plume.extent",
///         r_thermal_plume_gradient   => "r.thermal.plume.gradient",
///     }
/// }
/// ```
///
/// Each field will be:
/// - A public `f64`
/// - Serialized/deserialized using `serde` with the given ALN risk-name as
///   the JSON key.
/// - Interpreted as a normalized risk coordinate in [0.0, 1.0].
#[macro_export]
macro_rules! make_risk_vector_struct {
    (
        $( #[$meta:meta] )*
        $vis:vis struct $name:ident {
            $( $field:ident => $aln_name:expr ),* $(,)?
        }
    ) => {
        $( #[$meta] )*
        #[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
        $vis struct $name {
            $(
                #[serde(rename = $aln_name)]
                pub $field: f64,
            )*
        }

        impl $name {
            /// Construct a new risk vector, clamping all coordinates into [0.0, 1.0].
            pub fn new_clamped(
                $( $field: f64 ),*
            ) -> Self {
                fn clamp01(v: f64) -> f64 {
                    if v < 0.0 {
                        0.0
                    } else if v > 1.0 {
                        1.0
                    } else {
                        v
                    }
                }

                Self {
                    $(
                        $field: clamp01($field),
                    )*
                }
            }

            /// Return true if all coordinates are within [0.0, 1.0] (inclusive).
            pub fn is_normalized(&self) -> bool {
                $(
                    (self.$field >= 0.0) && (self.$field <= 1.0)
                )&&*
            }
        }
    };
}

// Concrete Cyboquatic qpudatashard risk vector.
//
// This struct is intended to be the canonical r-vector for eco-restoration
// telemetry covering:
// - Heat / cold
// - Hydraulic surcharge
// - Biofilm / fouling
// - Pathogens
// - CECs (PFBS, PFAS, pharmaceuticals)
// - SAT performance
// - Thermal plumes
make_risk_vector_struct! {
    /// Normalized Cyboquatic risk vector for eco-restoration qpudatashards.
    ///
    /// All fields are scalar f64 values in [0.0, 1.0] and are bound to
    /// ALN risk coordinate names via `serde` rename attributes.
    pub struct CyboquaticRiskVector {
        // Heat coordinates
        r_heat_local               => "r.heat.local",
        r_heat_rate                => "r.heat.rate",

        // Cold coordinates
        r_cold_local               => "r.cold.local",
        r_cold_rate                => "r.cold.rate",

        // Hydraulic surcharge
        r_surcharge_head           => "r.hydraulic.surcharge.head",
        r_surcharge_duration       => "r.hydraulic.surcharge.duration",

        // Biofilm / fouling
        r_biofilm_surface          => "r.biofilm.surface",
        r_biofilm_growth           => "r.biofilm.growth",

        // Pathogens
        r_pathogen_conc            => "r.pathogen.concentration",
        r_pathogen_trend           => "r.pathogen.trend",

        // CECs
        r_cec_conc                 => "r.cec.concentration",
        r_cec_pulse                => "r.cec.pulse",

        // SAT performance
        r_sat_infiltration_deficit => "r.sat.infiltration.deficit",
        r_sat_clogging             => "r.sat.clogging",

        // Thermal plumes
        r_thermal_plume_extent     => "r.thermal.plume.extent",
        r_thermal_plume_gradient   => "r.thermal.plume.gradient",
    }
}

#[cfg(test)]
mod tests {
    use super::CyboquaticRiskVector;

    #[test]
    fn clamping_works() {
        let rv = CyboquaticRiskVector::new_clamped(
            -0.1, 1.2, // heat
            0.0, 1.1,  // cold
            0.5, 0.9,  // surcharge
            2.0, -5.0, // biofilm
            0.3, 0.7,  // pathogens
            0.4, 0.6,  // CECs
            0.8, 0.2,  // SAT
            0.9, 1.5,  // plume
        );
        assert!(rv.is_normalized());
        assert_eq!(rv.r_heat_local, 0.0);
        assert_eq!(rv.r_heat_rate, 1.0);
        assert_eq!(rv.r_biofilm_surface, 1.0);
        assert_eq!(rv.r_biofilm_growth, 0.0);
        assert_eq!(rv.r_thermal_plume_gradient, 1.0);
    }
}
