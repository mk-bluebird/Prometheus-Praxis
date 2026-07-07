// Filename: fw/src/blast_radius_index.cpp

#include "blast_radius_index.hpp"

const VaultBlastIndex* find_vault_index(uint32_t vault_id_hash) {
    // Simple linear search; can be upgraded to binary search if sorted.
    for (uint16_t i = 0; i < g_num_vault_indices; ++i) {
        if (g_vault_indices[i].vault_id_hash == vault_id_hash) {
            return &g_vault_indices[i];
        }
    }
    return nullptr;
}

const BlastRadiusEntry* get_vault_entries(const VaultBlastIndex* idx) {
    if (!idx) {
        return nullptr;
    }
    return &g_blast_entries[idx->offset];
}
