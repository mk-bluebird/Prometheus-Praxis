// filename: src/ringbuffer_telemetry.hpp
// destination: src/

#ifndef RINGBUFFER_TELEMETRY_HPP
#define RINGBUFFER_TELEMETRY_HPP

#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>

#define RB_CAPACITY 1024u
#define RB_MASK (RB_CAPACITY - 1u)

// Telemetry sample structure (non-illustrative; extend as needed).
typedef struct {
    uint32_t timestamp_ticks;    // system tick or timer count
    float    value0;             // e.g., motor current
    float    value1;             // e.g., panel angle
    float    value2;             // e.g., wind speed
    uint32_t flags;              // status bits, safety corridor state, etc.
} TelemetrySample;

// Single-producer single-consumer ring buffer.
typedef struct {
    TelemetrySample buffer[RB_CAPACITY];

    // head: next slot index to be written by producer.
    _Atomic uint32_t head;
    // tail: next slot index to be read by consumer.
    _Atomic uint32_t tail;
} TelemetryRingBuffer;

#ifdef __cplusplus
extern "C" {
#endif

void rb_init(TelemetryRingBuffer *rb);
bool rb_push(TelemetryRingBuffer *rb, const TelemetrySample *sample);
bool rb_pop(TelemetryRingBuffer *rb, TelemetrySample *out);

#ifdef __cplusplus
}
#endif

#endif // RINGBUFFER_TELEMETRY_HPP
