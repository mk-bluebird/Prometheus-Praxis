// File: cpp/eco_restoration/urban_phoenix_envelopes_evaluation.cpp

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <iomanip>
#include <cmath>

namespace prometheus_praxis {
namespace urban_phoenix {

// -------------------------
// Core governance constants
// -------------------------

struct GovernanceRefs {
    std::string governance_ref; // e.g. "2b586028-a87b-4194-93b8-f5794c194e8a.md"
    std::string ecology_ref;    // e.g. "e4ba68f5-edd6-438f-8dff-822a2527b867.md"
};

constexpr float GLOBAL_ROH_CEILING = 0.30f;

// -----------------------------
// Urban heat envelope (CPP map)
// -----------------------------

struct UrbanHeatEnvelopePhoenix2026v1 {
    std::string tile_id;
    std::string sampled_at_utc;

    float air_temp_c;
    float mrt_c;
    float lst_c;
    float uhi_intensity_c;

    float heat_risk_coordinate;   // r_heat in [0,1]
    float roh_contribution;       // heat contribution to RoH in [0,1]
    float ecoimpact_heat_score;   // EcoImpact from heat

    float safe_heat_max_c;
    std::string safe_heat_window_start_utc;
    std::string safe_heat_window_end_utc;

    std::string sensor_source;
    std::string eco_safety_envelope_id;
    std::string ker_envelope_id;
    std::string roh_envelope_id;

    GovernanceRefs refs;
};

bool invariant_urban_heat_safety(const UrbanHeatEnvelopePhoenix2026v1 &env) {
    if (env.air_temp_c > env.safe_heat_max_c) {
        return false;
    }
    if (env.mrt_c > env.safe_heat_max_c + 5.0f) {
        return false;
    }
    if (env.heat_risk_coordinate < 0.0f || env.heat_risk_coordinate > 1.0f) {
        return false;
    }
    if (env.roh_contribution > GLOBAL_ROH_CEILING) {
        return false;
    }
    return true;
}

// -----------------------------
// Air quality plane (CPP map)
// -----------------------------

struct AirQualityPlanePhoenix2026v1 {
    std::string tile_id;
    std::string sampled_at_utc;

    float ozone_ppb;
    float pm25_ug_per_m3;
    float pm10_ug_per_m3;
    float nox_ppb;

    float air_risk_coordinate;    // r_air in [0,1]
    float ecoimpact_air_score;
    float roh_contribution;

    float ozone_max_ppb;
    float pm25_max_ug_per_m3;
    float pm10_max_ug_per_m3;

    std::string sensor_source;
    std::string eco_safety_envelope_id;
    std::string ker_envelope_id;
    std::string roh_envelope_id;

    GovernanceRefs refs;
};

bool invariant_air_quality_safety(const AirQualityPlanePhoenix2026v1 &env) {
    if (env.ozone_ppb > env.ozone_max_ppb) {
        return false;
    }
    if (env.pm25_ug_per_m3 > env.pm25_max_ug_per_m3) {
        return false;
    }
    if (env.pm10_ug_per_m3 > env.pm10_max_ug_per_m3) {
        return false;
    }
    if (env.air_risk_coordinate < 0.0f || env.air_risk_coordinate > 1.0f) {
        return false;
    }
    if (env.roh_contribution > GLOBAL_ROH_CEILING) {
        return false;
    }
    return true;
}

// ----------------------------------------
// Water flux coordinate (CPP map)
// ----------------------------------------

struct WaterFluxCoordinatePhoenix2026v1 {
    std::string tile_id;
    std::string sampled_at_utc;

    float surface_water_flux_l_per_s;
    float infiltration_rate_mm_per_h;
    std::string cyboquatic_channel_state;

    float soil_salinity_ds_per_m;
    float contaminants_index;
    float soil_moisture_pct;

    float water_risk_coordinate;  // r_water in [0,1]
    float ecoimpact_water_score;
    float roh_contribution;

    float salinity_max_ds_per_m;
    float contaminants_max_index;

    std::string soilsync_run_id;
    bool soilsync_sla_breached;

    std::string sensor_source;
    std::string eco_safety_envelope_id;
    std::string ker_envelope_id;
    std::string roh_envelope_id;

