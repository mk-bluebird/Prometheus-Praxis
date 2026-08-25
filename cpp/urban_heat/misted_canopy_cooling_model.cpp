#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr double kWaterDensityKgPerL = 1.0;
constexpr double kWaterSpecificHeatJPerKgK = 4181.3;
constexpr double kLatentHeatReferenceJPerKg = 2'501'000.0;
constexpr double kLatentHeatTemperatureSlopeJPerKgK = 2'361.0;
constexpr double kStandardPressurePa = 101'325.0;
constexpr double kMaximumDropletDiameterUm = 500.0;
constexpr double kMinimumDropletDiameterUm = 5.0;
constexpr double kMaximumWaterMassFlowKgPerS = 0.050;
constexpr double kMaximumAirSpeedMPerS = 25.0;
constexpr double kMaximumCanopyVolumeM3 = 100'000.0;
constexpr double kMaximumParasiticPowerW = 1'000'000.0;
constexpr double kMaximumDriftRisk = 1.0;
constexpr double kMaximumUncertaintyFraction = 0.35;
constexpr double kMinimumMeasurementQuality = 0.90;
constexpr double kMinimumEvaporationFraction = 0.0;
constexpr double kMaximumEvaporationFraction = 1.0;
constexpr double kDryZoneMinimumEvaporationFraction = 0.0;
constexpr double kMaxRelativeHumidity = 1.0;
constexpr double kMinRelativeHumidity = 0.0;
constexpr double kMinPressurePa = 70'000.0;
constexpr double kMaxPressurePa = 110'000.0;
constexpr double kMinAmbientTemperatureC = -10.0;
constexpr double kMaxAmbientTemperatureC = 60.0;
constexpr double kMinWaterTemperatureC = 0.0;
constexpr double kMaxWaterTemperatureC = 60.0;

enum class OperationalStatus {
    Qualified,
    HoldForReview
};

enum class ClaimScope {
    EvaporationPotentialOnly,
    FieldValidatedLocalCooling,
    NoPublicClaim
};

struct Input {
    std::string canopyId;
    std::string nozzleModel;
    std::string nozzleConfiguration;
    double waterMassFlowKgPerS;
    double waterTemperatureC;
    double ambientAirTemperatureC;
    double relativeHumidityFraction;
    double barometricPressurePa;
    double localAirSpeedMPerS;
    std::vector<double> dropletDiameterDistributionUm;
    double canopyVolumeM3;
    std::string residenceTimeModelVersion;
    double pumpPowerW;
    double fanPowerW;
    std::string waterQualityStatus;
    std::string maintenanceStatus;
    std::string drainageStatus;
    double measurementQuality;
    std::string modelCalibrationStatus;
    std::string fieldValidationStatus;
};

struct Result {
    double latentCoolingPotentialW;
    double sensibleCoolingPotentialW;
    double parasiticPowerW;
    double effectiveCoolingPotentialW;
    double estimatedEvaporationFraction;
    double estimatedDriftRisk;
    double waterUseLPerHour;
    double uncertaintyLowerW;
    double uncertaintyUpperW;
    OperationalStatus operationalStatus;
    ClaimScope claimScope;
    std::string reviewReason;
};

double clamp(double value, double lower, double upper) {
    return std::max(lower, std::min(value, upper));
}

bool isFinite(double value) {
    return std::isfinite(value);
}

void requireNonEmpty(std::string_view value, std::string_view name) {
    if (value.empty()) {
        throw std::invalid_argument(std::string(name) + " must not be empty");
    }
}

void requireRange(
    std::string_view name,
    double value,
    double minimum,
    double maximum
) {
    if (!isFinite(value) || value < minimum || value > maximum) {
        throw std::invalid_argument(
            std::string(name) + " must be finite and within the permitted range"
        );
    }
}

double parseDouble(const char* raw, std::string_view name) {
    try {
        std::size_t consumed = 0;
        const std::string input(raw);
        const double value = std::stod(input, &consumed);
        if (consumed != input.size() || !isFinite(value)) {
            throw std::invalid_argument("invalid numeric input");
        }
        return value;
    } catch (const std::exception&) {
        throw std::invalid_argument(std::string(name) + " must be a finite number");
    }
}

