// filename: src/cybow_decoder/cybow_frame_decode.c
// target-repo: https://github.com/mk-bluebird/Prometheus-Praxis
// language: C (flat POD, ARM-friendly, non-actuating)
// license: MIT OR Apache-2.0

#include <stddef.h>
#include <stdint.h>

// CYBOW frame layout (conceptual, binary on the wire):
// [0..3]   magic      (u32)
// [4..5]   version    (u16)
// [6..7]   flags      (u16)
// [8..15]  frame_id   (u64)
// [16..47] manifest_hash (32 bytes, e.g. IPFS / content-addressed digest)
// [48..51] payload_len (u32, bytes)
// [52..]   payload    (schema-dependent, interpreted via ALN manifest)
//
// This decoder:
// - Validates minimal header.
// - Extracts manifest_hash (32 bytes).
// - Returns typed views over header and payload without heap allocation.
// - Does NOT perform IPFS network calls or ALN evaluation; it only
//   prepares inputs for higher layers.[file:3]

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint64_t frame_id;
    uint8_t  manifest_hash[32];
    uint32_t payload_len;
    // Pointer into the original buffer for payload bytes.
    const uint8_t* payload;
} CybowFrameHeader;

// Simple error codes for decoder.[file:3]
enum {
    CYBOW_DECODE_OK              = 0,
    CYBOW_DECODE_ERR_NULL        = 1,
    CYBOW_DECODE_ERR_SIZE        = 2,
    CYBOW_DECODE_ERR_MAGIC       = 3,
    CYBOW_DECODE_ERR_PAYLOAD_LEN = 4
};

// Optional: canonical magic for CYBOW frames (big endian constant).
static const uint32_t CYBOW_MAGIC = 0x43594257u; // 'CYBW' as example tag.

// Decode a CYBOW frame from a flat byte buffer.
// - in_buf: pointer to raw bytes.
// - in_size: number of bytes available.
// - out: header view to fill (must be non-NULL).
// Returns CYBOW_DECODE_OK on success, error code otherwise.
//
// Non-actuating: does no IO, allocates no heap, and does not interpret
// payload content beyond providing a pointer and length.[file:3]
int cybow_frame_decode(const uint8_t* in_buf,
                       size_t in_size,
                       CybowFrameHeader* out)
{
    if (in_buf == NULL || out == NULL) {
        return CYBOW_DECODE_ERR_NULL;
    }

    // Minimal header size before payload.
    const size_t MIN_HEADER = 52u;
    if (in_size < MIN_HEADER) {
        return CYBOW_DECODE_ERR_SIZE;
    }

    // Decode basic scalar fields using little-endian layout.
    // Adjust endianness if your wire format differs.[file:3]
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

    // Manifest hash: 32 raw bytes, treated as opaque content-addressed digest.
    for (size_t i = 0; i < 32u; ++i) {
        out->manifest_hash[i] = in_buf[offset + i];
    }
    offset += 32;

    uint32_t payload_len =
        ((uint32_t)in_buf[offset + 0])        |
        ((uint32_t)in_buf[offset + 1] << 8)  |
        ((uint32_t)in_buf[offset + 2] << 16) |
        ((uint32_t)in_buf[offset + 3] << 24);
    offset += 4;

    // Basic sanity checks.[file:3]
    if (magic != CYBOW_MAGIC) {
        return CYBOW_DECODE_ERR_MAGIC;
    }
    if ((size_t)payload_len > in_size - offset) {
        return CYBOW_DECODE_ERR_PAYLOAD_LEN;
    }

    // Populate header view.
    out->magic       = magic;
    out->version     = version;
    out->flags       = flags;
    out->frame_id    = frame_id;
    out->payload_len = payload_len;
    out->payload     = &in_buf[offset];

    return CYBOW_DECODE_OK;
}

// Schema-aware ALN orchestration hook (interface only):
//
// Given a decoded frame header, higher layers can:
//
// - Interpret manifest_hash as a content-addressed key (e.g., IPFS CID bytes
//   or an internal hash).[file:3]
// - Look up an ALN manifest that defines:
//   - The schema for payload bytes (fields, types, KER axes).
//   - Validation rules (corridors, Lyapunov residual semantics).
//   - DID / governance bindings.
//
// This decoder does not perform that lookup; it only exposes manifest_hash
// and payload as opaque data for those ALN / IPFS handlers.
typedef struct {
    const CybowFrameHeader* header;
    // Optional future fields for decoded ALN manifest metadata,
    // left opaque here to keep the decoder non-actuating.[file:3]
} CybowManifestContext;

// Stub function showing how a caller would connect the decoder
// to ALN manifests. Implementation belongs in a higher-level
// governance / schema module, not in this low-level decoder.[file:3]
void cybow_manifest_route(const CybowFrameHeader* header,
                          CybowManifestContext* ctx)
{
    if (ctx == NULL || header == NULL) {
        return;
    }
    ctx->header = header;
    // A real implementation would:
    // - Map header->manifest_hash to a manifest ID.
    // - Load or reference an ALN v2 manifest that describes payload schema.
    // - Set up schema-aware decoding of header->payload bytes.
    // This file deliberately keeps that logic out to remain
    // non-harmful and transport-only.
}