    GovernanceRefs refs;
};

bool invariant_water_flux_safety(const WaterFluxCoordinatePhoenix2026v1 &env) {
    if (env.soil_salinity_ds_per_m > env.salinity_max_ds_per_m) {
        return false;
    }
    if (env.contaminants_index > env.contaminants_max_index) {
        return false;
    }
    if (env.water_risk_coordinate < 0.0f || env.water_risk_coordinate > 1.0f) {
        return false;
    }
    if (env.roh_contribution > GLOBAL_ROH_CEILING) {
        return false;
    }
    return true;
}

struct RestorationTaskUpdate {
    std::string tile_id;
    std::string new_state;
    std::string reason;
};

std::vector<RestorationTaskUpdate>
policy_block_new_tasks_on_water_out_of_corridor(const WaterFluxCoordinatePhoenix2026v1 &env) {
    std::vector<RestorationTaskUpdate> updates;
    bool out_of_corridor = false;

    if (env.contaminants_index > env.contaminants_max_index) {
        out_of_corridor = true;
    }
    if (env.soil_salinity_ds_per_m > env.salinity_max_ds_per_m) {
        out_of_corridor = true;
    }
    if (env.soilsync_sla_breached) {
        out_of_corridor = true;
    }

    if (out_of_corridor) {
        RestorationTaskUpdate u;
        u.tile_id = env.tile_id;
        u.new_state = "PreflightRejected";
        u.reason = "WaterOutOfCorridorOrSoilSLA";
        updates.push_back(u);
    }

    return updates;
}

// -------------------------------------------
// Lyapunov update representation and helpers
// -------------------------------------------

struct LyapunovUpdate {
    std::string plane_id;
    float coordinate;
    float weight;
};

float lyapunov_term(const LyapunovUpdate &upd) {
    return upd.weight * upd.coordinate * upd.coordinate;
}

// ------------------------------
// Evaluation scoring structures
// ------------------------------

enum class ComponentKind {
    AdvectionKernel,
    MARLStack,
    StreamingPipeline
};

struct UrbanClimateModelComponent {
    std::string id;
    ComponentKind kind;
    std::string description;

    float ker_score;
    float roh_score;
    float biodiversity_score;
    float ecological_planes_score;
    float urban_corridors_score;
    float streaming_sla_score;
    float neurorights_score;

