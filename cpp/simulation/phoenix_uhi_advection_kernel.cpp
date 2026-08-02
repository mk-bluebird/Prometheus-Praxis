// File: cpp/simulation/phoenix_uhi_advection_kernel.cpp

#include <cmath>
#include <array>
#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>
#include <limits>

/**
 * Simple hex-grid advection kernel tailored to Phoenix daytime wind climatology.
 *
 * Coordinate convention:
 * - We use an axial hex coordinate system (q,r) with q aligned east-west and r northeast-southwest.
 * - Directions are ordered as: 0=E, 1=NE, 2=NW, 3=W, 4=SW, 5=SE.
 *
 * The advection term per hex h is:
 *   A_h = eta * sum_{j in N(h)} w_hj * (UHI_j - UHI_h)
 *
 * where w_hj is determined by:
 *   w_hj = k_dir(theta_wind, dir_vector_j) * k_dist(d_j)
 *
 * For Phoenix:
 * - Daytime prevailing winds in warm season: predominantly westerly (flow from W to E).
 * - Cool season: predominantly easterly (flow from E to W).
 * - We model this via a wind direction angle and speed, but most users will pass the
 *   climatological mean (e.g., 270 degrees for westerly flow in summer).
 */

struct WindState {
    // Wind direction in meteorological convention (degrees from which the wind blows).
    // 270° = westerly (blowing from west toward east), 90° = easterly, 180° = southerly, etc.
    double direction_deg;
    // Wind speed in m/s (or any consistent unit, only used to scale advection).
    double speed;
};

struct HexCoord {
    int q; // axial coordinate, roughly east-west
    int r; // axial coordinate, roughly northeast-southwest
};

struct HexCell {
    HexCoord coord;
    double uhi;     // UHI intensity or land-surface temperature anomaly
    bool active;    // whether this hex participates (e.g., valid data)
};

class PhoenixAdvectionKernel {
public:
    PhoenixAdvectionKernel(double eta, double base_distance_m)
        : eta_(eta), base_distance_m_(base_distance_m) {
        if (base_distance_m_ <= 0.0) {
            throw std::invalid_argument("base_distance_m must be positive");
        }
    }

    /**
     * Compute advection contribution for a single hex given its neighbors.
     *
     * neighbors must be in fixed directional order:
     *   0=E, 1=NE, 2=NW, 3=W, 4=SW, 5=SE
     *
     * Missing neighbors should be marked with active=false.
     */
    double compute_advection_for_hex(const HexCell& center,
                                     const std::array<HexCell, 6>& neighbors,
                                     const WindState& wind) const {
        if (!center.active) {
            return 0.0;
        }

        // Convert wind direction to a unit flow vector in our grid plane.
        // Meteorological convention: direction_deg is where wind blows FROM.
        // We want a flow vector pointing TO where it blows (opposite).
        double flow_angle_rad = deg_to_rad(normalize_deg(wind.direction_deg + 180.0));
        double flow_vx = std::cos(flow_angle_rad);
        double flow_vy = std::sin(flow_angle_rad);

        double advection_sum = 0.0;
        for (std::size_t dir = 0; dir < neighbors.size(); ++dir) {
            const HexCell& nb = neighbors[dir];
            if (!nb.active) {
                continue;
            }
            double w = directional_weight(dir, flow_vx, flow_vy, wind.speed)
                       * distance_weight(center.coord, nb.coord);
            advection_sum += w * (nb.uhi - center.uhi);
        }

        return eta_ * advection_sum;
    }

    /**
     * Compute advection term for an entire grid.
     *
     * grid is a 2D matrix of HexCell, indexed [row][col], with axial coordinates attached.
     * wind is the prevailing wind state at the Landsat overpass time (e.g., ~10–11am local).
     *
     * The neighbor layout assumes a pointy-top axial grid with even-r horizontal layout.
     */
    std::vector<std::vector<double>> compute_advection_field(
            const std::vector<std::vector<HexCell>>& grid,
            const WindState& wind) const {
        std::size_t rows = grid.size();
        if (rows == 0) {
            return {};
        }
        std::size_t cols = grid[0].size();
        std::vector<std::vector<double>> result(rows, std::vector<double>(cols, 0.0));

        for (std::size_t r = 0; r < rows; ++r) {
            for (std::size_t c = 0; c < cols; ++c) {
                const HexCell& center = grid[r][c];
                std::array<HexCell, 6> neighbors = find_neighbors(grid, r, c);
                result[r][c] = compute_advection_for_hex(center, neighbors, wind);
            }
        }

        return result;
    }

private:
    double eta_;
    double base_distance_m_;

