// File: cpp/tools/corridor_health_pipeline.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>

// Corridor health pipeline wiring pattern:
// - Drone-mounted multispectral camera acquires imagery.
// - Onboard Jetson Nano runs semantic segmentation to produce a land-cover raster.
// - Raster output is partitioned into hex cells (e.g., Phoenix H3-like grid).
// - Each cell's NDVI time series is fed into a Kalman filter.
// - Anomalous degradation (e.g., sudden NDVI drop inconsistent with model) triggers an alert
//   to city arborists for inspection/intervention.

struct NdviSample {
    double ndvi;
    std::int64_t timestamp_s;
};

struct HexNdviTimeSeries {
    std::string hex_id;
    std::vector<NdviSample> samples;
};

// Simple Kalman filter for NDVI trend: state x_t ≈ NDVI_t with process and measurement noise.
class NdviKalmanFilter {
public:
    NdviKalmanFilter(double initial_state,
                     double process_var,
                     double measurement_var)
        : x_(initial_state),
          P_(1.0),
          Q_(process_var),
          R_(measurement_var)
    {}

    double update(double z) {
        // Predict
        double x_pred = x_;
        double P_pred = P_ + Q_;

        // Update
        double K = P_pred / (P_pred + R_);
        x_ = x_pred + K * (z - x_pred);
        P_ = (1.0 - K) * P_pred;

        return x_;
    }

    double state() const { return x_; }
    double variance() const { return P_; }

private:
    double x_;
    double P_;
    double Q_;
    double R_;
};

struct NdviAnomaly {
    std::string hex_id;
    double ndvi_observed;
    double ndvi_expected;
    double deviation;
    std::int64_t timestamp_s;
};

class CorridorHealthMonitor {
public:
    CorridorHealthMonitor(double anomaly_threshold)
        : anomaly_threshold_(anomaly_threshold)
    {}

    std::vector<NdviAnomaly> process_time_series(const HexNdviTimeSeries& ts) {
        std::vector<NdviAnomaly> anomalies;

        if (ts.samples.empty()) {
            return anomalies;
        }

        NdviKalmanFilter kf(ts.samples.front().ndvi, 0.0005, 0.0025);

        for (const auto& s : ts.samples) {
            double expected = kf.state();
            double updated = kf.update(s.ndvi);
            double deviation = s.ndvi - expected;

            if (std::fabs(deviation) > anomaly_threshold_) {
                NdviAnomaly an;
                an.hex_id = ts.hex_id;
                an.ndvi_observed = s.ndvi;
                an.ndvi_expected = expected;
                an.deviation = deviation;
                an.timestamp_s = s.timestamp_s;
                anomalies.push_back(an);
            }
        }

        return anomalies;
    }

private:
    double anomaly_threshold_;
};

void send_arborist_alert(const NdviAnomaly& an) {
    std::cout << "ALERT: Corridor NDVI anomaly in hex " << an.hex_id
              << " at " << an.timestamp_s
              << " observed=" << an.ndvi_observed
              << " expected=" << an.ndvi_expected
              << " deviation=" << an.deviation << "\n";
}

int main() {
    // In real wiring:
    // - Jetson Nano runs semantic segmentation over multispectral frames.
    // - NDVI raster is aggregated per hex and appended to HexNdviTimeSeries for each hex.
    // Here we emulate a single hex's NDVI time series.

    HexNdviTimeSeries ts;
    ts.hex_id = "phoenix_hex_1234";

    std::int64_t t0 = 1700000000;
    for (int i = 0; i < 30; ++i) {
        NdviSample s;
        s.timestamp_s = t0 + i * 86400;
        if (i < 20) {
            s.ndvi = 0.42 + 0.02 * std::sin(i * 0.2);
        } else {
            // Simulate degradation: drop NDVI sharply
            s.ndvi = 0.20 + 0.01 * std::sin(i * 0.2);
        }
        ts.samples.push_back(s);
    }

    CorridorHealthMonitor monitor(0.10);
    auto anomalies = monitor.process_time_series(ts);

    for (const auto& an : anomalies) {
        send_arborist_alert(an);
    }

    return 0;
}
