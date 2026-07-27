// filename: Prometheus-Praxis/src/cpp/waste/shredding/shredding_governance_adapter.hpp
// destination: github.com/mk-bluebird/Prometheus-Praxis/src/cpp/waste/shredding/shredding_governance_adapter.hpp
// license: MIT OR Apache-2.0
//
// Role:
// Governance adapter header defining ShreddingKerSnapshot and
// ShreddingKerAdapter that bind shredding/screening telemetry into the
// EcoNet governance spine (KER triplets, Lyapunov residual Vt, RoH scalar,
// non‑offsettable plane IDs) with read‑only compute methods.

#ifndef PROMETHEUS_PRAXIS_WASTE_SHREDDING_GOVERNANCE_ADAPTER_HPP
#define PROMETHEUS_PRAXIS_WASTE_SHREDDING_GOVERNANCE_ADAPTER_HPP

#include <cstddef>
#include <cstdint>

#include "screening_module.hpp"

namespace prometheus_praxis {
namespace waste {
namespace shredding {

// ---------------------------------------------------------------------------
// Shredder telemetry POD (local to shredding band)
// ---------------------------------------------------------------------------
//
// This struct mirrors non-actuating shredder telemetry: feed, torque,
// power, and coarse particle statistics. It is used as input to the
// governance adapter, which then binds it into KER / Lyapunov surfaces.

struct ShredderTelemetry
{
    double feed_rate_kg_per_h;   // incoming feed to shredder [kg/h]
    double shaft_torque_nm;      // shaft torque [N·m]
    double motor_power_kw;       // motor power [kW]
    double avg_inlet_size_mm;    // average inlet particle size [mm]
    double vt_residual;          // Lyapunov residual slice for shredder [>=0]
    double roh_score;            // RoH kernel ceiling for shredder [0,1]
};

// ---------------------------------------------------------------------------
// EcoNet governance spine bindings: KER snapshot for shredding
// ---------------------------------------------------------------------------
//
// ShreddingKerSnapshot is a read-only governance struct that encodes
// the KER triplet, Lyapunov residual, RoH scalar, and non-offsettable
// plane identifiers used by the EcoNet governance spine.

struct ShreddingKerSnapshot
{
    double k;              // knowledge factor [0,1]
    double e;              // eco-impact factor [0,1]
    double r;              // risk factor [0,1]

    double vt;             // Lyapunov residual Vt [>=0]
    double roh;            // RoH scalar [0,1]

    // Non-offsettable plane identifiers, as numeric IDs that the Rust
    // governance spine maps to EcoNet planes (water, heat, waste, topology).
    std::uint32_t plane_waste_id;
    std::uint32_t plane_topology_id;

    // Lane classification for governance: RESEARCH / PILOT / PRODUCTION / BLOCKED.
    CorridorTag lane_tag;
};

// ---------------------------------------------------------------------------
// Shredding governance adapter
// ---------------------------------------------------------------------------
//
// ShreddingKerAdapter binds shredder + screening telemetry into the
// EcoNet governance spine via pure compute methods. It never performs
// actuation and only exposes read-only snapshots suitable for Rust FFI,
// SQL views, and ALN particles.

class ShreddingKerAdapter
{
public:
    // Construct adapter with non-offsettable plane IDs.
    ShreddingKerAdapter(
        std::uint32_t plane_waste_id,
        std::uint32_t plane_topology_id
    ) noexcept
        : plane_waste_id_(plane_waste_id),
          plane_topology_id_(plane_topology_id)
    {}

    // Compute a governance snapshot from shredder and drum-screen telemetry.
    // Returns a pure ShreddingKerSnapshot that higher layers can persist
    // into EcoNet views (v_shard_ker, v_shard_residual) or bind into ALN
    // particles. No side effects or hardware IO.
    ShreddingKerSnapshot computeKerSnapshot(
        const ShredderTelemetry &shredder,
        const ScreenDrumTelemetry &drum
    ) const noexcept
    {
        ShreddingKerSnapshot out{};

        double k = normalize_knowledge(shredder, drum);
        double e = normalize_eco_impact(shredder, drum);
        double r = normalize_risk(shredder, drum);

        out.k = clamp01(k);
        out.e = clamp01(e);
        out.r = clamp01(r);

        // Lyapunov residual Vt composed from shredder and drum slices.
        double vt = shredder.vt_residual + drum.vt_residual;
        if (vt < 0.0) vt = 0.0;
        out.vt = vt;

        // RoH scalar as a ceiling across shredder and drum.
        double roh = (shredder.roh_score > drum.roh_score)
            ? shredder.roh_score
            : drum.roh_score;
        out.roh = clamp01(roh);

        out.plane_waste_id = plane_waste_id_;
        out.plane_topology_id = plane_topology_id_;

        out.lane_tag = classify_lane(out.k, out.e, out.r, out.roh);

        return out;
    }