std::vector<double> parseCommaSeparatedPositiveDoubles(
    const char* raw,
    std::string_view name
) {
    const std::string input(raw);
    if (input.empty()) {
        throw std::invalid_argument(std::string(name) + " must not be empty");
    }

    std::vector<double> values;
    std::size_t start = 0;

    while (start < input.size()) {
        const std::size_t end = input.find(',', start);
        const std::string token = input.substr(
            start,
            end == std::string::npos ? std::string::npos : end - start
        );

        if (token.empty()) {
            throw std::invalid_argument(std::string(name) + " contains an empty value");
        }

        std::size_t consumed = 0;
        double value = 0.0;

        try {
            value = std::stod(token, &consumed);
        } catch (const std::exception&) {
            throw std::invalid_argument(std::string(name) + " contains an invalid value");
        }

        if (consumed != token.size()) {
            throw std::invalid_argument(std::string(name) + " contains an invalid value");
        }

        requireRange(
            std::string(name) + " item",
            value,
            kMinimumDropletDiameterUm,
            kMaximumDropletDiameterUm
        );

        values.push_back(value);

        if (end == std::string::npos) {
            break;
        }

        start = end + 1;
    }

    return values;
}

double arithmeticMean(const std::vector<double>& values) {
    if (values.empty()) {
        throw std::invalid_argument("droplet diameter distribution must not be empty");
    }

    double sum = 0.0;
    for (const double value : values) {
        sum += value;
    }

    return sum / static_cast<double>(values.size());
}

double coefficientOfVariation(const std::vector<double>& values, double mean) {
    if (values.size() < 2 || mean <= 0.0) {
        return 0.0;
    }

    double sumSquares = 0.0;
    for (const double value : values) {
        const double delta = value - mean;
        sumSquares += delta * delta;
    }

    const double variance = sumSquares / static_cast<double>(values.size() - 1);
    return std::sqrt(variance) / mean;
}

bool equalsApproved(std::string_view value) {
    return value == "approved";
}

bool hasRequiredOperationalApprovals(const Input& input) {
    return equalsApproved(input.waterQualityStatus)
        && equalsApproved(input.maintenanceStatus)
        && equalsApproved(input.drainageStatus)
        && equalsApproved(input.modelCalibrationStatus);
}

bool hasFieldValidation(const Input& input) {
    return equalsApproved(input.fieldValidationStatus);
}

double latentHeatOfVaporizationJPerKg(double waterTemperatureC) {
    const double latentHeat = kLatentHeatReferenceJPerKg
        - kLatentHeatTemperatureSlopeJPerKgK * waterTemperatureC;

    return std::max(2'200'000.0, latentHeat);
}

double estimateResidenceTimeSeconds(
    const Input& input,
    double meanDropletDiameterUm
) {
    const double characteristicLengthM = std::cbrt(input.canopyVolumeM3);
    const double airTransportSpeedMPerS = std::max(input.localAirSpeedMPerS, 0.20);

    const double baseResidenceTimeS = characteristicLengthM / airTransportSpeedMPerS;
    const double dropletFallPenalty = clamp(
        meanDropletDiameterUm / kMaximumDropletDiameterUm,
        0.0,
        1.0
    );

    return std::max(0.05, baseResidenceTimeS * (1.0 - 0.65 * dropletFallPenalty));
}

double estimateEvaporationTimescaleSeconds(
    const Input& input,
    double meanDropletDiameterUm
) {
    const double diameterFactor = std::pow(
        meanDropletDiameterUm / 50.0,
        2.0
    );

    const double humidityFactor = 0.20
        + 1.80 * clamp(input.relativeHumidityFraction, 0.0, 1.0);

    const double temperatureDrive = clamp(
        (input.ambientAirTemperatureC - input.waterTemperatureC + 10.0) / 50.0,
        0.20,
        1.50
    );

    const double airflowFactor = 1.0 / clamp(
        0.40 + input.localAirSpeedMPerS,
        0.40,
        10.0
    );

    const double pressureFactor = clamp(
        input.barometricPressurePa / kStandardPressurePa,
        0.70,
        1.10
    );

    return std::max(
        0.05,
        1.20 * diameterFactor * humidityFactor * airflowFactor
            * pressureFactor / temperatureDrive
    );
}

double estimateEvaporationFraction(
    const Input& input,
    double meanDropletDiameterUm
) {
    const double residenceTimeS = estimateResidenceTimeSeconds(input, meanDropletDiameterUm);
    const double evaporationTimescaleS = estimateEvaporationTimescaleSeconds(
        input,
        meanDropletDiameterUm
    );

    const double fraction = 1.0 - std::exp(-residenceTimeS / evaporationTimescaleS);

    return clamp(
        fraction,
        kMinimumEvaporationFraction,
        kMaximumEvaporationFraction
    );
}

