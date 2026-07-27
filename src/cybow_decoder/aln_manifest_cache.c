// filename: src/cybow_decoder/aln_manifest_cache.c
// target-repo: https://github.com/mk-bluebird/Prometheus-Praxis
// language: C (flat POD, non-actuating, local cache only)
// license: MIT OR Apache-2.0

#include <stddef.h>
#include <stdint.h>

// ALN manifest cache entry:
// - Keyed by a 32-byte manifest_hash (as carried in CYBOW frames).
// - Stores a small metadata view: version tag, lane (RESEARCH/PILOT/PROD),
//   and an opaque pointer for higher-level schema objects (ALN v2 manifests,
//   IPFS blocks, etc.).[file:3]

typedef enum {
    ALN_LANE_RESEARCH = 0,
    ALN_LANE_PILOT    = 1,
    ALN_LANE_PROD     = 2
} aln_lane_t;

typedef struct {
    uint8_t  manifest_hash[32];
    // Simple versioning for CYBOW frame schemas, e.g. "drainagedecay.v2026.07".
    char     version_tag[64];
    aln_lane_t lane;
    // Opaque pointer to a decoded manifest structure or external handle.
    // This cache itself does not interpret the manifest; it only stores a handle.
    void*    manifest_handle;
    // Pin flag: pinned PROD manifests must not be evicted.[file:3]
    uint8_t  pinned;
} aln_manifest_entry;

// Simple fixed-size cache for embedded use.
#define ALN_MANIFEST_CACHE_CAPACITY 32

static aln_manifest_entry g_manifest_cache[ALN_MANIFEST_CACHE_CAPACITY];

