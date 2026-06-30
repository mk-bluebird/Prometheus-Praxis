// filename: src/ringbuffer_telemetry.c
// destination: src/

#include "ringbuffer_telemetry.hpp"

void rb_init(TelemetryRingBuffer *rb) {
    atomic_store_explicit(&rb->head, 0u, memory_order_relaxed);
    atomic_store_explicit(&rb->tail, 0u, memory_order_relaxed);
}

bool rb_push(TelemetryRingBuffer *rb, const TelemetrySample *sample) {
    uint32_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);
    uint32_t tail = atomic_load_explicit(&rb->tail, memory_order_acquire);

    uint32_t next_head = (head + 1u) & RB_MASK;

    // Full if advancing head would collide with tail.
    if (next_head == tail) {
        return false; // buffer full, sample dropped or counted as loss
    }

    // Write sample data into buffer slot.
    rb->buffer[head].timestamp_ticks = sample->timestamp_ticks;
    rb->buffer[head].value0          = sample->value0;
    rb->buffer[head].value1          = sample->value1;
    rb->buffer[head].value2          = sample->value2;
    rb->buffer[head].flags           = sample->flags;

    // Publish new head with release ordering.
    atomic_store_explicit(&rb->head, next_head, memory_order_release);
    return true;
}

bool rb_pop(TelemetryRingBuffer *rb, TelemetrySample *out) {
    uint32_t tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&rb->head, memory_order_acquire);

    if (head == tail) {
        return false; // buffer empty
    }

    // Read sample data from buffer slot.
    out->timestamp_ticks = rb->buffer[tail].timestamp_ticks;
    out->value0          = rb->buffer[tail].value0;
    out->value1          = rb->buffer[tail].value1;
    out->value2          = rb->buffer[tail].value2;
    out->flags           = rb->buffer[tail].flags;

    uint32_t next_tail = (tail + 1u) & RB_MASK;

    // Publish new tail with release ordering.
    atomic_store_explicit(&rb->tail, next_tail, memory_order_release);
    return true;
}
