// File: cpp/eco_restoration/ppx_workload_engine.cpp
#include <Eigen/Dense>
#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace ppx::eco_restoration {

constexpr bool finite(double value) noexcept {
    constexpr double maximum = 1.7976931348623157e308;
    return value == value && value >= -maximum && value <= maximum;
}

struct NumericTelemetry {
    double r_hydraulics{};
    double r_energy{};
    double r_uncertainty{};
    double r_reliability{};
    double roh{};
};

template <typename Object, auto Member>
struct FieldRange {
    double minimum{};
    double maximum{};

    [[nodiscard]] constexpr bool accepts(const Object& object) const noexcept {
        const double value = object.*Member;
        return finite(value) && value >= minimum && value <= maximum;
    }
};

template <typename Object, typename... Rules>
class TelemetryValidator {
public:
    constexpr explicit TelemetryValidator(Rules... rules) : rules_(std::move(rules)...) {}

    [[nodiscard]] constexpr bool operator()(const Object& object) const noexcept {
        return std::apply(
            [&object](const auto&... rule) constexpr { return (rule.accepts(object) && ...); },
            rules_);
    }

private:
    std::tuple<Rules...> rules_;
};

template <typename Object, auto Member>
constexpr auto allowed_range(double minimum, double maximum) {
    return FieldRange<Object, Member>{minimum, maximum};
}

template <typename Object, typename... Rules>
constexpr auto make_telemetry_validator(Rules... rules) {
    return TelemetryValidator<Object, Rules...>(std::move(rules)...);
}

constexpr auto validate_numeric = make_telemetry_validator<NumericTelemetry>(
    allowed_range<NumericTelemetry, &NumericTelemetry::r_hydraulics>(0.0, 1.0),
    allowed_range<NumericTelemetry, &NumericTelemetry::r_energy>(0.0, 1.0),
    allowed_range<NumericTelemetry, &NumericTelemetry::r_uncertainty>(0.0, 1.0),
    allowed_range<NumericTelemetry, &NumericTelemetry::r_reliability>(0.0, 1.0),
    allowed_range<NumericTelemetry, &NumericTelemetry::roh>(0.0, 1.0));

struct EnergyInputs {
    double power_w{};
    double duration_s{};
    double renewable_fraction{};
    double grid_carbon_g_per_kwh{};
    double reference_carbon_g{};
};

[[nodiscard]] double energy_risk(const EnergyInputs& input) {
    if (!finite(input.power_w) || !finite(input.duration_s) ||
        !finite(input.renewable_fraction) || !finite(input.grid_carbon_g_per_kwh) ||
        !finite(input.reference_carbon_g) || input.power_w < 0.0 ||
        input.duration_s < 0.0 || input.renewable_fraction < 0.0 ||
        input.renewable_fraction > 1.0 || input.grid_carbon_g_per_kwh < 0.0 ||
        input.reference_carbon_g <= 0.0) {
        throw std::invalid_argument("invalid energy-risk input");
    }
    const double carbon_g = input.power_w * input.duration_s *
        (1.0 - input.renewable_fraction) * input.grid_carbon_g_per_kwh / 3'600'000.0;
    return std::clamp(carbon_g / input.reference_carbon_g, 0.0, 1.0);
}

[[nodiscard]] double independent_variance(
    std::span<const double> jacobian, std::span<const double> sigma) {
    if (jacobian.size() != sigma.size()) throw std::invalid_argument("Jacobian dimensions differ");
    double variance = 0.0;
    for (std::size_t i = 0; i < jacobian.size(); ++i) {
        if (sigma[i] < 0.0 || !finite(sigma[i])) throw std::invalid_argument("invalid uncertainty");
        variance += jacobian[i] * jacobian[i] * sigma[i] * sigma[i];
    }
    return variance;
}

[[nodiscard]] double energy_risk_variance(
    const EnergyInputs& input, std::span<const double> sigma) {
    if (sigma.size() != 4) throw std::invalid_argument("energy uncertainty requires four values");
    const double denominator = 3'600'000.0 * input.reference_carbon_g;
    const double raw = input.power_w * input.duration_s * (1.0 - input.renewable_fraction) *
        input.grid_carbon_g_per_kwh / denominator;
    if (raw <= 0.0 || raw >= 1.0) return 0.0;

    const std::array<double, 4> jacobian{
        input.duration_s * (1.0 - input.renewable_fraction) * input.grid_carbon_g_per_kwh / denominator,
        input.power_w * (1.0 - input.renewable_fraction) * input.grid_carbon_g_per_kwh / denominator,
        -input.power_w * input.duration_s * input.grid_carbon_g_per_kwh / denominator,
        input.power_w * input.duration_s * (1.0 - input.renewable_fraction) / denominator
    };
    return independent_variance(jacobian, sigma);
}

struct GroundControlPoint {
    Eigen::Vector2d predicted_utm{};
    Eigen::Vector2d surveyed_utm{};
    double sigma_m{0.02};
};

struct HexGridCalibration {
    double scale{};
    double rotation_rad{};
    Eigen::Vector2d offset_utm{};
    double residual_rms_m{};

