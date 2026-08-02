// File: cpp/eco_restoration/air_temperature_downscaling_from_lst.cpp

#include <vector>
#include <string>
#include <cmath>
#include <iostream>

/**
 * 36. Data assimilation of air temperature measurements into hex UHI map.
 *
 * Landsat-derived UHI is based on land surface temperature (LST), whereas
 * weather stations measure near-surface air temperature (T_air). We seek a
 * downscaling scheme that converts hex-level LST_h to air temperature T_air,h
 * using NDVI and building height H_h, assimilating station data.[65][59][165][168]
 *
 * Model:
 *   T_air,h ≈ a_0 + a_1 LST_h + a_2 NDVI_h + a_3 H_h + ε_h
 *
 * where:
 *   - NDVI_h moderates the LST→air coupling (more vegetation reduces air temp).
 *   - H_h (mean building height) modulates canyon effects and heat storage.
 *
 * We fit (a_0, a_1, a_2, a_3) using station data mapped to nearby hexes:
 *   T_air_obs,h, LST_h, NDVI_h, H_h.
 *
 * Then, for all hexes, we predict:
 *   T_air_pred,h = a_0 + a_1 LST_h + a_2 NDVI_h + a_3 H_h
 *
 * and define air-based UHI:
 *   UHI_air,h = T_air_pred,h - T_air_rural_ref
 *
 * This blends ground air measurements and satellite LST using NDVI and
 * building height as physical covariates.
 */

struct StationHexSample {
    double lst_h;
    double ndvi_h;
    double H_h;       // mean building height
    double T_air_obs; // observed air temperature from station
};

struct DownscaleCoeffs {
    double a0;
    double a1;
    double a2;
    double a3;
};

DownscaleCoeffs fit_downscale_model(const std::vector<StationHexSample>& samples) {
    int n = static_cast<int>(samples.size());
    if (n < 4) {
        throw std::invalid_argument("Need at least 4 samples to fit downscale model.");
    }

    const int k = 4;
    double XtX[k][k] = {{0.0}};
    double XtY[k] = {0.0};

    for (const auto& s : samples) {
        double x[k] = {s.lst_h, s.ndvi_h, s.H_h, 1.0};
        for (int i = 0; i < k; ++i) {
            XtY[i] += x[i] * s.T_air_obs;
            for (int j = 0; j < k; ++j) {
                XtX[i][j] += x[i] * x[j];
            }
        }
    }

    // Solve XtX * beta = XtY.
    double A[k][k + 1];
    for (int i = 0; i < k; ++i) {
        for (int j = 0; j < k; ++j) {
            A[i][j] = XtX[i][j];
        }
        A[i][k] = XtY[i];
    }

    for (int pivot = 0; pivot < k; ++pivot) {
        int max_row = pivot;
        double max_val = std::fabs(A[pivot][pivot]);
        for (int i = pivot + 1; i < k; ++i) {
            double val = std::fabs(A[i][pivot]);
            if (val > max_val) {
                max_val = val;
                max_row = i;
            }
        }
        if (max_val < 1e-12) {
            throw std::runtime_error("Singular XtX in downscale model.");
        }
        if (max_row != pivot) {
            for (int j = 0; j <= k; ++j) {
                std::swap(A[pivot][j], A[max_row][j]);
            }
        }

        double diag = A[pivot][pivot];
        for (int j = pivot; j <= k; ++j) {
            A[pivot][j] /= diag;
        }
        for (int i = pivot + 1; i < k; ++i) {
            double factor = A[i][pivot];
            for (int j = pivot; j <= k; ++j) {
                A[i][j] -= factor * A[pivot][j];
            }
        }
    }

    double beta[k];
    for (int i = k - 1; i >= 0; --i) {
        double sum = A[i][k];
        for (int j = i + 1; j < k; ++j) {
            sum -= A[i][j] * beta[j];
        }
        beta[i] = sum;
    }

    DownscaleCoeffs coeffs;
    coeffs.a1 = beta[0];
    coeffs.a2 = beta[1];
    coeffs.a3 = beta[2];
    coeffs.a0 = beta[3];
    return coeffs;
}

struct HexDownscaleInput {
    std::string hex_id;
    double lst_h;
    double ndvi_h;
    double H_h;
};

struct HexAirTempResult {
    std::string hex_id;
    double T_air_pred;
    double UHI_air;
};

HexAirTempResult predict_air_temp(const HexDownscaleInput& h,
                                  const DownscaleCoeffs& coeffs,
                                  double T_air_rural_ref) {
    double T_air_pred =
        coeffs.a0 +
        coeffs.a1 * h.lst_h +
        coeffs.a2 * h.ndvi_h +
        coeffs.a3 * h.H_h;

    double UHI_air = T_air_pred - T_air_rural_ref;
    return {h.hex_id, T_air_pred, UHI_air};
}

int main_downscale() {
    // Synthetic station-hex calibration data.
    std::vector<StationHexSample> samples = {
        {42.0, 0.30, 10.0, 39.0},
        {40.0, 0.45, 8.0, 36.5},
        {44.0, 0.20, 12.0, 40.5},
        {38.5, 0.50, 6.0, 35.0}
    };

    DownscaleCoeffs coeffs = fit_downscale_model(samples);

    HexDownscaleInput hex{"hex_10_20", 41.5, 0.35, 9.0};
    double T_air_rural_ref = 35.0;

    HexAirTempResult res = predict_air_temp(hex, coeffs, T_air_rural_ref);

    std::cout << "Hex " << res.hex_id
              << " | T_air_pred=" << res.T_air_pred
              << " | UHI_air=" << res.UHI_air << "\n";

    return 0;
}
