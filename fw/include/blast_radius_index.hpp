// Filename: fw/include/blast_radius_index.hpp

#pragma once
#include <cstdint>

// Single downstream reach entry for a vault breach.
struct BlastRadiusEntry {
    uint32_t reach_id_hash;      // hashed reach ID for compactness
    float    distance_m;         // along-channel distance [m]
    float    delay_seconds;      // estimated travel time [s]
    float    peak_surcharge_m;   // peak ΔH at this reach [m]
    float    peak_r_surcharge;   // normalized rsurcharge ∈ [0,1]
    float    blast_intensity;    // scalar B = ΔH·Q·rsurcharge or similar
};

// Vault-specific index (fixed-size slice into a global array).
struct VaultBlastIndex {
    uint32_t vault_id_hash;      // hashed vault_nodeid
    uint16_t offset;             // index into global array
    uint16_t count;              // number of entries
};

// Global tables compiled into firmware or loaded from flash.
extern const VaultBlastIndex g_vault_indices[];
extern const BlastRadiusEntry g_blast_entries[];
extern const uint16_t g_num_vault_indices;

// API for FOG router to get affected reaches for a vault.
const VaultBlastIndex* find_vault_index(uint32_t vault_id_hash);
const BlastRadiusEntry* get_vault_entries(const VaultBlastIndex* idx);
