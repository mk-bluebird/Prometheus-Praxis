// File: cpp/eco_restoration/risk_vector_merkle.cpp

#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>

// Canonical risk planes for ecosafety_core_v2.
// Fixed order for serialization and hashing.
enum class RiskPlane : std::size_t {
    ENERGY = 0,
    HYDRAULICS = 1,
    PFAS = 2,
    COLD = 3,
    BOD = 4,
    TSS = 5,
    CEC = 6,
    CARBON = 7,
    BIODIVERSITY = 8,
    MATERIALS = 9,
    NEURORIGHTS = 10,
    TOPOLOGY = 11,
    DATAQUALITY = 12,
    UNCERTAINTY = 13
};

constexpr std::size_t RISK_PLANE_COUNT = 14;

// Non-offsettable planes: CARBON, BIODIVERSITY, NEURORIGHTS.
constexpr uint16_t NONOFFSET_MASK =
    (1u << static_cast<std::size_t>(RiskPlane::CARBON)) |
    (1u << static_cast<std::size_t>(RiskPlane::BIODIVERSITY)) |
    (1u << static_cast<std::size_t>(RiskPlane::NEURORIGHTS));

// Compact risk vector representation.
struct RiskVector {
    std::array<float, RISK_PLANE_COUNT> r{};

    void set(RiskPlane plane, float value) {
        if (value < 0.0f || value > 1.0f) {
            throw std::invalid_argument("RiskVector: value must be in [0,1]");
        }
        r[static_cast<std::size_t>(plane)] = value;
    }

    float get(RiskPlane plane) const {
        return r[static_cast<std::size_t>(plane)];
    }
};

// Simple hash function for demonstration.
// In a real deployment, replace this with an allowed, vetted primitive
// consistent with Eco-Fort hashing policy (e.g. a safe SHA-2 variant).
// Here we use a trivial 64-bit FNV-1a-like hash over bytes for illustration.
std::array<uint8_t, 32> hash_bytes(const std::vector<uint8_t>& data) {
    uint64_t h = 1469598103934665603ull; // FNV offset basis
    for (uint8_t b : data) {
        h ^= static_cast<uint64_t>(b);
        h *= 1099511628211ull; // FNV prime
    }
    // Expand 64-bit h into 32 bytes deterministically.
    std::array<uint8_t, 32> out{};
    for (std::size_t i = 0; i < 32; ++i) {
        out[i] = static_cast<uint8_t>((h >> ((i % 8) * 8)) & 0xFF);
    }
    return out;
}

// Merkle tree utilities.
struct MerkleNode {
    std::array<uint8_t, 32> hash{};
};

struct MerklePathEntry {
    std::array<uint8_t, 32> sibling_hash{};
    bool is_left; // true if current node is left child, sibling is right.
};

struct MerklePath {
    std::vector<MerklePathEntry> entries;
};

// Serialize RiskVector into compact binary format:
// [u8 version][u16 plane_count][u16 nonoffset_mask][array<f32> r_plane]
std::vector<uint8_t> serialize_risk_vector(const RiskVector& rv,
                                           uint8_t version = 1) {
    std::vector<uint8_t> buf;
    buf.reserve(1 + 2 + 2 + RISK_PLANE_COUNT * 4);

    // Version
    buf.push_back(version);

    // plane_count (u16 little-endian)
    uint16_t plane_count = static_cast<uint16_t>(RISK_PLANE_COUNT);
    buf.push_back(static_cast<uint8_t>(plane_count & 0xFF));
    buf.push_back(static_cast<uint8_t>((plane_count >> 8) & 0xFF));

    // nonoffset_mask (u16 little-endian)
    buf.push_back(static_cast<uint8_t>(NONOFFSET_MASK & 0xFF));
    buf.push_back(static_cast<uint8_t>((NONOFFSET_MASK >> 8) & 0xFF));

    // Risk values as f32 little-endian
    for (std::size_t i = 0; i < RISK_PLANE_COUNT; ++i) {
        float v = rv.r[i];
        uint32_t bits;
        static_assert(sizeof(float) == sizeof(uint32_t),
                      "Unexpected float size");
        std::memcpy(&bits, &v, sizeof(float));
        buf.push_back(static_cast<uint8_t>(bits & 0xFF));
        buf.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
        buf.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
    }

    return buf;
}

// Compute leaf hash of a RiskVector.
std::array<uint8_t, 32> risk_vector_leaf_hash(const RiskVector& rv) {
    std::vector<uint8_t> bytes = serialize_risk_vector(rv);
    return hash_bytes(bytes);
}

