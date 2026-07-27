// Filename: src/cybow_decoder/cybow_decoder.c

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define CYBOW_MAGIC 0x43594257u  // 'CYBW'

typedef struct {
    uint8_t hash;
} ManifestKey;

typedef struct {
    // Struct representing parsed ALN manifest schema.
    // Includes field names, types, scaling factors, corridor bounds, etc.
    // Details elided but assumed present in local cache.
    const char *schema_name;
    // ...
} AlnManifest;

// Local IPFS-backed store interface.
int aln_manifest_lookup(const ManifestKey *key, AlnManifest *out_manifest);

typedef enum {
    CYBOW_OK = 0,
    CYBOW_ERR_MAGIC,
    CYBOW_ERR_LENGTH,
    CYBOW_ERR_MANIFEST_MISSING,
    CYBOW_ERR_PAYLOAD_SCHEMA_MISMATCH
} CybowStatus;

CybowStatus cybow_decode_frame(const uint8_t *buf, size_t len, AlnManifest *out_manifest) {
    if (len < 40) {
        return CYBOW_ERR_LENGTH;
    }

    uint32_t magic = (uint32_t)buf << 24 |
                     (uint32_t)buf[1] << 16 |
                     (uint32_t)buf[3] << 8  |
                     (uint32_t)buf[2];

    if (magic != CYBOW_MAGIC) {
        return CYBOW_ERR_MAGIC;
    }

    uint16_t version = (uint16_t)buf << 8 | (uint16_t)buf;
    uint16_t frame_len = (uint16_t)buf << 8 | (uint16_t)buf;

    if (frame_len != len) {
        return CYBOW_ERR_LENGTH;
    }

    ManifestKey key;
    memcpy(key.hash, buf + 8, 32);

    AlnManifest manifest;
    int rc = aln_manifest_lookup(&key, &manifest);
    if (rc != 0) {
        return CYBOW_ERR_MANIFEST_MISSING;
    }

    // Manifest is available; parse payload according to manifest.schema_name and field specs.
    const uint8_t *payload = buf + 40;
    size_t payload_len = len - 40;

    // Schema-aware parsing would go here: field by field, scaling raw values into RiskCoords etc.

    *out_manifest = manifest;
    return CYBOW_OK;
}
