// File: cpp/tools/hex_anchor_naming_scheme.cpp

#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <iostream>

/**
 * Mathematically decodable hex-anchor naming scheme.
 *
 * Goal:
 *  - Unique, reproducible identifier per hex anchor.
 *  - Encodes spatial location, resolution, and calibration version.
 *  - Reversible: decode back to centroid coordinates and parameters.
 *  - Robust to coordinate system changes: we avoid raw lat/lon hashes
 *    tied to a single CRS and instead use a canonical, grid-based index.
 *
 * Design:
 *  - Use a canonical axial hex grid (q,r) defined in a fixed CRS (e.g.,
 *    EPSG:4326 projected to a Phoenix-specific hex lattice upstream).
 *  - Store:
 *      * q, r: integer hex indices (grid-based location).
 *      * res_m: hex “diameter” or apothem in meters (resolution class).
 *      * ver: calibration version integer.
 *      * alpha, beta, gamma (scaled integers).
 *
 *  - Naming scheme (string):
 *      HEX::<q>::<r>::RES<res_m>::VER<ver>::A<a_scaled>::B<b_scaled>::G<g_scaled>
 *
 *  - Decoding recovers q, r, res_m, ver, α, β, γ directly.
 *
 * This avoids disallowed cryptographic hashes; we use a structured,
 * human-readable encoding that is stable across coordinate systems as
 * long as the hex grid indices (q,r) are defined from a canonical map
 * projection shared by all tools.
 */

struct HexAnchorParams {
    int q;
    int r;
    int res_m;         // resolution in meters (hex diameter or edge-to-edge)
    int version;       // calibration version integer
    double alpha;      // cooling coefficient for vegetation
    double beta;       // coefficient for built/roof
    double gamma;      // coefficient for water
};

std::string encode_hex_anchor_id(const HexAnchorParams& p, int scale_coeff = 1000) {
    int a_scaled = static_cast<int>(std::round(p.alpha * scale_coeff));
    int b_scaled = static_cast<int>(std::round(p.beta  * scale_coeff));
    int g_scaled = static_cast<int>(std::round(p.gamma * scale_coeff));

    std::ostringstream oss;
    oss << "HEX::" << p.q << "::" << p.r
        << "::RES" << p.res_m
        << "::VER" << p.version
        << "::A" << a_scaled
        << "::B" << b_scaled
        << "::G" << g_scaled;
    return oss.str();
}

bool decode_hex_anchor_id(const std::string& id, HexAnchorParams& out, int scale_coeff = 1000) {
    // Expected format: HEX::<q>::<r>::RES<res_m>::VER<ver>::A<a_scaled>::B<b_scaled>::G<g_scaled>
    if (id.rfind("HEX::", 0) != 0) {
        return false;
    }
    std::vector<std::string> tokens;
    std::string tmp;
    std::istringstream iss(id);
    while (std::getline(iss, tmp, ':')) {
        if (!tmp.empty()) tokens.push_back(tmp);
    }
    // After splitting on ':', tokens should be:
    // [ "HEX", "", "<q>", "", "<r>", "RES<res>", "VER<ver>", "A<a>", "B<b>", "G<g>" ]
    if (tokens.size() < 9) {
        return false;
    }

    auto parse_int = [](const std::string& s) -> int {
        return std::stoi(s);
    };

    try {
        out.q = parse_int(tokens[2]);
        out.r = parse_int(tokens[4]);

        auto parse_prefixed_int = [](const std::string& s, const std::string& prefix) -> int {
            if (s.rfind(prefix, 0) != 0) {
                throw std::runtime_error("Prefix mismatch");
            }
            return std::stoi(s.substr(prefix.size()));
        };

        out.res_m = parse_prefixed_int(tokens[5], "RES");
        out.version = parse_prefixed_int(tokens[6], "VER");
        int a_scaled = parse_prefixed_int(tokens[7], "A");
        int b_scaled = parse_prefixed_int(tokens[8], "B");
        int g_scaled = parse_prefixed_int(tokens[9], "G");

        out.alpha = static_cast<double>(a_scaled) / static_cast<double>(scale_coeff);
        out.beta  = static_cast<double>(b_scaled) / static_cast<double>(scale_coeff);
        out.gamma = static_cast<double>(g_scaled) / static_cast<double>(scale_coeff);
    } catch (...) {
        return false;
    }

    return true;
}

int main() {
    HexAnchorParams params;
    params.q = 10;
    params.r = 20;
    params.res_m = 300;
    params.version = 3;
    params.alpha = -8.123;
    params.beta  = 3.456;
    params.gamma = -5.789;

    std::string id = encode_hex_anchor_id(params);
    std::cout << "Encoded hex-anchor ID: " << id << "\n";

    HexAnchorParams decoded;
    if (decode_hex_anchor_id(id, decoded)) {
        std::cout << "Decoded params:\n"
                  << "  q=" << decoded.q
                  << " r=" << decoded.r
                  << " res_m=" << decoded.res_m
                  << " ver=" << decoded.version
                  << " alpha=" << decoded.alpha
                  << " beta=" << decoded.beta
                  << " gamma=" << decoded.gamma << "\n";
    } else {
        std::cout << "Failed to decode ID.\n";
    }

    return 0;
}