    static double deg_to_rad(double deg) {
        return deg * M_PI / 180.0;
    }

    static double normalize_deg(double deg) {
        while (deg < 0.0) deg += 360.0;
        while (deg >= 360.0) deg -= 360.0;
        return deg;
    }

    /**
     * Map neighbor index to a direction unit vector in the same plane as flow_vx,flow_vy.
     *
     * We approximate the hex directions onto a Cartesian plane:
     *  0 (E)  : (1, 0)
     *  3 (W)  : (-1, 0)
     *  1 (NE) : (0.5,  sqrt(3)/2)
     *  2 (NW) : (-0.5, sqrt(3)/2)
     *  4 (SW) : (-0.5, -sqrt(3)/2)
     *  5 (SE) : (0.5,  -sqrt(3)/2)
     */
    static void neighbor_direction_vector(std::size_t dir, double& vx, double& vy) {
        const double s3 = std::sqrt(3.0) / 2.0;
        switch (dir) {
            case 0: vx = 1.0;   vy = 0.0;    break; // E
            case 1: vx = 0.5;   vy = s3;     break; // NE
            case 2: vx = -0.5;  vy = s3;     break; // NW
            case 3: vx = -1.0;  vy = 0.0;    break; // W
            case 4: vx = -0.5;  vy = -s3;    break; // SW
            case 5: vx = 0.5;   vy = -s3;    break; // SE
            default: vx = 0.0;  vy = 0.0;    break;
        }
    }

    /**
     * Directional kernel component k_dir capturing Phoenix’s daytime winds.
     *
     * We use a cosine-aligned kernel:
     *   k_dir = max(0, cos(phi))^p * (1 + a * wind_speed)
     *
     * where phi is the angle between flow vector and neighbor direction.
     * For westerly flow (from west to east), eastward neighbors (dir 0, 1, 5) get higher weights.
     * For easterly flow, westward neighbors (dir 3, 2, 4) get higher weights.
     *
     * The exponent p controls anisotropy; we choose p=2 for stronger directional preference.
     */
    static double directional_weight(std::size_t dir,
                                     double flow_vx,
                                     double flow_vy,
                                     double wind_speed) {
        double ndx = 0.0, ndy = 0.0;
        neighbor_direction_vector(dir, ndx, ndy);

        double flow_norm = std::sqrt(flow_vx * flow_vx + flow_vy * flow_vy);
        double nd_norm = std::sqrt(ndx * ndx + ndy * ndy);
        if (flow_norm <= std::numeric_limits<double>::epsilon() ||
            nd_norm <= std::numeric_limits<double>::epsilon()) {
            return 0.0;
        }

        double dot = flow_vx * ndx + flow_vy * ndy;
        double cos_phi = dot / (flow_norm * nd_norm);
        // Clamp numeric noise.
        if (cos_phi > 1.0) cos_phi = 1.0;
        if (cos_phi < -1.0) cos_phi = -1.0;

        double base = std::max(0.0, cos_phi); // only downwind and sideways
        double p = 2.0;                       // anisotropy exponent
        double a = 0.1;                       // scaling with speed (mild)
        return std::pow(base, p) * (1.0 + a * wind_speed);
    }

    /**
     * Distance kernel component k_dist.
     *
     * For immediate neighbors in a regular hex mesh, the center-to-center distance is constant.
     * We retain a Gaussian-like form to allow extension if the grid later stores explicit meters.
     *
     *   k_dist = exp(- (d / d0)^2 )
     *
     * where d0 = base_distance_m_ (Phoenix hex scale, e.g., 250–500 m).
     */
    double distance_weight(const HexCoord& a, const HexCoord& b) const {
        // For now, treat all neighbor distances as base_distance_m_, but allow
        // slight variation if axial coordinates imply diagonal separation.
        int dq = b.q - a.q;
        int dr = b.r - a.r;
        double d_axial = std::sqrt(static_cast<double>(dq * dq + dr * dr));
        double d_m = base_distance_m_ * d_axial;
        double ratio = d_m / base_distance_m_;
        return std::exp(-ratio * ratio);
    }