// Build Merkle tree over leaf hashes and return root hash.
// Leaves: hashes[i] for i=0..N-1.
// Internal nodes: hash of concatenated children hashes.
std::array<uint8_t, 32> build_merkle_root(const std::vector<std::array<uint8_t, 32>>& leaves) {
    if (leaves.empty()) {
        // Hash of empty vector, arbitrary choice.
        std::vector<uint8_t> empty;
        return hash_bytes(empty);
    }

    std::vector<std::array<uint8_t, 32>> level = leaves;

    while (level.size() > 1) {
        std::vector<std::array<uint8_t, 32>> next_level;
        for (std::size_t i = 0; i < level.size(); i += 2) {
            if (i + 1 < level.size()) {
                // Concatenate two child hashes.
                std::vector<uint8_t> concat;
                concat.reserve(64);
                concat.insert(concat.end(), level[i].begin(), level[i].end());
                concat.insert(concat.end(), level[i + 1].begin(), level[i + 1].end());
                next_level.push_back(hash_bytes(concat));
            } else {
                // Odd leaf; promote to next level directly.
                next_level.push_back(level[i]);
            }
        }
        level = next_level;
    }

    return level.front();
}

// Compute Merkle path for leaf at index idx.
MerklePath compute_merkle_path(const std::vector<std::array<uint8_t, 32>>& leaves,
                               std::size_t idx) {
    MerklePath path;
    if (leaves.empty() || idx >= leaves.size()) {
        return path;
    }

    std::vector<std::array<uint8_t, 32>> level = leaves;
    std::size_t index = idx;

    while (level.size() > 1) {
        std::vector<std::array<uint8_t, 32>> next_level;
        for (std::size_t i = 0; i < level.size(); i += 2) {
            if (i + 1 < level.size()) {
                // Pair (i, i+1)
                std::vector<uint8_t> concat;
                concat.reserve(64);
                concat.insert(concat.end(), level[i].begin(), level[i].end());
                concat.insert(concat.end(), level[i + 1].begin(), level[i + 1].end());
                std::array<uint8_t, 32> parent_hash = hash_bytes(concat);
                next_level.push_back(parent_hash);

                // If current index is in this pair, record sibling.
                if (index == i || index == i + 1) {
                    MerklePathEntry entry;
                    if (index == i) {
                        entry.is_left = true;
                        entry.sibling_hash = level[i + 1];
                    } else {
                        entry.is_left = false;
                        entry.sibling_hash = level[i];
                    }
                    path.entries.push_back(entry);

                    // Update index at next level.
                    index = next_level.size() - 1;
                }
            } else {
                // Single node promoted.
                next_level.push_back(level[i]);
                if (index == i) {
                    // No sibling at this level.
                    // Index becomes last in next level.
                    index = next_level.size() - 1;
                }
            }
        }
        level = next_level;
    }

    return path;
}

// Verify Merkle path given leaf hash and root hash.
bool verify_merkle_path(const std::array<uint8_t, 32>& leaf_hash,
                        const MerklePath& path,
                        const std::array<uint8_t, 32>& root_hash) {
    std::array<uint8_t, 32> current = leaf_hash;

    for (const auto& entry : path.entries) {
        std::vector<uint8_t> concat;
        concat.reserve(64);
        if (entry.is_left) {
            concat.insert(concat.end(), current.begin(), current.end());
            concat.insert(concat.end(), entry.sibling_hash.begin(), entry.sibling_hash.end());
        } else {
            concat.insert(concat.end(), entry.sibling_hash.begin(), entry.sibling_hash.end());
            concat.insert(concat.end(), current.begin(), current.end());
        }
        current = hash_bytes(concat);
    }

    return current == root_hash;
}

// Utility to print hash as hex string.
std::string hash_to_hex(const std::array<uint8_t, 32>& h) {
    static const char* hex_digits = "0123456789abcdef";
    std::string s;
    s.reserve(64);
    for (uint8_t b : h) {
        s.push_back(hex_digits[(b >> 4) & 0xF]);
        s.push_back(hex_digits[b & 0xF]);
    }
    return s;
}

// Demonstration main: build a few RiskVectors, compute Merkle root and path.
int main() {
    // Build sample risk vectors.
    RiskVector rv1, rv2, rv3;
    rv1.set(RiskPlane::PFAS, 0.3f);
    rv1.set(RiskPlane::BOD, 0.2f);
    rv1.set(RiskPlane::CARBON, 0.1f);

    rv2.set(RiskPlane::PFAS, 0.5f);
    rv2.set(RiskPlane::TSS, 0.4f);
    rv2.set(RiskPlane::CARBON, 0.2f);

    rv3.set(RiskPlane::PFAS, 0.1f);
    rv3.set(RiskPlane::CEC, 0.3f);
    rv3.set(RiskPlane::CARBON, 0.05f);

    // Compute leaf hashes.
    std::vector<std::array<uint8_t, 32>> leaves;
    leaves.push_back(risk_vector_leaf_hash(rv1));
    leaves.push_back(risk_vector_leaf_hash(rv2));
    leaves.push_back(risk_vector_leaf_hash(rv3));

    // Build Merkle root.
    std::array<uint8_t, 32> root = build_merkle_root(leaves);
    std::cout << "Merkle root: " << hash_to_hex(root) << "\n";

    // Compute Merkle path for leaf 1 (rv2).
    std::size_t idx = 1;
    MerklePath path = compute_merkle_path(leaves, idx);

    // Verify path.
    bool ok = verify_merkle_path(leaves[idx], path, root);
    std::cout << "Merkle path verification for leaf " << idx << ": "
              << (ok ? "OK" : "FAIL") << "\n";

    return ok ? 0 : 1;
}
