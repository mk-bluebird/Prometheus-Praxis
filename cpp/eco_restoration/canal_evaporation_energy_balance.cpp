// File: cpp/eco_restoration/canal_evaporation_energy_balance.cpp
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ppx::eco_restoration {

struct CanalEnergyTelemetry {
    double hex_water_surface_area_m2{};
    double interval_seconds{};
    double net_radiation_w_m2{};
    double ground_or_bed_flux_w_m2{};
    double sensible_heat_flux_w_m2{};
    double latent_heat_flux_w_m2{};
    double flow_m3_s{};
    double upstream_water_temperature_c{};
    double downstream_water_temperature_c{};
};

struct CanalEnergyBalance {
    double latent_heat_of_vaporization_j_kg{};
    double net_storage_flux_w_m2{};
    double evaporation_loss_kg{};
    double evaporation_loss_m3{};
    double advective_thermal_power_w{};
};

class CanalEvaporationEnergyBalance {
public:
    [[nodiscard]] CanalEnergyBalance operator()(const CanalEnergyTelemetry& t) const {
        validate(t);

        constexpr double water_density_kg_m3 = 998.0;
        constexpr double water_specific_heat_j_kg_k = 4181.0;
        const double mean_temperature_c =
            0.5 * (t.upstream_water_temperature_c + t.downstream_water_temperature_c);
        const double latent_heat_j_kg = 2.501e6 - 2361.0 * mean_temperature_c;
        if (latent_heat_j_kg <= 0.0) {
            throw std::invalid_argument("water temperature is outside latent-heat model bounds");
        }

        const double net_storage_flux =
            t.net_radiation_w_m2 - t.ground_or_bed_flux_w_m2 -
            t.sensible_heat_flux_w_m2 - t.latent_heat_flux_w_m2;
        const double evaporation_kg = std::max(0.0, t.latent_heat_flux_w_m2) *
            t.hex_water_surface_area_m2 * t.interval_seconds / latent_heat_j_kg;
        const double thermal_advection_w = water_density_kg_m3 *
            water_specific_heat_j_kg * t.flow_m3_s *
            (t.upstream_water_temperature_c - t.downstream_water_temperature_c);

        return {
            latent_heat_j_kg,
            net_storage_flux,
            evaporation_kg,
            evaporation_kg / water_density_kg_m3,
            thermal_advection_w
        };
    }

private:
    static void validate(const CanalEnergyTelemetry& t) {
        for (const double value : {
            t.hex_water_surface_area_m2, t.interval_seconds, t.flow_m3_s,
            t.net_radiation_w_m2, t.ground_or_bed_flux_w_m2,
            t.sensible_heat_flux_w_m2, t.latent_heat_flux_w_m2,
            t.upstream_water_temperature_c, t.downstream_water_temperature_c
        }) {
            if (!std::isfinite(value)) {
                throw std::invalid_argument("canal energy telemetry must be finite");
            }
        }
        if (t.hex_water_surface_area_m2 <= 0.0 || t.interval_seconds <= 0.0 ||
            t.flow_m3_s < 0.0) {
            throw std::invalid_argument("canal area, duration, and flow violate bounds");
        }
    }
};

}  // namespace ppx::eco_restoration
