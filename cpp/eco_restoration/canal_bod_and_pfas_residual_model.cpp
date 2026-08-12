// File: cpp/eco_restoration/canal_bod_and_pfas_residual_model.cpp
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace ppx::eco_restoration {

struct CanalTelemetry {
    double water_temperature_c{};
    double flow_m3_s{};
    double wetted_cross_section_m2{};
    double segment_length_m{};
    double bod_upstream_mg_l{};
    double k20_per_day{};
    double temperature_coefficient{1.047};
};

class BodDownstreamPredictor {
public:
    [[nodiscard]] double operator()(const CanalTelemetry& t) const {
        if (t.flow_m3_s <= 0.0 || t.wetted_cross_section_m2 <= 0.0 ||
            t.segment_length_m < 0.0 || t.bod_upstream_mg_l < 0.0 ||
            t.k20_per_day < 0.0 || t.temperature_coefficient <= 0.0 ||
            !std::isfinite(t.water_temperature_c)) {
            throw std::invalid_argument("invalid BOD canal telemetry");
        }

        const double velocity_m_s = t.flow_m3_s / t.wetted_cross_section_m2;
        const double residence_days = t.segment_length_m / velocity_m_s / 86400.0;
        const double k_temperature_per_day =
            t.k20_per_day * std::pow(t.temperature_coefficient, t.water_temperature_c - 20.0);
        return t.bod_upstream_mg_l * std::exp(-k_temperature_per_day * residence_days);
    }

    [[nodiscard]] double decay_constant_per_day(const CanalTelemetry& t) const {
        return t.k20_per_day *
               std::pow(t.temperature_coefficient, t.water_temperature_c - 20.0);
    }
};

struct PfAsTransportConfig {
    double segment_length_m{};
    double velocity_m_s{};
    double dispersion_m2_s{};
    double cold_reference_c{20.0};
    double cold_decay_coefficient{1.0};
    double reference_decay_per_s{};
    double temperature_c{};
    double dt_s{};
    double mass_matrix_weight{1.0};
};

struct PfAsResidual {
    double v_current{};
    double v_next{};
    double delta_v{};
    double cold_survival_factor{};
    bool numerically_stable{};
    bool residual_corridor_passed{};
};

class PfAsFiniteDifferenceResidual {
public:
    [[nodiscard]] PfAsResidual step(
        std::vector<double>& concentration_mg_l,
        const PfAsTransportConfig& config,
        double maximum_delta_v) const {
        validate(concentration_mg_l, config, maximum_delta_v);

        const std::size_t count = concentration_mg_l.size();
        const double dx = config.segment_length_m / static_cast<double>(count - 1);
        const double courant = config.velocity_m_s * config.dt_s / dx;
        const double diffusion = config.dispersion_m2_s * config.dt_s / (dx * dx);
        const double cold_survival =
            std::exp(-config.cold_decay_coefficient *
                     std::max(0.0, config.cold_reference_c - config.temperature_c));
        const double decay_per_s = config.reference_decay_per_s * cold_survival;
        const bool stable = courant + 2.0 * diffusion + decay_per_s * config.dt_s <= 1.0;

        if (!stable) {
            throw std::runtime_error("PFAS finite-difference stability corridor exceeded");
        }

        const double v_current = residual(concentration_mg_l, dx, config.mass_matrix_weight);
        std::vector<double> next(count, 0.0);
        next.front() = concentration_mg_l.front();
        next.back() = concentration_mg_l.back();

        for (std::size_t i = 1; i + 1 < count; ++i) {
            const double advection = -courant * (concentration_mg_l[i] - concentration_mg_l[i - 1]);
            const double dispersion = diffusion * (concentration_mg_l[i + 1] -
                                                   2.0 * concentration_mg_l[i] +
                                                   concentration_mg_l[i - 1]);
            const double reaction = -decay_per_s * config.dt_s * concentration_mg_l[i];
            next[i] = std::max(0.0, concentration_mg_l[i] + advection + dispersion + reaction);
        }

        const double v_next = residual(next, dx, config.mass_matrix_weight);
        concentration_mg_l.swap(next);
        return {
            v_current,
            v_next,
            v_next - v_current,
            cold_survival,
            stable,
            v_next - v_current <= maximum_delta_v
        };
    }

private:
    static double residual(const std::vector<double>& c, double dx, double weight) {
        double v = 0.0;
        for (const double value : c) v += weight * dx * value * value;
        return v;
    }

    static void validate(const std::vector<double>& c, const PfAsTransportConfig& x,
                         double maximum_delta_v) {
        if (c.size() < 3 || x.segment_length_m <= 0.0 || x.velocity_m_s < 0.0 ||
            x.dispersion_m2_s < 0.0 || x.reference_decay_per_s < 0.0 ||
            x.cold_decay_coefficient < 0.0 || x.dt_s <= 0.0 ||
            x.mass_matrix_weight <= 0.0 || !std::isfinite(maximum_delta_v)) {
            throw std::invalid_argument("invalid PFAS transport configuration");
        }
        for (const double value : c) {
            if (!std::isfinite(value) || value < 0.0) {
                throw std::invalid_argument("PFAS concentration must be finite and non-negative");
            }
        }
    }
};

}  // namespace ppx::eco_restoration
