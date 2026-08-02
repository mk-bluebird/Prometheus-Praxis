// File: cpp/data_acq/drone_spectral_ingest.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <iomanip>
#include <chrono>

// drone_spectral_ingest:
// - Simulates reading a GeoTIFF-like grid from a Micasense RedEdge payload.
// - Computes per-hex NDVI and canopy cover from red/NIR bands.
// - Writes results into a simple time-series structure for habitat regeneration simulation.
//
// This implementation avoids external GeoTIFF libraries and uses a synthetic spectral grid.

namespace eco {

struct SpectralPixel {
    double red; // reflectance (0..1)
    double nir; // reflectance (0..1)
};

struct SpectralTile {
    int width;
    int height;
    std::vector<SpectralPixel> pixels;
};

struct HexCanopyRecord {
    std::string hex_id;
    double mean_ndvi;
    double canopy_cover_fraction;
    std::chrono::system_clock::time_point timestamp;
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

// Simple time-series database stub: stores hex canopy records.
class TimeSeriesDB {
public:
    void insert(const HexCanopyRecord& rec) {
        records_.push_back(rec);
    }

    void print_all() const {
        std::cout << std::fixed << std::setprecision(3);
        for (const auto& r : records_) {
            std::time_t tt = std::chrono::system_clock::to_time_t(r.timestamp);
            std::cout << "Hex " << r.hex_id
                      << " NDVI=" << r.mean_ndvi
                      << " canopy=" << (r.canopy_cover_fraction * 100.0) << "% "
                      << " at " << std::put_time(std::gmtime(&tt), "%F %T") << "\n";
        }
    }

private:
    std::vector<HexCanopyRecord> records_;
};

class DroneSpectralIngest {
public:
    DroneSpectralIngest(int width,
                        int height,
                        double lat_center_deg,
                        double lon_center_deg,
                        double hex_size_km)
        : width_(width),
          height_(height),
          lat_center_deg_(lat_center_deg),
          lon_center_deg_(lon_center_deg),
          anchor_(hex_size_km, lat_center_deg, lon_center_deg) {}

    SpectralTile read_geotiff() const {
        SpectralTile tile;
        tile.width = width_;
        tile.height = height_;
        tile.pixels.resize(width_ * height_);

        // Synthetic canopy pattern: higher NDVI in patches.
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                double dx = (x - width_ / 2.0) / (width_ / 2.0);
                double dy = (y - height_ / 2.0) / (height_ / 2.0);
                double r2 = dx * dx + dy * dy;
                double ndvi = 0.2 + 0.6 * std::exp(-r2 * 3.0); // 0.2-0.8
                // Convert NDVI to red/nir reflectance:
                // NDVI = (NIR - RED) / (NIR + RED)
                double nir = 0.4 + 0.3 * ndvi;
                double red = nir * (1.0 - ndvi) / (1.0 + ndvi + 1e-6);
                tile.pixels[y * width_ + x] = SpectralPixel{red, nir};
            }
        }
        return tile;
    }

    std::vector<HexCanopyRecord> compute_per_hex(const SpectralTile& tile,
                                                 double pixel_scale_m) const {
        struct Accum {
            double ndvi_sum = 0.0;
            int count = 0;
            int canopy_pixels = 0;
        };
        std::unordered_map<std::string, Accum> acc;

        for (int y = 0; y < tile.height; ++y) {
            for (int x = 0; x < tile.width; ++x) {
                const SpectralPixel& p = tile.pixels[y * tile.width + x];
                double ndvi = compute_ndvi(p.red, p.nir);

                double dx_m = (x - tile.width / 2.0) * pixel_scale_m;
                double dy_m = (y - tile.height / 2.0) * pixel_scale_m;
                double lat_deg = lat_center_deg_ + dy_m / 111320.0;
                double lon_deg = lon_center_deg_ + dx_m / (111320.0 * std::cos(lat_center_deg_ * M_PI / 180.0));

                std::string hex_id = anchor_.compute_hex_anchor(lat_deg, lon_deg);
                auto& a = acc[hex_id];
                a.ndvi_sum += ndvi;
                a.count += 1;
                if (ndvi > 0.5) { // canopy threshold
                    a.canopy_pixels += 1;
                }
            }
        }

        std::vector<HexCanopyRecord> records;
        records.reserve(acc.size());
        auto now = std::chrono::system_clock::now();
        for (const auto& kv : acc) {
            const auto& a = kv.second;
            if (a.count == 0) continue;
            double mean_ndvi = a.ndvi_sum / static_cast<double>(a.count);
            double canopy_fraction = static_cast<double>(a.canopy_pixels) /
                                     static_cast<double>(a.count);
            HexCanopyRecord rec;
            rec.hex_id = kv.first;
            rec.mean_ndvi = mean_ndvi;
            rec.canopy_cover_fraction = canopy_fraction;
            rec.timestamp = now;
            records.push_back(rec);
        }
        return records;
    }

private:
    int width_;
    int height_;
    double lat_center_deg_;
    double lon_center_deg_;
    HexAnchorPhoenix anchor_;

    static double compute_ndvi(double red, double nir) {
        double num = nir - red;
        double den = nir + red + 1e-6;
        return num / den;
    }
};

} // namespace eco

int main() {
    using namespace eco;

    int width = 100;
    int height = 100;
    double lat_center = 33.4484;
    double lon_center = -112.0740;
    double hex_size_km = 0.5;
    double pixel_scale_m = 1.0;

    DroneSpectralIngest ingest(width, height, lat_center, lon_center, hex_size_km);
    SpectralTile tile = ingest.read_geotiff();
    auto records = ingest.compute_per_hex(tile, pixel_scale_m);

    TimeSeriesDB tsdb;
    for (const auto& r : records) {
        tsdb.insert(r);
    }

    tsdb.print_all();

    return 0;
}