    [[nodiscard]] Eigen::Vector2d apply(const Eigen::Vector2d& predicted_utm) const {
        const double cosine = std::cos(rotation_rad);
        const double sine = std::sin(rotation_rad);
        return {
            scale * (cosine * predicted_utm.x() - sine * predicted_utm.y()) + offset_utm.x(),
            scale * (sine * predicted_utm.x() + cosine * predicted_utm.y()) + offset_utm.y()
        };
    }
};

[[nodiscard]] HexGridCalibration calibrate_hex_grid(
    const std::vector<GroundControlPoint>& points) {
    if (points.size() < 2) throw std::invalid_argument("two or more control points are required");

    Eigen::MatrixXd design(2 * points.size(), 4);
    Eigen::VectorXd observations(2 * points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto& point = points[i];
        if (!finite(point.sigma_m) || point.sigma_m <= 0.0) {
            throw std::invalid_argument("invalid RTK uncertainty");
        }
        const double weight = 1.0 / point.sigma_m;
        const double x = point.predicted_utm.x();
        const double y = point.predicted_utm.y();
        design.row(2 * i) << weight * x, -weight * y, weight, 0.0;
        design.row(2 * i + 1) << weight * y, weight * x, 0.0, weight;
        observations[2 * i] = weight * point.surveyed_utm.x();
        observations[2 * i + 1] = weight * point.surveyed_utm.y();
    }

    const Eigen::ColPivHouseholderQR<Eigen::MatrixXd> qr(design);
    if (qr.rank() < 4) throw std::invalid_argument("control-point geometry is rank deficient");
    const Eigen::Vector4d solution = qr.solve(observations);
    HexGridCalibration result{
        std::hypot(solution[0], solution[1]),
        std::atan2(solution[1], solution[0]),
        {solution[2], solution[3]},
        0.0
    };

    double squared_error = 0.0;
    for (const auto& point : points) {
        squared_error += (result.apply(point.predicted_utm) - point.surveyed_utm).squaredNorm();
    }
    result.residual_rms_m = std::sqrt(squared_error / (2.0 * points.size()));
    return result;
}

struct CanalEnergyTelemetry {
    double area_m2{};
    double interval_s{};
    double net_radiation_w_m2{};
    double bed_flux_w_m2{};
    double sensible_flux_w_m2{};
    double latent_flux_w_m2{};
    double flow_m3_s{};
    double upstream_temperature_c{};
    double downstream_temperature_c{};
};

struct CanalEnergyBalance {
    double net_storage_flux_w_m2{};
    double evaporation_loss_m3{};
    double thermal_advection_w{};
};

[[nodiscard]] CanalEnergyBalance canal_energy_balance(const CanalEnergyTelemetry& input) {
    if (input.area_m2 <= 0.0 || input.interval_s <= 0.0 || input.flow_m3_s < 0.0) {
        throw std::invalid_argument("invalid canal-energy geometry");
    }
    constexpr double density = 998.0;
    constexpr double specific_heat = 4181.0;
    const double mean_temperature = 0.5 * (input.upstream_temperature_c + input.downstream_temperature_c);
    const double latent_heat = 2.501e6 - 2361.0 * mean_temperature;
    if (latent_heat <= 0.0 || !finite(latent_heat)) throw std::invalid_argument("invalid water temperature");

    return {
        input.net_radiation_w_m2 - input.bed_flux_w_m2 - input.sensible_flux_w_m2 - input.latent_flux_w_m2,
        std::max(0.0, input.latent_flux_w_m2) * input.area_m2 * input.interval_s / latent_heat / density,
        density * specific_heat * input.flow_m3_s *
            (input.upstream_temperature_c - input.downstream_temperature_c)
    };
}

class LuaSubscriberBroadcaster {
public:
    explicit LuaSubscriberBroadcaster(std::vector<std::filesystem::path> subscriber_paths)
        : paths_(std::move(subscriber_paths)), descriptor_(socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0)) {
        if (descriptor_ < 0) throw std::runtime_error("cannot create Unix datagram socket");
        for (const auto& path : paths_) {
            if (!path.is_absolute() || !std::filesystem::is_directory(path.parent_path())) {
                close(descriptor_);
                throw std::invalid_argument("subscriber socket directory is unavailable");
            }
        }
    }

    ~LuaSubscriberBroadcaster() { if (descriptor_ >= 0) close(descriptor_); }

    void publish(std::string_view frame) const noexcept {
        for (const auto& path : paths_) {
            const std::string text = path.string();
            if (text.size() >= sizeof(sockaddr_un::sun_path)) continue;
            sockaddr_un address{};
            address.sun_family = AF_UNIX;
            std::memcpy(address.sun_path, text.c_str(), text.size() + 1);
            sendto(descriptor_, frame.data(), frame.size(), MSG_DONTWAIT,
                reinterpret_cast<const sockaddr*>(&address), sizeof(address));
        }
    }

private:
    std::vector<std::filesystem::path> paths_;
    int descriptor_{-1};
};

}  // namespace ppx::eco_restoration