double estimateSensibleFraction(
    const Input& input,
    double evaporationFraction
) {
    if (input.ambientAirTemperatureC <= input.waterTemperatureC) {
        return 0.0;
    }

    const double humidityAvailability = 1.0 - input.relativeHumidityFraction;
    const double airflowAvailability = clamp(input.localAirSpeedMPerS / 2.5, 0.0, 1.0);

    return clamp(
        0.15 + 0.45 * evaporationFraction
            + 0.20 * humidityAvailability
            + 0.20 * airflowAvailability,
        0.0,
        1.0
    );
}

double estimateDriftRisk(
    const Input& input,
    double meanDropletDiameterUm,
    double dropletCoefficientOfVariation
) {
    const double fineDropletRisk = clamp(
        (80.0 - meanDropletDiameterUm) / 75.0,
        0.0,
        1.0
    );

    const double airSpeedRisk = clamp(
        (input.localAirSpeedMPerS - 1.5) / 5.0,
        0.0,
        1.0
    );

    const double distributionRisk = clamp(
        dropletCoefficientOfVariation / 1.0,
        0.0,
        1.0
    );

    return clamp(
        0.55 * fineDropletRisk
            + 0.30 * airSpeedRisk
            + 0.15 * distributionRisk,
        0.0,
        kMaximumDriftRisk
    );
}

double estimateUncertaintyFraction(
    const Input& input,
    double dropletCoefficientOfVariation,
    double driftRisk
) {
    const double measurementUncertainty = 1.0 - input.measurementQuality;
    const double humidityUncertainty = input.relativeHumidityFraction > 0.85 ? 0.07 : 0.02;
    const double distributionUncertainty = clamp(
        dropletCoefficientOfVariation * 0.10,
        0.0,
        0.10
    );
    const double driftUncertainty = driftRisk * 0.12;
    const double modelUncertainty = hasFieldValidation(input) ? 0.05 : 0.16;

    return clamp(
        measurementUncertainty
            + humidityUncertainty
            + distributionUncertainty
            + driftUncertainty
            + modelUncertainty,
        0.0,
        0.80
    );
}

Result holdForReview(
    const Input& input,
    double evaporationFraction,
    double driftRisk,
    double waterUseLPerHour,
    double latentCoolingPotentialW,
    double sensibleCoolingPotentialW,
    double parasiticPowerW,
    double effectiveCoolingPotentialW,
    double uncertaintyFraction,
    std::string reason
) {
    const double uncertaintyMagnitude =
        std::abs(effectiveCoolingPotentialW) * uncertaintyFraction;

    return {
        latentCoolingPotentialW,
        sensibleCoolingPotentialW,
        parasiticPowerW,
        effectiveCoolingPotentialW,
        evaporationFraction,
        driftRisk,
        waterUseLPerHour,
        effectiveCoolingPotentialW - uncertaintyMagnitude,
        effectiveCoolingPotentialW + uncertaintyMagnitude,
        OperationalStatus::HoldForReview,
        ClaimScope::NoPublicClaim,
        std::move(reason)
    };
}

