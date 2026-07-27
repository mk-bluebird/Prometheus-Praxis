// filename: src/aln_kernel/ker_hex_kriging.cpp
// destination: Prometheus-Praxis/src/aln_kernel/ker_hex_kriging.cpp
// license: MIT OR Apache-2.0
// language: C++ (C17, non-actuating)

#include <cstddef>
#include <cstdint>
#include <cmath>

// Hex-anchored KER score triad and scalar s = k + e - r, all in [0,1].[file:3]
struct KerScore {
    float k;   // knowledge factor
    float e;   // eco-impact factor
    float r;   // residual risk

    // Compute scalar KER s = k + e - r.[file:3]
    float scalar() const {
        float s = k + e - r;
        // We do not clamp here; callers enforce corridor bounds.[file:3]
        return s;
    }
};

// 2D hex coordinate in projected metres.[file:3]
struct HexCoord {
    float x;
    float y;
};

// Neighbour sample: position plus KER triad.[file:3]
struct HexKerSample {
    HexCoord coord;
    KerScore ker;
};

// Isotropic exponential variogram for scalar KER.[file:3]
struct VariogramModel {
    float nugget;
    float sill;
    float range;

    float gamma(float h) const {
        if (h <= 0.0f) {
            return 0.0f;
        }
        float r = (range <= 0.0f) ? 1.0f : range;
        return nugget + sill * (1.0f - std::exp(-h / r));
    }
};

// Result for a single kriging interpolation.[file:3]
struct KrigingResult {
    float interpolated_scalar;
    KerScore interpolated_ker;
    // Weights are returned for diagnostics and hex-stamping.[file:3]
    float weights[64];
    std::size_t weight_count;
};

// Euclidean distance between two hex coordinates.[file:3]
static float ker_hex_distance(const HexCoord& a, const HexCoord& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Simple Gaussian elimination with partial pivoting for a dense system
// of size n x n stored row-major in a, solving A x = b.
// Returns false on singular matrix; outputs solution in x.[file:3]
static bool ker_hex_solve_linear(float* a, float* b, float* x, std::size_t n) {
    const float eps = 1e-6f;
    // Forward elimination.[file:3]
    for (std::size_t k = 0; k < n; ++k) {
        // Pivot.[file:3]
        std::size_t max_row = k;
        float max_val = std::fabs(a[k * n + k]);
        for (std::size_t i = k + 1; i < n; ++i) {
            float val = std::fabs(a[i * n + k]);
            if (val > max_val) {
                max_val = val;
                max_row = i;
            }
        }
        if (max_val < eps) {
            return false;
        }
        if (max_row != k) {
            for (std::size_t j = 0; j < n; ++j) {
                float tmp = a[k * n + j];
                a[k * n + j] = a[max_row * n + j];
                a[max_row * n + j] = tmp;
            }
            float tmp_b = b[k];
            b[k] = b[max_row];
            b[max_row] = tmp_b;
        }
        // Normalize pivot row.[file:3]
        float pivot = a[k * n + k];
        for (std::size_t j = k; j < n; ++j) {
            a[k * n + j] /= pivot;
        }
        b[k] /= pivot;
        // Eliminate.[file:3]
        for (std::size_t i = 0; i < n; ++i) {
            if (i == k) {
                continue;
            }
            float factor = a[i * n + k];
            if (factor == 0.0f) {
                continue;
            }
            for (std::size_t j = k; j < n; ++j) {
                a[i * n + j] -= factor * a[k * n + j];
            }
            b[i] -= factor * b[k];
        }
    }
    // Back-substitution: matrix is now near-identity.[file:3]
    for (std::size_t i = 0; i < n; ++i) {
        x[i] = b[i];
    }
    return true;
}

// Euclidean projection onto the probability simplex:
// lambda_i >= 0, sum_i lambda_i = 1.[file:3]
static void ker_hex_project_to_simplex(float* lambda, std::size_t n) {
    // Copy and sort descending.[file:3]
    float u[64];
    std::size_t idx[64];
    for (std::size_t i = 0; i < n; ++i) {
        u[i] = lambda[i];
        idx[i] = i;
    }
    // Simple insertion sort for small n.[file:3]
    for (std::size_t i = 1; i < n; ++i) {
        float key = u[i];
        std::size_t key_idx = idx[i];
        std::size_t j = i;
        while (j > 0 && u[j - 1] < key) {
            u[j] = u[j - 1];
            idx[j] = idx[j - 1];
            --j;
        }
        u[j] = key;
        idx[j] = key_idx;
    }

    float sum = 0.0f;
    float theta = 0.0f;
    std::size_t rho = 0;
    for (std::size_t i = 0; i < n; ++i) {
        sum += u[i];
        float t = sum - 1.0f;
        float denom = static_cast<float>(i + 1);
        float val = u[i] - t / denom;
        if (val > 0.0f) {
            rho = i + 1;
            theta = t / denom;
        }
    }

    // Project.[file:3]
    float total = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        float v = lambda[i] - theta;
        if (v < 0.0f) {
            v = 0.0f;
        }
        lambda[i] = v;
        total += v;
    }
    if (total <= 0.0f) {
        // Fall back to uniform simplex if all entries collapsed.[file:3]
        float uniform = 1.0f / static_cast<float>(n);
        for (std::size_t i = 0; i < n; ++i) {
            lambda[i] = uniform;
        }
    } else {
        // Normalize to sum 1.[file:3]
        for (std::size_t i = 0; i < n; ++i) {
            lambda[i] /= total;
        }
    }
}

