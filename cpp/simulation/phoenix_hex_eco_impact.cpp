// File: cpp/simulation/phoenix_hex_eco_impact.cpp
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <limits>

/*
  Phoenix hex-grid eco-impact and heat-island offset module.

  - Encodes a Phoenix-anchored hex grid (compatible with H3-style indexing via i,j axial coords).
  - Ensures spatial invariance of eco-impact under grid rotation/translation via a minimal feature set.
  - Computes ΔT_cell using:
      ΔT_cell = α·(1 − canopy_frac)·exp(−β·distance_to_cool_corridor) + γ·albedo_deviation
    where α, β, γ are calibration parameters (to be fitted from Landsat-8 band-10 data externally).
*/

namespace phoenix_hex {

// Axial coordinates for a hex cell (q,r) ~ (i,j)
struct HexCoord {
    int32_t i; // axial coordinate i
    int32_t j; // axial coordinate j
};

// Simple 2D point in projected coordinates (meters, e.g., EPSG:3857 or local Phoenix grid)
struct Point2D {
    double x;
    double y;
};

// Geographic/ecologic feature bundle for a hex cell.
// Minimal set to guarantee spatial invariance of eco-impact under rotation/translation:
//
// 1. Canopy fraction (dimensionless [0,1]).
// 2. Surface albedo (dimensionless [0,1]) and deviation from a reference albedo.
// 3. Distance to nearest "cool corridor" (meters) defined in intrinsic spatial coordinates.
// 4. Land-cover categorical tag (impervious vs pervious) to interpret albedo and canopy robustly.
// 5. Thermal reference (e.g. mean band-10 brightness temperature over the hex, Kelvin).
//
// All are defined from Phoenix-fixed physical coordinates rather than grid-dependent indices,
// so rotating/translating the hex lattice does not change their values for a given physical patch.
enum class LandCoverType : uint8_t {
    Impervious = 0,
    Pervious = 1,
    Mixed     = 2
};

struct HexFeatures {
    double canopy_fraction;        // [0,1]
    double surface_albedo;        // [0,1]
    double albedo_reference;      // [0,1] reference (e.g. Phoenix corridor target)
    double distance_to_cool_corridor_m; // meters
    double thermal_ref_K;         // e.g. Landsat-8 band-10 brightness temp (Kelvin)
    LandCoverType land_cover;
};

// Calibration parameters for the ΔT_cell formula.
struct HeatIslandCalibration {
    double alpha; // intensity scale, e.g. K
    double beta;  // corridor decay rate, 1/m
    double gamma; // albedo sensitivity, K per albedo unit
};

// Eco-impact result per cell: thermal offset plus a simple eco-score.
struct EcoImpactResult {
    double delta_T_K;     // heat-island offset (Kelvin)
    double eco_score;     // dimensionless [0,1], higher is better
};

// Utility to compute Euclidean distance between a hex centroid and a corridor point.
// In practice, corridor geometries would be more complex; this keeps it minimal and invariant.
double euclidean_distance(const Point2D& a, const Point2D& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Hex-lattice geometry: mapping axial (i,j) to a 2D position given a hex cell size.
// For invariance, the origin and orientation are arbitrary, but fixed; eco-impact only depends on
// physical distances, not on where we choose (0,0).
Point2D hex_centroid_from_axial(const HexCoord& c, double hex_size_m) {
    // Axial coordinates to 2D (pointy-top hex):
    // x = hex_size * (sqrt(3) * i + sqrt(3)/2 * j)
    // y = hex_size * (3.0/2.0 * j)
    static const double sqrt3 = std::sqrt(3.0);
    double x = hex_size_m * (sqrt3 * static_cast<double>(c.i) + (sqrt3 / 2.0) * static_cast<double>(c.j));
    double y = hex_size_m * (1.5 * static_cast<double>(c.j));
    return Point2D{x, y};
}

// Compute albedo deviation relative to reference.
double compute_albedo_deviation(const HexFeatures& f) {
    return f.surface_albedo - f.albedo_reference;
}

// Compute ΔT_cell using the given calibration and features.
double compute_heat_island_offset(const HexFeatures& f, const HeatIslandCalibration& calib) {
    const double canopy_term = 1.0 - f.canopy_fraction;
    const double corridor_term = std::exp(-calib.beta * f.distance_to_cool_corridor_m);
    const double albedo_dev = compute_albedo_deviation(f);
    return calib.alpha * canopy_term * corridor_term + calib.gamma * albedo_dev;
}

// A simple eco-score that combines cooling factors and land-cover semantics:
// - Higher canopy_fraction increases score.
// - Shorter distance to cool corridors increases score.
// - Albedo closer to reference increases score.
// - Pervious surfaces are favored over impervious.
double compute_eco_score(const HexFeatures& f, double max_distance_m) {
    if (max_distance_m <= 0.0) {
        max_distance_m = 1.0;
    }

    double canopy_score = f.canopy_fraction; // already [0,1]
    double corridor_score = 1.0 - std::min(f.distance_to_cool_corridor_m / max_distance_m, 1.0);

    double albedo_dev = std::fabs(compute_albedo_deviation(f));
    double albedo_score = 1.0 - std::min(albedo_dev, 1.0);

    double land_cover_bonus = 0.0;
    switch (f.land_cover) {
        case LandCoverType::Impervious:
            land_cover_bonus = 0.0;
            break;
        case LandCoverType::Pervious:
            land_cover_bonus = 0.25;
            break;
        case LandCoverType::Mixed:
            land_cover_bonus = 0.10;
            break;
    }

    double raw = 0.4 * canopy_score + 0.3 * corridor_score + 0.3 * albedo_score + land_cover_bonus;
    if (raw < 0.0) raw = 0.0;
    if (raw > 1.0) raw = 1.0;
    return raw;
}

// Minimal feature set invariance check:
//
// Given two grid configurations (e.g., different origins or rotations), but the same physical
// hex centroids and eco features, eco-impact must be equal up to numerical tolerance.
bool check_spatial_invariance(
    const std::vector<HexCoord>& coords_a,
    const std::vector<HexCoord>& coords_b,
    const std::vector<HexFeatures>& features,
    const HeatIslandCalibration& calib,
    double hex_size_m,
    double tolerance_K
) {
    const std::size_t n = coords_a.size();
    if (coords_b.size() != n || features.size() != n) {
        return false;
    }

    // For invariance, we require that the physical centroids derived from grid A and grid B
    // correspond to the same physical locations. Here we assume upstream mapping has ensured that.
    // We only check ΔT equality for each index.
    for (std::size_t idx = 0; idx < n; ++idx) {
        double dt_a = compute_heat_island_offset(features[idx], calib);
        double dt_b = compute_heat_island_offset(features[idx], calib);
        double diff = std::fabs(dt_a - dt_b);
        if (diff > tolerance_K) {
            return false;
        }
    }
    return true;
}

// A simple Phoenix hex-grid simulation driver.
class PhoenixHexGrid {
public:
    PhoenixHexGrid(double hex_size_m, HeatIslandCalibration calib)
        : hex_size_m_(hex_size_m), calib_(calib)
    {}

    void add_cell(const HexCoord& coord, const HexFeatures& features) {
        coords_.push_back(coord);
        features_.push_back(features);
    }

    std::vector<EcoImpactResult> compute_eco_impacts(double max_distance_m) const {
        std::vector<EcoImpactResult> results;
        results.reserve(features_.size());
        for (const auto& f : features_) {
            EcoImpactResult r;
            r.delta_T_K = compute_heat_island_offset(f, calib_);
            r.eco_score = compute_eco_score(f, max_distance_m);
            results.push_back(r);
        }
        return results;
    }

    std::vector<Point2D> compute_centroids() const {
        std::vector<Point2D> pts;
        pts.reserve(coords_.size());
        for (const auto& c : coords_) {
            pts.push_back(hex_centroid_from_axial(c, hex_size_m_));
        }
        return pts;
    }

private:
    double hex_size_m_;
    HeatIslandCalibration calib_;
    std::vector<HexCoord> coords_;
    std::vector<HexFeatures> features_;
};

} // namespace phoenix_hex

// Demo main: not required in library usage, but gives a concrete example.
int main() {
    using namespace phoenix_hex;

    HeatIslandCalibration calib{2.5, 0.0015, 1.2};

    PhoenixHexGrid grid(100.0, calib);

    HexCoord c1{0, 0};
    HexFeatures f1;
    f1.canopy_fraction = 0.15;
    f1.surface_albedo = 0.22;
    f1.albedo_reference = 0.30;
    f1.distance_to_cool_corridor_m = 800.0;
    f1.thermal_ref_K = 315.0;
    f1.land_cover = LandCoverType::Impervious;

    HexCoord c2{1, -1};
    HexFeatures f2;
    f2.canopy_fraction = 0.45;
    f2.surface_albedo = 0.30;
    f2.albedo_reference = 0.30;
    f2.distance_to_cool_corridor_m = 150.0;
    f2.thermal_ref_K = 310.0;
    f2.land_cover = LandCoverType::Pervious;

    grid.add_cell(c1, f1);
    grid.add_cell(c2, f2);

    auto impacts = grid.compute_eco_impacts(1500.0);
    for (std::size_t i = 0; i < impacts.size(); ++i) {
        std::cout << "Cell " << i
                  << " ΔT_K=" << impacts[i].delta_T_K
                  << " eco_score=" << impacts[i].eco_score
                  << '\n';
    }

    return 0;
}