Result evaluate(const Input& input) {
    requireNonEmpty(input.canopyId, "canopy_id");
    requireNonEmpty(input.nozzleModel, "nozzle_model");
    requireNonEmpty(input.nozzleConfiguration, "nozzle_configuration");
    requireNonEmpty(input.residenceTimeModelVersion, "residence_time_model_version");
    requireNonEmpty(input.waterQualityStatus, "water_quality_status");
    requireNonEmpty(input.maintenanceStatus, "maintenance_status");
    requireNonEmpty(input.drainageStatus, "drainage_status");
    requireNonEmpty(input.modelCalibrationStatus, "model_calibration_status");
    requireNonEmpty(input.fieldValidationStatus, "field_validation_status");

    requireRange(
        "water_mass_flow_kg_s",
        input.waterMassFlowKgPerS,
        0.0,
        kMaximumWaterMassFlowKgPerS
    );
    requireRange(
        "water_temperature_c",
        input.waterTemperatureC,
        kMinWaterTemperatureC,
        kMaxWaterTemperatureC
    );
    requireRange(
        "ambient_air_temperature_c",
        input.ambientAirTemperatureC,
        kMinAmbientTemperatureC,
        kMaxAmbientTemperatureC
    );
    requireRange(
        "relative_humidity_fraction",
        input.relativeHumidityFraction,
        kMinRelativeHumidity,
        kMaxRelativeHumidity
    );
    requireRange(
        "barometric_pressure_pa",
        input.barometricPressurePa,
        kMinPressurePa,
        kMaxPressurePa
    );
    requireRange(
        "local_air_speed_m_s",
        input.localAirSpeedMPerS,
        0.0,
        kMaximumAirSpeedMPerS
    );
    requireRange(
        "canopy_volume_m3",
        input.canopyVolumeM3,
        0.1,
        kMaximumCanopyVolumeM3
    );
    requireRange("pump_power_w", input.pumpPowerW, 0.0, kMaximumParasiticPowerW);
    requireRange("fan_power_w", input.fanPowerW, 0.0, kMaximumParasiticPowerW);
    requireRange(
        "measurement_quality",
        input.measurementQuality,
        0.0,
        1.0
    );

    const double meanDropletDiameterUm = arithmeticMean(input.dropletDiameterDistributionUm);
    const double dropletCv = coefficientOfVariation(
        input.dropletDiameterDistributionUm,
        meanDropletDiameterUm
    );
    const double evaporationFraction = estimateEvaporationFraction(
        input,
        meanDropletDiameterUm
    );
    const double sensibleFraction = estimateSensibleFraction(
        input,
        evaporationFraction
    );
    const double driftRisk = estimateDriftRisk(
        input,
        meanDropletDiameterUm,
        dropletCv
    );

    const double latentCoolingPotentialW =
        input.waterMassFlowKgPerS
        * evaporationFraction
        * latentHeatOfVaporizationJPerKg(input.waterTemperatureC);

    const double sensibleCoolingPotentialW =
        input.waterMassFlowKgPerS
        * sensibleFraction
        * kWaterSpecificHeatJPerKgK
        * std::max(input.ambientAirTemperatureC - input.waterTemperatureC, 0.0);

    const double parasiticPowerW = input.pumpPowerW + input.fanPowerW;
    const double effectiveCoolingPotentialW =
        latentCoolingPotentialW
        + sensibleCoolingPotentialW
        - parasiticPowerW;

    const double waterUseLPerHour =
        input.waterMassFlowKgPerS
        * 3600.0
        / kWaterDensityKgPerL;

    const double uncertaintyFraction = estimateUncertaintyFraction(
        input,
        dropletCv,
        driftRisk
    );

    if (!hasRequiredOperationalApprovals(input)) {
        return holdForReview(
            input,
            evaporationFraction,
            driftRisk,
            waterUseLPerHour,
            latentCoolingPotentialW,
            sensibleCoolingPotentialW,
            parasiticPowerW,
            effectiveCoolingPotentialW,
            uncertaintyFraction,
            "water quality, maintenance, drainage, or model calibration is not approved"
        );
    }

    if (input.measurementQuality < kMinimumMeasurementQuality) {
        return holdForReview(
            input,
            evaporationFraction,
            driftRisk,
            waterUseLPerHour,
            latentCoolingPotentialW,
            sensibleCoolingPotentialW,
            parasiticPowerW,
            effectiveCoolingPotentialW,
            uncertaintyFraction,
            "measurement quality is below the approved minimum"
        );
    }

    if (driftRisk > 0.50) {
        return holdForReview(
            input,
            evaporationFraction,
            driftRisk,
            waterUseLPerHour,
            latentCoolingPotentialW,
            sensibleCoolingPotentialW,
            parasiticPowerW,
            effectiveCoolingPotentialW,
            uncertaintyFraction,
            "estimated droplet drift risk exceeds the approved planning boundary"
        );
    }

    if (uncertaintyFraction > kMaximumUncertaintyFraction) {
        return holdForReview(
            input,
            evaporationFraction,
            driftRisk,
            waterUseLPerHour,
            latentCoolingPotentialW,
            sensibleCoolingPotentialW,
            parasiticPowerW,
            effectiveCoolingPotentialW,
            uncertaintyFraction,
            "model uncertainty exceeds the approved planning boundary"
        );
    }

    if (evaporationFraction < kDryZoneMinimumEvaporationFraction) {
        return holdForReview(
            input,
            evaporationFraction,
            driftRisk,
            waterUseLPerHour,
            latentCoolingPotentialW,
            sensibleCoolingPotentialW,
            parasiticPowerW,
            effectiveCoolingPotentialW,
            uncertaintyFraction,
            "evaporation estimate does not support an intended cooling-zone result"
        );
    }

    const double uncertaintyMagnitude =
        std::abs(effectiveCoolingPotentialW) * uncertaintyFraction;

    return {
        latentCoolingPotentialW,
        sensibleCoolingPotentialW,
        parasiticPowerW,
        effectiveCoolingPotentialW,
        evaporationFraction,
        driftRisk,
        waterUseLPerHour,
        effectiveCoolingPotentialW - uncertaintyMagnitude,
        effectiveCoolingPotentialW + uncertaintyMagnitude,
        OperationalStatus::Qualified,
        hasFieldValidation(input)
            ? ClaimScope::FieldValidatedLocalCooling
            : ClaimScope::EvaporationPotentialOnly,
        "all safety and quality gates passed; result remains decision support only"
    };
}

