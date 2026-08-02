// File: cpp/tools/hex_anchor_geometric_index.cpp

#include <cstdint>
#include <cmath>
#include <iostream>

/**
 * 50. Hex-anchor geometric hash as a multi-scale index.
 *
 * Concept:
 *  - We define a hierarchical indexing scheme for Phoenix hexes that:
 *      * Encodes location and resolution in a single 64-bit integer.
 *      * Allows derivation of lower-resolution parent hex IDs via bitwise truncation.
 *      * Is mathematically decodable to obtain hex center and resolution
 *        without a lookup table, similar in spirit to H3.[204][208][211][217][214]
 *
 * Construction (UTM-based):
 *  - Let (E, N) be UTM easting/northing in meters for the hex center.
 *  - Choose a base precision Δ0 (e.g., 10 m) and define integer coordinates:
 *
 *        e0 = floor(E / Δ0),  n0 = floor(N / Δ0)
 *
 *  - Resolution level R (0..15) defines hex scale: cell size ΔR = Δ0 ⋅ 2^R.
 *  - We encode index as:
 *
 *        idx = (R << 60) | (eR << 30) | (nR)
 *
 *    where:
 *      eR = floor(E / ΔR), nR = floor(N / ΔR),
 *      R uses the top 4 bits, eR and nR use 30 bits each (Phoenix fits in these ranges).
 *
 * Parent derivation:
 *  - Parent at coarser resolution Rp = R - k has:
 *
 *        ep = floor(eR / 2^k)
 *        np = floor(nR / 2^k)
 *
 *    so parent index:
 *
 *        idx_p = (Rp << 60) | (ep << 30) | np
 *
 *  - This can be implemented via bit shifts and masks.
 *
 * Mathematical decoding:
 *  - Given idx, recover:
 *
 *        R  = idx >> 60
 *        eR = (idx >> 30) & ((1ULL << 30) - 1)
 *        nR =  idx        & ((1ULL << 30) - 1)
 *
 *  - Then hex center in UTM:
 *
 *        ΔR = Δ0 ⋅ 2^R
 *        E_c = (eR + 0.5) ⋅ ΔR
 *        N_c = (nR + 0.5) ⋅ ΔR
 *
 *  - Hex boundary can be reconstructed from E_c, N_c and ΔR using standard
 *    hex geometry; no lookup table is required.
 */

struct HexIndex {
    uint64_t idx;
};

constexpr double BASE_RES_M = 10.0;

HexIndex encode_hex_index(double utm_e, double utm_n, uint8_t R) {
    double delta_R = BASE_RES_M * std::pow(2.0, R);
    uint64_t eR = static_cast<uint64_t>(std::floor(utm_e / delta_R));
    uint64_t nR = static_cast<uint64_t>(std::floor(utm_n / delta_R));

    uint64_t idx = (static_cast<uint64_t>(R) << 60)
                 | ( (eR & ((1ULL << 30) - 1)) << 30 )
                 | ( nR & ((1ULL << 30) - 1) );
    return {idx};
}

void decode_hex_index(const HexIndex& h, double& utm_e_center,
                      double& utm_n_center, uint8_t& R_out) {
    uint64_t idx = h.idx;
    uint8_t R = static_cast<uint8_t>(idx >> 60);
    uint64_t eR = (idx >> 30) & ((1ULL << 30) - 1);
    uint64_t nR = idx & ((1ULL << 30) - 1);

    double delta_R = BASE_RES_M * std::pow(2.0, R);
    utm_e_center = (static_cast<double>(eR) + 0.5) * delta_R;
    utm_n_center = (static_cast<double>(nR) + 0.5) * delta_R;
    R_out = R;
}

HexIndex hex_parent(const HexIndex& child, uint8_t levels_up) {
    uint64_t idx = child.idx;
    uint8_t R_child = static_cast<uint8_t>(idx >> 60);
    if (levels_up > R_child) levels_up = R_child;
    uint8_t R_parent = static_cast<uint8_t>(R_child - levels_up);

    uint64_t eR_child = (idx >> 30) & ((1ULL << 30) - 1);
    uint64_t nR_child = idx & ((1ULL << 30) - 1);

    uint64_t factor = 1ULL << levels_up;
    uint64_t eR_parent = eR_child / factor;
    uint64_t nR_parent = nR_child / factor;

    uint64_t idx_p = (static_cast<uint64_t>(R_parent) << 60)
                   | ( (eR_parent & ((1ULL << 30) - 1)) << 30 )
                   | ( nR_parent & ((1ULL << 30) - 1) );
    return {idx_p};
}

int run_hex_index_demo() {
    double utm_e = 410000.0;
    double utm_n = 3725000.0;
    uint8_t R = 6;

    HexIndex h = encode_hex_index(utm_e, utm_n, R);

    double e_c, n_c;
    uint8_t R_decoded;
    decode_hex_index(h, e_c, n_c, R_decoded);

    std::cout << "Encoded idx=" << h.idx << "\n";
    std::cout << "Decoded center: E=" << e_c << " N=" << n_c
              << " R=" << static_cast<int>(R_decoded) << "\n";

    HexIndex parent = hex_parent(h, 2);
    decode_hex_index(parent, e_c, n_c, R_decoded);
    std::cout << "Parent center:  E=" << e_c << " N=" << n_c
              << " R=" << static_cast<int>(R_decoded) << "\n";

    return 0;
}