// Constrained ordinary kriging over scalar KER s = k + e - r.
//
// - neighbours: array of HexKerSample, each with positive scalar s in (0,1].
// - target: hex coordinate for interpolation.
// - model: variogram parameters.
// - out: KrigingResult.
//
// Invariants:
// - Weights lambda_i >= 0 and sum_i lambda_i = 1 (simplex).
// - If all neighbour scalars s_i > 0, then interpolated scalar s_0 > 0 (convex combination).
// - Interpolated triad (k_0, e_0, r_0) is reconstructed so that s_0 ≈ k_0 + e_0 - r_0.[file:3]
extern "C" void ker_hex_kriging_run(const HexKerSample* neighbours,
                                    std::size_t neighbour_count,
                                    HexCoord target,
                                    VariogramModel model,
                                    KrigingResult* out)
{
    if (!neighbours || !out || neighbour_count == 0U || neighbour_count > 64U) {
        return;
    }

    // Degenerate case: single neighbour, copy its KER.[file:3]
    if (neighbour_count == 1U) {
        const HexKerSample& s = neighbours[0];
        float w = 1.0f;
        out->weights[0] = w;
        out->weight_count = 1U;
        out->interpolated_scalar = s.ker.scalar();
        out->interpolated_ker = s.ker;
        return;
    }

    const std::size_t n = neighbour_count;
    const std::size_t size = n + 1U; // ordinary kriging with one Lagrange multiplier.[file:3]

    float A[65 * 65];
    float b[65];
    float x[65];

    // Build kriging matrix A and RHS b for scalar KER.[file:3]
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            float h = ker_hex_distance(neighbours[i].coord, neighbours[j].coord);
            A[i * size + j] = model.gamma(h);
        }
        A[i * size + n] = 1.0f;    // constraint row.[file:3]
        A[n * size + i] = 1.0f;    // constraint column.[file:3]

        float h0 = ker_hex_distance(neighbours[i].coord, target);
        b[i] = model.gamma(h0);
    }
    A[n * size + n] = 0.0f;
    b[n] = 1.0f; // sum_i lambda_i = 1.[file:3]

    // Solve for unconstrained weights and Lagrange multiplier.[file:3]
    if (!ker_hex_solve_linear(A, b, x, size)) {
        return;
    }

    // Extract lambda and project into simplex.[file:3]
    float lambda[64];
    for (std::size_t i = 0; i < n; ++i) {
        lambda[i] = x[i];
    }
    ker_hex_project_to_simplex(lambda, n);

    // Interpolate scalar s and local means for k and e.[file:3]
    float s_interp = 0.0f;
    float k_mean = 0.0f;
    float e_mean = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        const KerScore& ks = neighbours[i].ker;
        float wi = lambda[i];
        float si = ks.scalar();
        s_interp += wi * si;
        k_mean   += wi * ks.k;
        e_mean   += wi * ks.e;
    }

    // Reconstruct triad from scalar and local means: r = k + e - s.[file:3]
    KerScore k_interp{};
    k_interp.k = k_mean;
    k_interp.e = e_mean;
    float r_val = k_mean + e_mean - s_interp;
    if (r_val < 0.0f) {
        r_val = 0.0f;
    } else if (r_val > 1.0f) {
        r_val = 1.0f;
    }
    k_interp.r = r_val;

    // Populate result.[file:3]
    out->interpolated_scalar = s_interp;
    out->interpolated_ker = k_interp;
    out->weight_count = n;
    for (std::size_t i = 0; i < n; ++i) {
        out->weights[i] = lambda[i];
    }
}