const char* toString(OperationalStatus status) {
    switch (status) {
        case OperationalStatus::Qualified:
            return "qualified";
        case OperationalStatus::HoldForReview:
            return "hold_for_review";
    }

    return "hold_for_review";
}

const char* toString(ClaimScope scope) {
    switch (scope) {
        case ClaimScope::EvaporationPotentialOnly:
            return "evaporation_potential_only";
        case ClaimScope::FieldValidatedLocalCooling:
            return "field_validated_local_cooling";
        case ClaimScope::NoPublicClaim:
            return "no_public_claim";
    }

    return "no_public_claim";
}

void printUsage(const char* program) {
    std::cerr
        << "Usage:\n"
        << "  " << program
        << " canopy_id nozzle_model nozzle_configuration"
        << " water_mass_flow_kg_s water_temperature_c ambient_air_temperature_c"
        << " relative_humidity_fraction barometric_pressure_pa local_air_speed_m_s"
        << " droplet_diameters_um_csv canopy_volume_m3 residence_time_model_version"
        << " pump_power_w fan_power_w water_quality_status maintenance_status"
        << " drainage_status measurement_quality model_calibration_status"
        << " field_validation_status\n\n"
        << "Status values requiring qualification: approved\n"
        << "Example:\n"
        << "  " << program
        << " phoenix-canopy-01 low-drift-nozzle v1"
        << " 0.002 24.0 42.0 0.18 100800 1.4"
        << " 30,35,40 120.0 residence-v1"
        << " 120 180 approved approved approved 0.96 approved pending\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    constexpr int kExpectedArgumentCount = 21;

    if (argc != kExpectedArgumentCount) {
        printUsage(argv[0]);
        return 64;
    }

    try {
        const Input input{
            argv[1],
            argv[2],
            argv[3],
            parseDouble(argv[4], "water_mass_flow_kg_s"),
            parseDouble(argv[5], "water_temperature_c"),
            parseDouble(argv[6], "ambient_air_temperature_c"),
            parseDouble(argv[7], "relative_humidity_fraction"),
            parseDouble(argv[8], "barometric_pressure_pa"),
            parseDouble(argv[9], "local_air_speed_m_s"),
            parseCommaSeparatedPositiveDoubles(argv[10], "droplet_diameters_um_csv"),
            parseDouble(argv[11], "canopy_volume_m3"),
            argv[12],
            parseDouble(argv[13], "pump_power_w"),
            parseDouble(argv[14], "fan_power_w"),
            argv[15],
            argv[16],
            argv[17],
            parseDouble(argv[18], "measurement_quality"),
            argv[19],
            argv[20]
        };

        const Result result = evaluate(input);

        std::cout << std::fixed << std::setprecision(6);
        std::cout << "canopy_id=" << input.canopyId << '\n';
        std::cout << "nozzle_model=" << input.nozzleModel << '\n';
        std::cout << "nozzle_configuration=" << input.nozzleConfiguration << '\n';
        std::cout << "residence_time_model_version="
                  << input.residenceTimeModelVersion << '\n';
        std::cout << "latent_cooling_potential_w="
                  << result.latentCoolingPotentialW << '\n';
        std::cout << "sensible_cooling_potential_w="
                  << result.sensibleCoolingPotentialW << '\n';
        std::cout << "parasitic_power_w="
                  << result.parasiticPowerW << '\n';
        std::cout << "effective_cooling_potential_w="
                  << result.effectiveCoolingPotentialW << '\n';
        std::cout << "estimated_evaporation_fraction="
                  << result.estimatedEvaporationFraction << '\n';
        std::cout << "estimated_drift_risk="
                  << result.estimatedDriftRisk << '\n';
        std::cout << "water_use_l_h="
                  << result.waterUseLPerHour << '\n';
        std::cout << "uncertainty_lower_w="
                  << result.uncertaintyLowerW << '\n';
        std::cout << "uncertainty_upper_w="
                  << result.uncertaintyUpperW << '\n';
        std::cout << "operational_status="
                  << toString(result.operationalStatus) << '\n';
        std::cout << "public_heat_relief_claim_scope="
                  << toString(result.claimScope) << '\n';
        std::cout << "review_reason="
                  << result.reviewReason << '\n';
        std::cout << "commands_physical_infrastructure=false\n";
        std::cout << "model_output_is_decision_support_only=true\n";

        return result.operationalStatus == OperationalStatus::Qualified ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "Input error: " << error.what() << '\n';
        return 65;
    }
}