    // Compute a governance snapshot including air-classifier telemetry.
    ShreddingKerSnapshot computeKerSnapshot(
        const ShredderTelemetry &shredder,
        const ScreenDrumTelemetry &drum,
        const AirClassifierTelemetry &classifier
    ) const noexcept
    {
        ShreddingKerSnapshot out{};

        double k = normalize_knowledge(shredder, drum, classifier);
        double e = normalize_eco_impact(shredder, drum, classifier);
        double r = normalize_risk(shredder, drum, classifier);

        out.k = clamp01(k);
        out.e = clamp01(e);
        out.r = clamp01(r);

        double vt = shredder.vt_residual + drum.vt_residual + classifier.vt_residual;
        if (vt < 0.0) vt = 0.0;
        out.vt = vt;

        double roh = shredder.roh_score;
        if (drum.roh_score > roh) roh = drum.roh_score;
        if (classifier.roh_score > roh) roh = classifier.roh_score;
        out.roh = clamp01(roh);

        out.plane_waste_id = plane_waste_id_;
        out.plane_topology_id = plane_topology_id_;

        out.lane_tag = classify_lane(out.k, out.e, out.r, out.roh);

        return out;
    }

private:
    std::uint32_t plane_waste_id_;
    std::uint32_t plane_topology_id_;

    static double clamp01(double x) noexcept
    {
        if (x <= 0.0) return 0.0;
        if (x >= 1.0) return 1.0;
        return x;
    }

    // Knowledge factor: how informative shredding/screening window is.
    static double normalize_knowledge(
        const ShredderTelemetry &shredder,
        const ScreenDrumTelemetry &drum
    ) noexcept
    {
        double feed_norm = shredder.feed_rate_kg_per_h <= 0.0
            ? 0.0
            : shredder.feed_rate_kg_per_h / 1000.0;
        double fines_norm = drum.fines_fraction;
        if (feed_norm < 0.0) feed_norm = 0.0;
        if (feed_norm > 1.0) feed_norm = 1.0;
        if (fines_norm < 0.0) fines_norm = 0.0;
        if (fines_norm > 1.0) fines_norm = 1.0;
        return 0.6 * feed_norm + 0.4 * fines_norm;
    }

    static double normalize_knowledge(
        const ShredderTelemetry &shredder,
        const ScreenDrumTelemetry &drum,
        const AirClassifierTelemetry &classifier
    ) noexcept
    {
        double base = normalize_knowledge(shredder, drum);
        double cut_eff = classifier.cut_efficiency;
        if (cut_eff < 0.0) cut_eff = 0.0;
        if (cut_eff > 1.0) cut_eff = 1.0;
        return 0.5 * base + 0.5 * cut_eff;
    }

    // Eco-impact factor: how strongly shredding improves downstream ecology.
    static double normalize_eco_impact(
        const ShredderTelemetry &shredder,
        const ScreenDrumTelemetry &drum
    ) noexcept
    {
        double overs_norm = 1.0 - clamp01(drum.overs_fraction);
        double power_norm = shredder.motor_power_kw <= 0.0
            ? 0.0
            : shredder.motor_power_kw / 250.0;
        if (power_norm < 0.0) power_norm = 0.0;
        if (power_norm > 1.0) power_norm = 1.0;
        return 0.7 * overs_norm + 0.3 * (1.0 - power_norm);
    }

    static double normalize_eco_impact(
        const ShredderTelemetry &shredder,
        const ScreenDrumTelemetry &drum,
        const AirClassifierTelemetry &classifier
    ) noexcept
    {
        double base = normalize_eco_impact(shredder, drum);
        double heavies_norm = 1.0 - clamp01(classifier.heavies_fraction);
        return 0.6 * base + 0.4 * heavies_norm;
    }

    // Risk factor: aggregate KER risk for shredding/screening window.
    static double normalize_risk(
        const ShredderTelemetry &shredder,
        const ScreenDrumTelemetry &drum
    ) noexcept
    {
        double roh_max = (shredder.roh_score > drum.roh_score)
            ? shredder.roh_score
            : drum.roh_score;
        return clamp01(roh_max);
    }

    static double normalize_risk(
        const ShredderTelemetry &shredder,
        const ScreenDrumTelemetry &drum,
        const AirClassifierTelemetry &classifier
    ) noexcept
    {
        double roh_max = shredder.roh_score;
        if (drum.roh_score > roh_max) roh_max = drum.roh_score;
        if (classifier.roh_score > roh_max) roh_max = classifier.roh_score;
        return clamp01(roh_max);
    }

    // Lane classification from KER and RoH.
    static CorridorTag classify_lane(
        double k,
        double e,
        double r,
        double roh
    ) noexcept
    {
        // Basic governance bands:
        // - RESEARCH: low eco-impact or high risk / RoH.
        // - PILOT: moderate eco-impact, bounded risk.
        // - PRODUCTION: strong eco-impact, low risk and RoH.
        // - BLOCKED: otherwise.
        if (roh > 0.7 || r > 0.7) {
            return CorridorTag::RESEARCH;
        }
        if (e >= 0.8 && k >= 0.7 && r <= 0.3 && roh <= 0.3) {
            return CorridorTag::PRODUCTION;
        }
        if (e >= 0.5 && r <= 0.5 && roh <= 0.5) {
            return CorridorTag::PILOT;
        }
        return CorridorTag::BLOCKED;
    }
};

} // namespace shredding
} // namespace waste
} // namespace prometheus_praxis

#endif // PROMETHEUS_PRAXIS_WASTE_SHREDDING_GOVERNANCE_ADAPTER_HPP