// Utility: compare two 32-byte hashes.
static int hash_equals(const uint8_t* a, const uint8_t* b) {
    for (size_t i = 0; i < 32u; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

// Utility: zero-initialize an entry.
static void entry_clear(aln_manifest_entry* e) {
    if (!e) return;
    for (size_t i = 0; i < 32u; ++i) {
        e->manifest_hash[i] = 0;
    }
    for (size_t i = 0; i < sizeof(e->version_tag); ++i) {
        e->version_tag[i] = '\0';
    }
    e->lane = ALN_LANE_RESEARCH;
    e->manifest_handle = NULL;
    e->pinned = 0;
}

// Initialize the manifest cache; should be called once at startup.[file:3]
void aln_manifest_cache_init(void) {
    for (size_t i = 0; i < ALN_MANIFEST_CACHE_CAPACITY; ++i) {
        entry_clear(&g_manifest_cache[i]);
    }
}

// Look up a manifest by 32-byte hash.
// - manifest_hash: key.
// - out_entry: optional pointer to an entry view; may be NULL.
// Returns 1 if found, 0 if not.[file:3]
int aln_manifest_lookup(const uint8_t manifest_hash[32],
                        const aln_manifest_entry** out_entry)
{
    if (manifest_hash == NULL) {
        return 0;
    }
    for (size_t i = 0; i < ALN_MANIFEST_CACHE_CAPACITY; ++i) {
        aln_manifest_entry* e = &g_manifest_cache[i];
        // Empty slot heuristic: all-zero hash means unused.
        if (e->manifest_hash[0] == 0 &&
            e->manifest_hash[31] == 0 &&
            e->manifest_handle == NULL) {
            continue;
        }
        if (hash_equals(e->manifest_hash, manifest_hash)) {
            if (out_entry != NULL) {
                *out_entry = e;
            }
            return 1;
        }
    }
    return 0;
}

// Insert or update a manifest cache entry.
// - manifest_hash: key (32 bytes).
// - version_tag: null-terminated string, truncated to 63 chars.
// - lane: ALN lane (RESEARCH/PILOT/PROD).
// - manifest_handle: opaque pointer to decoded manifest / IPFS block.
// - pin: 1 to pin, 0 otherwise.
//
// If an entry with the same hash exists, it is updated.
// Otherwise, a free slot is used. Unpinned entries may be overwritten
// if the cache is full.[file:3]
int aln_manifest_cache_put(const uint8_t manifest_hash[32],
                           const char* version_tag,
                           aln_lane_t lane,
                           void* manifest_handle,
                           uint8_t pin)
{
    if (manifest_hash == NULL || version_tag == NULL) {
        return 0;
    }

    // First, try to update existing entry.
    for (size_t i = 0; i < ALN_MANIFEST_CACHE_CAPACITY; ++i) {
        aln_manifest_entry* e = &g_manifest_cache[i];
        if (hash_equals(e->manifest_hash, manifest_hash)) {
            // Update metadata, preserving pin if already pinned and pin == 0.
            for (size_t j = 0; j < 32u; ++j) {
                e->manifest_hash[j] = manifest_hash[j];
            }
            // Copy version_tag with truncation.
            size_t idx = 0;
            while (version_tag[idx] != '\0' &&
                   idx + 1 < sizeof(e->version_tag)) {
                e->version_tag[idx] = version_tag[idx];
                ++idx;
            }
            e->version_tag[idx] = '\0';
            e->lane = lane;
            e->manifest_handle = manifest_handle;
            if (pin) {
                e->pinned = 1;
            }
            return 1;
        }
    }

    // Find free slot or least-recent unpinned slot (here we just use first unpinned).[file:3]
    aln_manifest_entry* target = NULL;
    for (size_t i = 0; i < ALN_MANIFEST_CACHE_CAPACITY; ++i) {
        aln_manifest_entry* e = &g_manifest_cache[i];
        // Treat all-zero hash and NULL handle as free.
        if ((e->manifest_hash[0] == 0 &&
             e->manifest_hash[31] == 0 &&
             e->manifest_handle == NULL) ||
            (!e->pinned)) {
            target = e;
            break;
        }
    }
    if (target == NULL) {
        // Cache is full of pinned entries; cannot insert.[file:3]
        return 0;
    }

    // Fill in target slot.
    for (size_t j = 0; j < 32u; ++j) {
        target->manifest_hash[j] = manifest_hash[j];
    }
    size_t idx = 0;
    while (version_tag[idx] != '\0' &&
           idx + 1 < sizeof(target->version_tag)) {
        target->version_tag[idx] = version_tag[idx];
        ++idx;
    }
    target->version_tag[idx] = '\0';
    target->lane = lane;
    target->manifest_handle = manifest_handle;
    target->pinned = pin ? 1u : 0u;

    return 1;
}

// Pin a manifest entry (e.g. when promoting an ALN schema to PROD).
// Returns 1 on success, 0 if not found.[file:3]
int aln_manifest_pin(const uint8_t manifest_hash[32]) {
    const aln_manifest_entry* e = NULL;
    if (!aln_manifest_lookup(manifest_hash, &e)) {
        return 0;
    }
    // Cast away const for internal mutation.
    ((aln_manifest_entry*)e)->pinned = 1u;
    return 1;
}

// Simple helper to query lane and version for a manifest.
// Returns 1 on success, 0 if not found; outputs lane and version_tag
// (version_tag_out truncated to max_len, null-terminated).[file:3]
int aln_manifest_metadata(const uint8_t manifest_hash[32],
                          aln_lane_t* lane_out,
                          char* version_tag_out,
                          size_t max_len)
{
    const aln_manifest_entry* e = NULL;
    if (!aln_manifest_lookup(manifest_hash, &e)) {
        return 0;
    }
    if (lane_out) {
        *lane_out = e->lane;
    }
    if (version_tag_out && max_len > 0) {
        size_t i = 0;
        while (e->version_tag[i] != '\0' && i + 1 < max_len) {
            version_tag_out[i] = e->version_tag[i];
            ++i;
        }
        version_tag_out[i] = '\0';
    }
    return 1;
}

// NOTE: IPFS / content-addressed storage integration.
//
// This file deliberately avoids direct IPFS or network calls to remain
// non-actuating and portable. A higher-level module can:
//
// - Interpret the 32-byte manifest_hash as a CID or internal key.
// - Fetch or decode the manifest from local IPFS or other storage.
// - Pass a manifest_handle (e.g. pointer to decoded ALN v2 manifest)
//   into aln_manifest_cache_put().
//
// This keeps the cache focused on local versioning and PROD pinning,
// consistent with Prometheus-Praxis governance constraints.[file:3]
