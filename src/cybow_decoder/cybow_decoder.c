// Filename: src/cybow_decoder/cybow_decoder.c

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define CYBOW_MAGIC 0x43594257u  /* 'CYBW' */

typedef struct {
    uint8_t hash[32];
} ManifestKey;

typedef struct {
    const char *schema_name;
    /* additional schema metadata can be added here */
} AlnManifest;

typedef enum {
    CYBOW_OK = 0,
    CYBOW_ERR_MAGIC,
    CYBOW_ERR_LENGTH,
    CYBOW_ERR_MANIFEST_MISSING,
    CYBOW_ERR_PAYLOAD_SCHEMA_MISMATCH
} CybowStatus;

/* Local IPFS-backed store interface. */
int aln_manifest_lookup(const ManifestKey *key, AlnManifest *out_manifest);

/* Decode a CYBOW frame using its ALN manifest hash. */
CybowStatus cybow_decode_frame(const uint8_t *buf, size_t len, AlnManifest *out_manifest) {
    if (buf == NULL || out_manifest == NULL) {
        return CYBOW_ERR_LENGTH;
    }

    if (len < 40) {
        return CYBOW_ERR_LENGTH;
    }

    uint32_t magic = (uint32_t)buf[0] << 24 |
                     (uint32_t)buf[1] << 16 |
                     (uint32_t)buf[2] << 8  |
                     (uint32_t)buf[3];

    if (magic != CYBOW_MAGIC) {
        return CYBOW_ERR_MAGIC;
    }

    uint16_t version = (uint16_t)buf[4] << 8 | (uint16_t)buf[5];
    uint16_t frame_len = (uint16_t)buf[6] << 8 | (uint16_t)buf[7];

    (void)version; /* version is parsed but not used yet */

    if (frame_len != len) {
        return CYBOW_ERR_LENGTH;
    }

    ManifestKey key;
    memcpy(key.hash, buf + 8, sizeof(key.hash));

    AlnManifest manifest;
    int rc = aln_manifest_lookup(&key, &manifest);
    if (rc != 0) {
        return CYBOW_ERR_MANIFEST_MISSING;
    }

    const uint8_t *payload = buf + 40;
    size_t payload_len = len - 40;

    (void)payload;
    (void)payload_len;

    *out_manifest = manifest;
    return CYBOW_OK;
}

/* Decode a CYBOW frame and fall back to a minimal schema when the manifest is unavailable. */
CybowStatus cybow_decode_with_fallback(const uint8_t *buf,
                                       size_t len,
                                       AlnManifest *out_manifest,
                                       int *used_fallback) {
    if (used_fallback == NULL) {
        return CYBOW_ERR_LENGTH;
    }

    CybowStatus st = cybow_decode_frame(buf, len, out_manifest);
    if (st == CYBOW_OK) {
        *used_fallback = 0;
        return CYBOW_OK;
    }

    if (st == CYBOW_ERR_MANIFEST_MISSING || st == CYBOW_ERR_PAYLOAD_SCHEMA_MISMATCH) {
        static const AlnManifest fallback_manifest = {
            .schema_name = "CybowMinimal2026v1"
        };
        *out_manifest = fallback_manifest;
        *used_fallback = 1;
        return CYBOW_OK;
    }

    return st;
}
