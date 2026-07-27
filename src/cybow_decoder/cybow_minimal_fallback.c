// filename: src/cybow_decoder/cybow_minimal_fallback.c
// target-repo: https://github.com/mk-bluebird/Prometheus-Praxis
// language: C (non-actuating, invariant-preserving)
// license: MIT OR Apache-2.0

#include <stddef.h>
#include <stdint.h>

// CybowMinimal2026v1: minimal schema view used when an ALN manifest
// is missing, unreadable, or fails validation.
// This schema is deliberately small and non-actuating: it extracts
// only transport-safe fields and logs frames as RESEARCH-lane only.[file:3]

typedef struct {
    // Raw header fields (copied from cybow_frame_decode.[file:3])
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint64_t frame_id;
    uint8_t  manifest_hash[32];

    // Minimal payload view: length and pointer into raw buffer.
    uint32_t payload_len;
    const uint8_t* payload;

    // Lane is forced to RESEARCH for all fallback frames.
    // 0 = RESEARCH, 1 = PILOT, 2 = PROD (but fallback uses 0 only).[file:3]
    uint8_t lane;
} CybowMinimalFrame;

// Error codes focused on fallback conditions.
enum {
    CYBOW_MINIMAL_OK                 = 0,
    CYBOW_MINIMAL_ERR_NULL           = 1,
    CYBOW_MINIMAL_ERR_SIZE           = 2,
    CYBOW_MINIMAL_ERR_MAGIC          = 3,
    CYBOW_MINIMAL_ERR_PAYLOAD_LEN    = 4
};

// Must match cybow_frame_decode.c for wire compatibility.[file:3]
static const uint32_t CYBOW_MAGIC = 0x43594257u; // 'CYBW'

// Minimal invariant: we do not trust any external manifest.
// Instead, we reuse the low-level decoder logic and wrap the result
// as a CybowMinimalFrame with RESEARCH lane enforced.[file:3]
int cybow_minimal_decode(const uint8_t* in_buf,
                         size_t in_size,
                         CybowMinimalFrame* out)
{
    if (in_buf == NULL || out == NULL) {
        return CYBOW_MINIMAL_ERR_NULL;
    }

    const size_t MIN_HEADER = 52u;
    if (in_size < MIN_HEADER) {
        return CYBOW_MINIMAL_ERR_SIZE;
    }

    size_t offset = 0;

    uint32_t magic =
        ((uint32_t)in_buf[offset + 0]) |
        ((uint32_t)in_buf[offset + 1] << 8) |
        ((uint32_t)in_buf[offset + 2] << 16) |
        ((uint32_t)in_buf[offset + 3] << 24);
    offset += 4;

    uint16_t version =
        (uint16_t)in_buf[offset + 0] |
        (uint16_t)(in_buf[offset + 1] << 8);
    offset += 2;

    uint16_t flags =
        (uint16_t)in_buf[offset + 0] |
        (uint16_t)(in_buf[offset + 1] << 8);
    offset += 2;

    uint64_t frame_id =
        ((uint64_t)in_buf[offset + 0])        |
        ((uint64_t)in_buf[offset + 1] << 8)  |
        ((uint64_t)in_buf[offset + 2] << 16) |
        ((uint64_t)in_buf[offset + 3] << 24) |
        ((uint64_t)in_buf[offset + 4] << 32) |
        ((uint64_t)in_buf[offset + 5] << 40) |
        ((uint64_t)in_buf[offset + 6] << 48) |
        ((uint64_t)in_buf[offset + 7] << 56);
    offset += 8;

    uint8_t manifest_hash[32];
    for (size_t i = 0; i < 32u; ++i) {
        manifest_hash[i] = in_buf[offset + i];
    }
    offset += 32;

    uint32_t payload_len =
        ((uint32_t)in_buf[offset + 0])        |
        ((uint32_t)in_buf[offset + 1] << 8)  |
        ((uint32_t)in_buf[offset + 2] << 16) |
        ((uint32_t)in_buf[offset + 3] << 24);
    offset += 4;

    if (magic != CYBOW_MAGIC) {
        return CYBOW_MINIMAL_ERR_MAGIC;
    }
    if ((size_t)payload_len > in_size - offset) {
        return CYBOW_MINIMAL_ERR_PAYLOAD_LEN;
    }

    // Populate minimal frame view.
    out->magic       = magic;
    out->version     = version;
    out->flags       = flags;
    out->frame_id    = frame_id;
    for (size_t i = 0; i < 32u; ++i) {
        out->manifest_hash[i] = manifest_hash[i];
    }
    out->payload_len = payload_len;
    out->payload     = &in_buf[offset];

    // Fallback invariant: lane is always RESEARCH.[file:3]
    out->lane        = 0u;

    return CYBOW_MINIMAL_OK;
}

// Fallback policy function:
//
// Given a decoded CybowMinimalFrame and ALN/manifest status,
// decide whether to treat the frame as a safe RESEARCH-only record.
// This function is intentionally simple and non-actuating: it does
// not apply KER thresholds or Lyapunov gates, it only enforces that
// missing/corrupted manifests cannot yield PROD/PILOT lanes.[file:3]
typedef enum {
    CYBOW_FALLBACK_ACCEPT_RESEARCH = 0,
    CYBOW_FALLBACK_REJECT          = 1
} cybow_fallback_decision;

// Caller passes information about manifest/ALN state.
// In a real stack, these flags would come from aln_manifest_cache,
// ALN v2 validation, and IPFS/local storage status.[file:3]
typedef struct {
    uint8_t manifest_found;     // 0 = missing, 1 = found.
    uint8_t manifest_valid;     // 0 = corrupted/invalid, 1 = valid.
    uint8_t governance_ok;      // 0 = governance failure, 1 = ok.
} CybowFallbackContext;

// Decide fallback handling:
// - If manifest is missing or invalid, frame is accepted only as
//   RESEARCH-lane diagnostic (non-actuating).
// - If governance_ok is false, frame is rejected, even in RESEARCH,
//   to preserve DID/KER invariants.
// This keeps unsafe or ungoverned frames out of PROD/PILOT lanes
// by construction.[file:3]
cybow_fallback_decision cybow_minimal_fallback(const CybowMinimalFrame* frame,
                                               const CybowFallbackContext* ctx)
{
    if (frame == NULL || ctx == NULL) {
        return CYBOW_FALLBACK_REJECT;
    }

    if (!ctx->governance_ok) {
        // Governance failure: reject the frame outright.[file:3]
        return CYBOW_FALLBACK_REJECT;
    }

    if (!ctx->manifest_found || !ctx->manifest_valid) {
        // Manifest missing/corrupted: force RESEARCH lane only.
        // Note: we do not change frame->lane here to keep this
        // function side-effect-free; callers should persist lane=RESEARCH.
        return CYBOW_FALLBACK_ACCEPT_RESEARCH;
    }

    // If manifest exists and is valid, higher-level code should
    // use full ALN schema decoding instead of this fallback.[file:3]
    return CYBOW_FALLBACK_REJECT;
}