    std::string governance_ref;
    std::string ecology_ref;
    std::string notes;
};

struct UrbanClimateModelEvaluationEnvelope {
    std::string envelope_id;
    std::string region_context;
    std::vector<UrbanClimateModelComponent> components;
    std::string created_at_utc;
};

constexpr float MIN_KER_SCORE             = 4.0f;
constexpr float MIN_ROH_SCORE             = 4.0f;
constexpr float MIN_BIODIVERSITY_SCORE    = 4.0f;
constexpr float MIN_ECO_PLANES_SCORE      = 4.0f;
constexpr float MIN_URBAN_CORRIDORS_SCORE = 3.0f;
constexpr float MIN_STREAMING_SLA_SCORE   = 3.0f;
constexpr float MIN_NEURORIGHTS_SCORE     = 3.0f;

enum class EligibilityVerdict {
    Eligible,
    Ineligible
};

struct EligibilityDecision {
    std::string component_id;
    EligibilityVerdict verdict;
    std::string reason;
};

bool min_scores_invariant(const UrbanClimateModelComponent &c) {
    if (c.ker_score             < MIN_KER_SCORE)             return false;
    if (c.roh_score             < MIN_ROH_SCORE)             return false;
    if (c.biodiversity_score    < MIN_BIODIVERSITY_SCORE)    return false;
    if (c.ecological_planes_score < MIN_ECO_PLANES_SCORE)    return false;
    if (c.urban_corridors_score < MIN_URBAN_CORRIDORS_SCORE) return false;
    if (c.streaming_sla_score   < MIN_STREAMING_SLA_SCORE)   return false;
    if (c.neurorights_score     < MIN_NEURORIGHTS_SCORE)     return false;
    return true;
}

std::vector<EligibilityDecision>
enforce_urban_climate_model_eligibility(const UrbanClimateModelEvaluationEnvelope &env) {
    std::vector<EligibilityDecision> decisions;
    decisions.reserve(env.components.size());

    for (const auto &c : env.components) {
        EligibilityDecision d;
        d.component_id = c.id;
        if (min_scores_invariant(c)) {
            d.verdict = EligibilityVerdict::Eligible;
            d.reason  = "Scores meet minimum deployment thresholds.";
        } else {
            d.verdict = EligibilityVerdict::Ineligible;
            d.reason  = "Scores below minimum thresholds; see min_scores_invariant.";
        }
        decisions.push_back(d);
    }

    return decisions;
}

// ----------------------------------------
// Example components referencing envelopes
// ----------------------------------------

UrbanClimateModelComponent make_advection_kernel_A() {
    UrbanClimateModelComponent c;
    c.id          = "AdvectionKernelPhoenixHeatAirA";
    c.kind        = ComponentKind::AdvectionKernel;
    c.description = "Physics-informed advection kernel for Phoenix canopy-layer heat and air quality.";

    c.ker_score               = 4.5f; // emits KER-compatible coordinates from heat/air envelopes.
    c.roh_score               = 4.2f; // binds heat and air RoH contributions under GLOBAL_ROH_CEILING.
    c.biodiversity_score      = 4.0f; // respects BiodiversityHardFloor via corridor integration.[48]
    c.ecological_planes_score = 4.5f; // maps heat, air, and water into Lyapunov planes.[8]
    c.urban_corridors_score   = 4.0f; // uses LocalConstraintWindow safe-heat and schedules.[48]
    c.streaming_sla_score     = 3.5f; // integrates HeatSyncSLA and AirQualitySyncSLA patterns.
    c.neurorights_score       = 3.0f; // neutral; no direct BCI, but aligned with governance spine.[8]

    c.governance_ref = "2b586028-a87b-4194-93b8-f5794c194e8a.md";   // governance spine.[8]
    c.ecology_ref    = "e4ba68f5-edd6-438f-8dff-822a2527b867.md";   // soil/biodiversity corridors.[48]
    c.notes          = "Uses UrbanHeatEnvelopePhoenix2026v1 and AirQualityPlanePhoenix2026v1 for KER/Lyapunov coupling.";

    return c;
}

UrbanClimateModelComponent make_streaming_pipeline_S() {
    UrbanClimateModelComponent c;
    c.id          = "StreamingPipelinePhoenixHeatAirWaterS";
    c.kind        = ComponentKind::StreamingPipeline;
    c.description = "Real-time pipeline for heat, air, and water envelopes with SLA enforcement and evidence chain.";

    c.ker_score               = 4.0f; // streams KER-relevant fields into governance envelopes.[8]
    c.roh_score               = 4.0f; // preserves RoH ceiling semantics via stop gating.
    c.biodiversity_score      = 3.5f; // passes soil/biodiversity corridor states to downstream tasks.[48]
    c.ecological_planes_score = 4.0f; // ensures all envelopes feed Lyapunov planes consistently.
    c.urban_corridors_score   = 3.5f; // respects ErdeSyncSchedule and LocalConstraintWindow windows.[48]
    c.streaming_sla_score     = 4.5f; // implements SoilSyncSLA-like patterns for Heat/Air/Water streams.[48]
    c.neurorights_score       = 3.5f; // maintains evidence chain required by treaty gates and neurorights.[8]

    c.governance_ref = "2b586028-a87b-4194-93b8-f5794c194e8a.md";
    c.ecology_ref    = "e4ba68f5-edd6-438f-8dff-822a2527b867.md";
    c.notes          = "Implements ALN->CPP evidence chain for UrbanHeatEnvelopePhoenix2026v1, AirQualityPlanePhoenix2026v1, WaterFluxCoordinatePhoenix2026v1.";

    return c;
}

UrbanClimateModelComponent make_marl_stack_X() {
    UrbanClimateModelComponent c;
    c.id          = "MARLStackPhoenixUrbanClimateX";
    c.kind        = ComponentKind::MARLStack;
    c.description = "Constraint-aware MARL controller for Phoenix interventions (shade, airflow, cyboquatic routing).";

    c.ker_score               = 4.5f; // observes KEREnvelope, RoHEnvelope, LyapunovEnvelope.[8]
    c.roh_score               = 4.5f; // reward shaped to avoid breaching RoH ceiling.
    c.biodiversity_score      = 4.2f; // encodes BiodiversityHardFloor and EnforceCarbonBiodiversityBalance.[48]
    c.ecological_planes_score = 4.3f; // actions evaluated against heat/air/water Lyapunov planes.
    c.urban_corridors_score   = 4.0f; // respects ErdeSyncSchedule, safe-heat windows, labor/legal notes.[48]
    c.streaming_sla_score     = 3.5f; // assumes SLA-compliant streams; penalizes SLA breaches in policy.
    c.neurorights_score       = 4.0f; // aligned with neurorights/treaty gates for citizen-facing comfort systems.[8]

    c.governance_ref = "2b586028-a87b-4194-93b8-f5794c194e8a.md";
    c.ecology_ref    = "e4ba68f5-edd6-438f-8dff-822a2527b867.md";
    c.notes          = "Uses envelopes as structured observation space; policies bounded by governance spine.";

    return c;
}

// ------------------------------
// Demonstration main routine
// ------------------------------

void print_decision(const EligibilityDecision &d) {
    std::cout << "Component: " << d.component_id
              << " | Verdict: " << (d.verdict == EligibilityVerdict::Eligible ? "Eligible" : "Ineligible")
              << " | Reason: " << d.reason << "\n";
}

int main() {
    // Example envelopes (values illustrative, chosen to respect invariants).
    UrbanHeatEnvelopePhoenix2026v1 heat_env {
        "tile-001",
        "2026-08-02T21:00:00Z",
        38.0f,
        43.0f,
        45.0f,
        5.0f,
        0.7f,
        0.20f,
        0.6f,
        40.0f,
        "2026-08-02T19:00:00Z",
        "2026-08-02T23:00:00Z",
        "MaRTyStation01",
        "EcoSafetyEnvelopePhoenix2026v1",
        "KEREnvelopePhoenix2026v1",
        "RoHEnvelopePhoenix2026v1",
        {"2b586028-a87b-4194-93b8-f5794c194e8a.md",
         "e4ba68f5-edd6-438f-8dff-822a2527b867.md"}
    };

    AirQualityPlanePhoenix2026v1 air_env {
        "tile-001",
        "2026-08-02T21:00:00Z",
        70.0f,
        25.0f,
        40.0f,
        30.0f,
        0.5f,
        0.4f,
        0.15f,
        80.0f,
        35.0f,
        50.0f,
        "AQMonitor01",
        "EcoSafetyEnvelopePhoenix2026v1",
        "KEREnvelopePhoenix2026v1",
        "RoHEnvelopePhoenix2026v1",
        {"2b586028-a87b-4194-93b8-f5794c194e8a.md",
         "e4ba68f5-edd6-438f-8dff-822a2527b867.md"}
    };

    WaterFluxCoordinatePhoenix2026v1 water_env {
        "tile-001",
        "2026-08-02T21:00:00Z",
        10.0f,
        5.0f,
        "Active",
        2.0f,
        0.3f,
        18.0f,
        0.4f,
        0.5f,
        0.20f,
        4.0f,
        0.5f,
        "SoilSyncRun-2026-08-02",
        false,
        "StormwaterSensor01",
        "EcoSafetyEnvelopePhoenix2026v1",
        "KEREnvelopePhoenix2026v1",
        "RoHEnvelopePhoenix2026v1",
        {"2b586028-a87b-4194-93b8-f5794c194e8a.md",
         "e4ba68f5-edd6-438f-8dff-822a2527b867.md"}
    };

    // Check invariants to ensure envelopes are within safe corridors.
    bool heat_ok  = invariant_urban_heat_safety(heat_env);
    bool air_ok   = invariant_air_quality_safety(air_env);
    bool water_ok = invariant_water_flux_safety(water_env);

    std::cout << std::boolalpha;
    std::cout << "Heat envelope safe:  " << heat_ok  << "\n";
    std::cout << "Air envelope safe:   " << air_ok   << "\n";
    std::cout << "Water envelope safe: " << water_ok << "\n";

    // Demonstrate task blocking for water out-of-corridor conditions.
    auto updates = policy_block_new_tasks_on_water_out_of_corridor(water_env);
    for (const auto &u : updates) {
        std::cout << "Task update -> tile: " << u.tile_id
                  << ", new_state: " << u.new_state
                  << ", reason: " << u.reason << "\n";
    }

    // Build evaluation envelope with example components.
    UrbanClimateModelEvaluationEnvelope eval_env;
    eval_env.envelope_id    = "PhoenixUrbanModels2026";
    eval_env.region_context = "PhoenixMetro2026Arid";
    eval_env.created_at_utc = "2026-08-02T22:00:00Z";

    eval_env.components.push_back(make_advection_kernel_A());
    eval_env.components.push_back(make_marl_stack_X());
    eval_env.components.push_back(make_streaming_pipeline_S());

    auto decisions = enforce_urban_climate_model_eligibility(eval_env);
    for (const auto &d : decisions) {
        print_decision(d);
    }

    // Compute Lyapunov contributions from envelopes to illustrate eco-impact binding.
    LyapunovUpdate heat_plane {"HEAT_PHOENIX_2026", heat_env.heat_risk_coordinate, 1.0f};
    LyapunovUpdate air_plane  {"AIR_QUALITY_PHOENIX_2026", air_env.air_risk_coordinate, 1.0f};
    LyapunovUpdate water_plane{"WATER_FOG_PHOENIX_2026", water_env.water_risk_coordinate, 1.0f};

    float V_heat  = lyapunov_term(heat_plane);
    float V_air   = lyapunov_term(air_plane);
    float V_water = lyapunov_term(water_plane);
    float V_total = V_heat + V_air + V_water;

    std::cout << std::setprecision(3);
    std::cout << "Lyapunov planes: V_heat=" << V_heat
              << ", V_air=" << V_air
              << ", V_water=" << V_water
              << ", V_total=" << V_total << "\n";

    return 0;
}

} // namespace urban_phoenix
} // namespace prometheus_praxis
