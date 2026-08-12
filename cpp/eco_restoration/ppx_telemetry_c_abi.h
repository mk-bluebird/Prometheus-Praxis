// File: cpp/eco_restoration/ppx_telemetry_c_abi.h
#ifndef PPX_TELEMETRY_C_ABI_H
#define PPX_TELEMETRY_C_ABI_H

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
#define PPX_ALIGN8 alignas(8)
extern "C" {
#else
#define PPX_ALIGN8 _Alignas(8)
#endif

enum {
    PPX_TELEMETRY_ABI_VERSION = 1,
    PPX_TELEMETRY_MACHINE_ID_BYTES = 64,
    PPX_TELEMETRY_STATION_ID_BYTES = 64,
    PPX_TELEMETRY_TIMESTAMP_BYTES = 32
};

typedef struct PPX_ALIGN8 TelemetryC {
    uint32_t abi_version;                         /* offset 0 */
    uint32_t byte_size;                           /* offset 4 */
    uint64_t sequence;                            /* offset 8 */
    int64_t timestamp_unix_ns;                    /* offset 16 */
    uint32_t telemetry_domain;                    /* offset 24 */
    uint32_t presence_flags;                      /* offset 28 */
    char machine_id[PPX_TELEMETRY_MACHINE_ID_BYTES]; /* offset 32 */
    char station_id[PPX_TELEMETRY_STATION_ID_BYTES]; /* offset 96 */
    char timestamp_utc[PPX_TELEMETRY_TIMESTAMP_BYTES]; /* offset 160 */
    double r_hydraulics;                          /* offset 192 */
    double r_energy;                              /* offset 200 */
    double r_uncertainty;                         /* offset 208 */
    double r_reliability;                         /* offset 216 */
    double r_extra_1;                             /* offset 224 */
    double r_extra_2;                             /* offset 232 */
    double roh;                                   /* offset 240 */
    double vt_current;                            /* offset 248 */
    double vt_next;                               /* offset 256 */
} TelemetryC;

int ppx_telemetry_c_valid(const TelemetryC* telemetry);

#if defined(__cplusplus)
}
#include <cstddef>
#include <type_traits>

static_assert(std::is_standard_layout_v<TelemetryC>);
static_assert(std::is_trivially_copyable_v<TelemetryC>);
static_assert(alignof(TelemetryC) == 8);
static_assert(sizeof(TelemetryC) == 264);
static_assert(offsetof(TelemetryC, r_hydraulics) == 192);
static_assert(offsetof(TelemetryC, vt_next) == 256);
#endif

#endif
