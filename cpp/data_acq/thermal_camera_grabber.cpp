// File: cpp/data_acq/thermal_camera_grabber.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>
#include <thread>
#include <iomanip>

// thermal_camera_grabber:
// - Simulates capturing frames from a FLIR Lepton thermal camera over SPI.
// - Converts raw pixel values to temperatures (C).
// - Aggregates per-hex surface temperatures using a Phoenix hex anchor scheme.
// - Feeds results into a simple heat-island mitigation input structure.
//
// This code is self-contained and uses a synthetic frame generator instead of real SPI I/O.

namespace eco {

struct ThermalPixel {
    uint16_t raw_value; // simulated raw sensor value
};

struct ThermalFrame {
    int width;
    int height;
    std::vector<ThermalPixel> pixels;
};

struct HexTemperatureAggregate {
    std::string hex_id;
    double mean_temp_C;
    double max_temp_C;
};

class HexAnchorPhoenix {
public:
    HexAnchorPhoenix(double hex_size_km,
                     double lat_origin_deg,
                     double lon_origin_deg)
        : hex_size_km_(hex_size_km),
          lat_origin_deg_(lat_origin_deg),
          lon_origin_deg_(lon_origin_deg) {}

    std::string compute_hex_anchor(double lat_deg, double lon_deg) const {
        double x_km = 0.0;
        double y_km = 0.0;
        project_to_local(lat_deg, lon_deg, x_km, y_km,
                         lat_origin_deg_, lon_origin_deg_);

        double q = (std::sqrt(3.0) / 3.0 * x_km - 1.0 / 3.0 * y_km) / hex_size_km_;
        double r = (2.0 / 3.0 * y_km) / hex_size_km_;

        int qi = static_cast<int>(std::round(q));
        int ri = static_cast<int>(std::round(r));

        return "PHX-HX-" + std::to_string(qi) + "-" + std::to_string(ri);
    }

private:
    double hex_size_km_;
    double lat_origin_deg_;
    double lon_origin_deg_;

    static void project_to_local(double lat_deg, double lon_deg,
                                 double& x_km, double& y_km,
                                 double lat_origin_deg,
                                 double lon_origin_deg) {
        const double R = 6371.0;
        double lat_rad = lat_deg * M_PI / 180.0;
        double lon_rad = lon_deg * M_PI / 180.0;
        double lat0_rad = lat_origin_deg * M_PI / 180.0;
        double lon0_rad = lon_origin_deg * M_PI / 180.0;

        double dx = (lon_rad - lon0_rad) * std::cos(lat0_rad);
        double dy = (lat_rad - lat0_rad);

        x_km = R * dx;
        y_km = R * dy;
    }
};

class ThermalCameraGrabber {
public:
    ThermalCameraGrabber(int width,
                         int height,
                         double lat_origin_deg,
                         double lon_origin_deg,
                         double hex_size_km)
        : width_(width),
          height_(height),
          anchor_(hex_size_km, lat_origin_deg, lon_origin_deg) {}

    ThermalFrame capture_frame() const {
        ThermalFrame frame;
        frame.width = width_;
        frame.height = height_;
        frame.pixels.resize(width_ * height_);

        // Synthetic gradient: hotter towards center.
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                double dx = (x - width_ / 2.0) / (width_ / 2.0);
                double dy = (y - height_ / 2.0) / (height_ / 2.0);
                double r2 = dx * dx + dy * dy;
                double temp_C = 30.0 + 15.0 * std::exp(-r2 * 4.0); // 30-45 C
                uint16_t raw = static_cast<uint16_t>(temp_C * 100.0); // scale
                frame.pixels[y * width_ + x] = ThermalPixel{raw};
            }
        }
        return frame;
    }

    std::vector<HexTemperatureAggregate> aggregate_per_hex(const ThermalFrame& frame,
                                                           double lat_center_deg,
                                                           double lon_center_deg,
                                                           double pixel_scale_m) const {
        std::vector<HexTemperatureAggregate> aggregates;
        // Map hex_id -> (sum, max, count).
        struct Accum {
            double sum = 0.0;
            double max = -1e9;
            int count = 0;
        };
        std::unordered_map<std::string, Accum> acc;

        for (int y = 0; y < frame.height; ++y) {
            for (int x = 0; x < frame.width; ++x) {
                double temp_C = raw_to_temp(frame.pixels[y * frame.width + x].raw_value);

                // Approximate lat/lon of pixel around center.
                double dx_m = (x - frame.width / 2.0) * pixel_scale_m;
                double dy_m = (y - frame.height / 2.0) * pixel_scale_m;
                double lat_deg = lat_center_deg + dy_m / 111320.0; // ~m/deg
                double lon_deg = lon_center_deg + dx_m / (111320.0 * std::cos(lat_center_deg * M_PI / 180.0));

                std::string hex_id = anchor_.compute_hex_anchor(lat_deg, lon_deg);
                auto& a = acc[hex_id];
                a.sum += temp_C;
                a.count += 1;
                if (temp_C > a.max) a.max = temp_C;
            }
        }

        aggregates.reserve(acc.size());
        for (const auto& kv : acc) {
            HexTemperatureAggregate h;
            h.hex_id = kv.first;
            h.mean_temp_C = kv.second.sum / std::max(1, kv.second.count);
            h.max_temp_C = kv.second.max;
            aggregates.push_back(h);
        }
        return aggregates;
    }

    void feed_to_heat_island_model(const std::vector<HexTemperatureAggregate>& aggs) const {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Heat-island mitigation input (per hex):\n";
        for (const auto& h : aggs) {
            std::cout << "  " << h.hex_id
                      << " mean=" << h.mean_temp_C << " C"
                      << " max=" << h.max_temp_C << " C\n";
        }
    }

private:
    int width_;
    int height_;
    HexAnchorPhoenix anchor_;

    static double raw_to_temp(uint16_t raw) {
        // Simple linear conversion: raw = temp_C * 100.
        return static_cast<double>(raw) / 100.0;
    }
};

} // namespace eco

int main() {
    using namespace eco;

    int width = 80;
    int height = 60;
    double lat_center = 33.4484;
    double lon_center = -112.0740;
    double hex_size_km = 0.5;
    double pixel_scale_m = 5.0;

    ThermalCameraGrabber grabber(width, height, lat_center, lon_center, hex_size_km);

    ThermalFrame frame = grabber.capture_frame();
    auto aggs = grabber.aggregate_per_hex(frame, lat_center, lon_center, pixel_scale_m);
    grabber.feed_to_heat_island_model(aggs);

    return 0;
}
