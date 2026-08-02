// File: cpp/eco_restoration/hex_shape_moments_invariants.cpp

#include <vector>
#include <cmath>
#include <string>
#include <iostream>

/**
 * 31. Hex-anchor invariants under affine transformations
 *
 * We treat each hex h as a continuous domain with an internal land-cover
 * pattern represented by indicator fields:
 *   V(x,y), I(x,y), W(x,y) ∈ [0,1]
 * over the hex interior Ω_h (vegetation, impervious, water).
 *
 * Raw moments (order up to 2) for a field F(x,y) (e.g., vegetation) are:
 *   m_pq(F) = ∬_{Ω_h} x^p y^q F(x,y) dx dy
 *
 * Central moments relative to the centroid (x̄_F, ȳ_F):
 *   μ_pq(F) = ∬_{Ω_h} (x - x̄_F)^p (y - ȳ_F)^q F(x,y) dx dy
 *
 * with:
 *   x̄_F = m_10(F) / m_00(F), ȳ_F = m_01(F) / m_00(F).
 *
 * For rotation and scaling invariance, we use normalized second-order
 * central moments analogous to Hu/Zernike-style invariants:
 *
 *   η_20(F) = μ_20(F) / m_00(F)^{2}
 *   η_02(F) = μ_02(F) / m_00(F)^{2}
 *   η_11(F) = μ_11(F) / m_00(F)^{2}
 *
 * Under uniform scaling by factor s and rigid rotation, these normalized
 * central moments remain invariant up to numerical tolerances for a given
 * land-cover pattern, because:
 *   - m_00(F) scales with area (∝ s^2).
 *   - μ_pq(F) scale with s^{p+q+2}.
 *   - The ratio μ_pq / m_00^{(p+q)/2+1} cancels the scale factor.[161][169]
 *
 * As a practical "anchor fingerprint" for cross-project alignment, we
 * compute a vector:
 *
 *   Φ_h = [
 *     η_20(V), η_02(V), η_11(V),
 *     η_20(I), η_02(I), η_11(I),
 *     η_20(W), η_02(W), η_11(W)
 *   ]
 *
 * which characterizes the shape of vegetation, impervious, and water
 * distributions inside hex h independent of grid orientation and scale.
 */

struct LandCoverPixel {
    double x;
    double y;
    double veg;   // V(x,y)
    double imperv;// I(x,y)
    double water; // W(x,y)
};

struct Moments {
    double m00;
    double m10;
    double m01;
    double m20;
    double m02;
    double m11;
};

Moments compute_raw_moments(const std::vector<LandCoverPixel>& pixels,
                            double LandCoverPixel::*field_member) {
    Moments M{0,0,0,0,0,0};
    for (const auto& p : pixels) {
        double f = p.*field_member;
        M.m00 += f;
        M.m10 += p.x * f;
        M.m01 += p.y * f;
        M.m20 += p.x * p.x * f;
        M.m02 += p.y * p.y * f;
        M.m11 += p.x * p.y * f;
    }
    return M;
}

struct NormalizedCentralMoments {
    double eta20;
    double eta02;
    double eta11;
};

NormalizedCentralMoments compute_normalized_central(const Moments& M) {
    if (M.m00 <= 0.0) {
        return {0.0, 0.0, 0.0};
    }
    double xbar = M.m10 / M.m00;
    double ybar = M.m01 / M.m00;

    // Central moments μ_pq.
    double mu20 = M.m20 - xbar * xbar * M.m00;
    double mu02 = M.m02 - ybar * ybar * M.m00;
    double mu11 = M.m11 - xbar * ybar * M.m00;

    double scale = M.m00 * M.m00; // m00^2 for p+q=2.
    return {
        mu20 / scale,
        mu02 / scale,
        mu11 / scale
    };
}

struct HexAnchorFingerprint {
    NormalizedCentralMoments veg;
    NormalizedCentralMoments imperv;
    NormalizedCentralMoments water;
};

HexAnchorFingerprint compute_hex_fingerprint(const std::vector<LandCoverPixel>& pixels) {
    Moments Mv = compute_raw_moments(pixels, &LandCoverPixel::veg);
    Moments Mi = compute_raw_moments(pixels, &LandCoverPixel::imperv);
    Moments Mw = compute_raw_moments(pixels, &LandCoverPixel::water);

    HexAnchorFingerprint fp;
    fp.veg    = compute_normalized_central(Mv);
    fp.imperv = compute_normalized_central(Mi);
    fp.water  = compute_normalized_central(Mw);
    return fp;
}

int main_invariants() {
    // Synthetic hex pixels (e.g., normalized x,y in canonical hex coordinates).
    std::vector<LandCoverPixel> pixels = {
        {0.1, 0.2, 1.0, 0.0, 0.0},
        {0.3, 0.4, 0.8, 0.2, 0.0},
        {0.6, 0.2, 0.3, 0.7, 0.0},
        {0.5, 0.7, 0.0, 0.5, 0.5}
    };

    HexAnchorFingerprint fp = compute_hex_fingerprint(pixels);

    std::cout << "Hex anchor fingerprint (invariant moments):\n";
    std::cout << " Vegetation: eta20=" << fp.veg.eta20
              << " eta02=" << fp.veg.eta02
              << " eta11=" << fp.veg.eta11 << "\n";
    std::cout << " Impervious: eta20=" << fp.imperv.eta20
              << " eta02=" << fp.imperv.eta02
              << " eta11=" << fp.imperv.eta11 << "\n";
    std::cout << " Water: eta20=" << fp.water.eta20
              << " eta02=" << fp.water.eta02
              << " eta11=" << fp.water.eta11 << "\n";

    return 0;
}
