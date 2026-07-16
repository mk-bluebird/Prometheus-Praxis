#ifndef PROMETHEUS_PRAXIS_LANE_TOPOLOGY_ENGINE_HPP
#define PROMETHEUS_PRAXIS_LANE_TOPOLOGY_ENGINE_HPP

#include <cstddef>
#include <cstdint>

extern "C" {

struct r_topology_sample {
    const char* particle_id;
    const char* lane;
    float       k;
    float       e;
    float       r;
    float       residual;
    std::int64_t timestamp_ms;
};

struct lane_event {
    const char* particle_id;
    const char* from_lane;
    const char* to_lane;
    const char* reason;
};

typedef void (*lane_event_callback)(const lane_event* ev, void* user_data);

/// Process a sliding window of r_topology samples and emit lane events.
/// Inputs:
///   samples      - pointer to contiguous array of samples
///   len          - number of samples
///   window_ms    - sliding window size in milliseconds
///   cb           - callback for each event
///   user_data    - opaque pointer passed to callback
///
/// Return:
///   0 on success, non-zero on error.
std::int32_t lane_topology_process_window(
    const r_topology_sample* samples,
    std::size_t len,
    std::int64_t window_ms,
    lane_event_callback cb,
    void* user_data
);

} // extern "C"

#endif