    /**
     * Extract the six axial neighbors from a 2D grid laid out using even-r horizontal layout.
     *
     * We assume grid[r][c].coord is consistent with this layout; in practice, users should
     * construct coordinates from indices using a fixed mapping.
     */
    static std::array<HexCell, 6> find_neighbors(
            const std::vector<std::vector<HexCell>>& grid,
            std::size_t r,
            std::size_t c) {
        std::size_t rows = grid.size();
        std::size_t cols = grid[0].size();

        auto invalid = []() {
            HexCell h;
            h.coord = {0, 0};
            h.uhi = 0.0;
            h.active = false;
            return h;
        };

        std::array<HexCell, 6> neigh;
        for (auto& n : neigh) {
            n = invalid();
        }

        // Even-r layout (pointy-top hexes):
        // For even r:
        //   neighbors: E=(r,c+1), W=(r,c-1),
        //              NE=(r-1,c), NW=(r-1,c-1), SE=(r+1,c), SW=(r+1,c-1)
        // For odd r:
        //   neighbors: E=(r,c+1), W=(r,c-1),
        //              NE=(r-1,c+1), NW=(r-1,c), SE=(r+1,c+1), SW=(r+1,c)
        bool even = (r % 2 == 0);

        auto get_cell = [&](int rr, int cc) -> HexCell {
            if (rr < 0 || cc < 0 || rr >= static_cast<int>(rows) || cc >= static_cast<int>(cols)) {
                return invalid();
            }
            return grid[static_cast<std::size_t>(rr)][static_cast<std::size_t>(cc)];
        };

        // E, W
        neigh[0] = get_cell(static_cast<int>(r), static_cast<int>(c) + 1);
        neigh[3] = get_cell(static_cast<int>(r), static_cast<int>(c) - 1);

        if (even) {
            neigh[1] = get_cell(static_cast<int>(r) - 1, static_cast<int>(c));     // NE
            neigh[2] = get_cell(static_cast<int>(r) - 1, static_cast<int>(c) - 1); // NW
            neigh[5] = get_cell(static_cast<int>(r) + 1, static_cast<int>(c));     // SE
            neigh[4] = get_cell(static_cast<int>(r) + 1, static_cast<int>(c) - 1); // SW
        } else {
            neigh[1] = get_cell(static_cast<int>(r) - 1, static_cast<int>(c) + 1); // NE
            neigh[2] = get_cell(static_cast<int>(r) - 1, static_cast<int>(c));     // NW
            neigh[5] = get_cell(static_cast<int>(r) + 1, static_cast<int>(c) + 1); // SE
            neigh[4] = get_cell(static_cast<int>(r) + 1, static_cast<int>(c));     // SW
        }

        return neigh;
    }
};

// Example wiring of the full offset formula including advection:
//   ΔT_h = α V_h + β B_h + γ W_h + eta * Σ_j w_hj (UHI_j - UHI_h) + δ
struct HexDrivers {
    double V; // vegetation index or similar
    double B; // built-up index
    double W; // water / albedo / other driver
};

double compute_delta_T_with_advection(const HexDrivers& d,
                                      double alpha,
                                      double beta,
                                      double gamma,
                                      double delta,
                                      double advection_term) {
    return alpha * d.V + beta * d.B + gamma * d.W + advection_term + delta;
}

// Minimal demo main (can be removed in library usage).
int main() {
    // Simple 3x3 grid demo focused on Phoenix summer daytime (westerly flow).
    std::vector<std::vector<HexCell>> grid(3, std::vector<HexCell>(3));
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            HexCell cell;
            cell.coord = {c, r}; // simple mapping; in real usage align with physical hex tiling
            cell.uhi = 5.0 + static_cast<double>(r + c); // arbitrary UHI pattern
            cell.active = true;
            grid[r][c] = cell;
        }
    }

    PhoenixAdvectionKernel kernel(/*eta=*/0.5, /*base_distance_m=*/300.0);

    // Phoenix daytime wind in warm season: wind from W to E (westerly, 270°), ~3 m/s.
    WindState wind;
    wind.direction_deg = 270.0;
    wind.speed = 3.0;

    auto adv_field = kernel.compute_advection_field(grid, wind);

    std::cout << "Advection field (Phoenix daytime westerly example):\n";
    for (const auto& row : adv_field) {
        for (double v : row) {
            std::cout << v << " ";
        }
        std::cout << "\n";
    }

    return 0;
}
