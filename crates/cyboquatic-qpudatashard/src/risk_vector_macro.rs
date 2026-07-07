// ecorestorationshard/crates/cyboquatic-qpudatashard/src/risk_vector_macro.rs
#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

/// Macro to generate a risk-vector struct from an ALN-style definition.
///
/// Usage:
/// make_risk_vector_struct!{
///     /// Cyboquatic qpudatashard risk vector for heat/cold/surcharge/biofilm/pathogens/CEC/SAT/plumes.
///     pub struct CyboquaticRiskVector {
///         // field_name => aln_risk_name
///         r_heat_local              => "r.heat.local",
///         r_heat_rate               => "r.heat.rate",
///         r_cold_local              => "r.cold.local",
///         r_cold_rate               => "r.cold.rate",
///         r_surcharge_head          => "r.hydraulic.surcharge.head",
///         r_surcharge_duration      => "r.hydraulic.surcharge.duration",
///         r_biofilm_surface         => "r.biofilm.surface",
///         r_biofilm_growth          => "r.biofilm.growth",
///         r_pathogen_conc           => "r.pathogen.concentration",
///         r_pathogen_trend          => "r.pathogen.trend",
///         r_cec_conc                => "r.cec.concentration",
///         r_cec_pulse               => "r.cec.pulse",
///         r_sat_infiltration_deficit=> "r.sat.infiltration.deficit",
///         r_sat_clogging            => "r.sat.clogging",
///         r_thermal_plume_extent    => "r.thermal.plume.extent",
///         r_thermal_plume_gradient  => "r.thermal.plume.gradient",
///     }
/// }
///
/// Each field is serialized/deserialized using the ALN risk-name as the JSON key.
/// All fields are f64 and are expected to be in [0.0, 1.0].
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
                    if v < 0.0 { 0.0 } else if v > 1.0 { 1.0 } else { v }
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

// Example concrete risk vector struct for cyboquatic qpudatashard.
// This would typically live in src/lib.rs or a dedicated risk_vector.rs module,
// reusing the macro above.

make_risk_vector_struct! {
    /// Cyboquatic qpudatashard risk vector for heat, cold, surcharge, biofilm,
    /// pathogens, CECs, SAT performance, and thermal plumes.
    pub struct CyboquaticRiskVector {
        r_heat_local               => "r.heat.local",
        r_heat_rate                => "r.heat.rate",
        r_cold_local               => "r.cold.local",
        r_cold_rate                => "r.cold.rate",
        r_surcharge_head           => "r.hydraulic.surcharge.head",
        r_surcharge_duration       => "r.hydraulic.surcharge.duration",
        r_biofilm_surface          => "r.biofilm.surface",
        r_biofilm_growth           => "r.biofilm.growth",
        r_pathogen_conc            => "r.pathogen.concentration",
        r_pathogen_trend           => "r.pathogen.trend",
        r_cec_conc                 => "r.cec.concentration",
        r_cec_pulse                => "r.cec.pulse",
        r_sat_infiltration_deficit => "r.sat.infiltration.deficit",
        r_sat_clogging             => "r.sat.clogging",
        r_thermal_plume_extent     => "r.thermal.plume.extent",
        r_thermal_plume_gradient   => "r.thermal.plume.gradient",
    }
}
