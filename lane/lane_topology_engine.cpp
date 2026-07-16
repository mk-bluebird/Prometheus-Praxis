#include "lane_topology_engine.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace {

struct SampleView {
    const r_topology_sample* s;
};

bool lane_ge(const std::string& a, const std::string& b) {
    static const std::vector<std::string> order = {"RESEARCH", "PILOT", "PROD"};
    auto idx = [&](const std::string& x) -> int {
        for (std::size_t i = 0; i < order.size(); ++i) {
            if (order[i] == x) return static_cast<int>(i);
        }
        return -1;
    };
    int ia = idx(a);
    int ib = idx(b);
    if (ia < 0 || ib < 0) return 0;
    return ia >= ib;
}

std::string next_lane(const std::string& lane) {
    if (lane == "RESEARCH") return "PILOT";
    if (lane == "PILOT") return "PROD";
    return lane;
}

std::string prev_lane(const std::string& lane) {
    if (lane == "PROD") return "PILOT";
    if (lane == "PILOT") return "RESEARCH";
    return lane;
}

} // namespace

extern "C" std::int32_t lane_topology_process_window(
    const r_topology_sample* samples,
    std::size_t len,
    std::int64_t window_ms,
    lane_event_callback cb,
    void* user_data
) {
    if (!samples || len == 0 || !cb) {
        return 1;
    }

    // Simple sliding window: group by particle_id, compute metrics,
    // and emit promotion/downgrade events according to
    // LaneGovernanceTopology2026v1-like rules.
    std::vector<SampleView> views;
    views.reserve(len);
    for (std::size_t i = 0; i < len; ++i) {
        views.push_back(SampleView{&samples[i]});
    }

    // Sort by particle_id and timestamp.
    std::sort(views.begin(), views.end(), [](const SampleView& a, const SampleView& b) {
        int cmp = std::string(a.s->particle_id).compare(b.s->particle_id);
        if (cmp != 0) return cmp < 0;
        return a.s->timestamp_ms < b.s->timestamp_ms;
    });

    std::size_t i = 0;
    while (i < views.size()) {
        const char* pid = views[i].s->particle_id;
        std::string particle_id = pid ? pid : "";
        std::vector<const r_topology_sample*> window_samples;

        // Collect consecutive samples for this particle.
        std::size_t j = i;
        while (j < views.size() &&
               std::string(views[j].s->particle_id) == particle_id) {
            window_samples.push_back(views[j].s);
            ++j;
        }

        // For simplicity, use the last sample as current state.
        if (!window_samples.empty()) {
            const r_topology_sample* last = window_samples.back();
            std::string lane = last->lane ? last->lane : "";

            float sum_k = 0.0f;
            float sum_r = 0.0f;
            float max_residual = 0.0f;
            std::size_t count = 0;

            std::int64_t latest_ts = last->timestamp_ms;
            std::int64_t window_start = latest_ts - window_ms;

            for (auto s : window_samples) {
                if (s->timestamp_ms >= window_start) {
                    sum_k += s->k;
                    sum_r += s->r;
                    if (s->residual > max_residual) {
                        max_residual = s->residual;
                    }
                    ++count;
                }
            }

            if (count > 0) {
                float avg_k = sum_k / static_cast<float>(count);
                float avg_r = sum_r / static_cast<float>(count);

                // Example rules:
                // - Promote if avg_k >= 0.8 and avg_r <= 0.3 and residual is small.
                // - Downgrade if avg_r >= 0.6 or residual exceeds corridor.
                if (avg_k >= 0.8f && avg_r <= 0.3f && max_residual <= 0.2f) {
                    std::string to_lane = next_lane(lane);
                    if (!to_lane.empty() && to_lane != lane) {
                        lane_event ev{
                            last->particle_id,
                            last->lane,
                            to_lane.c_str(),
                            "promotion"
                        };
                        cb(&ev, user_data);
                    }
                } else if (avg_r >= 0.6f || max_residual >= 0.5f) {
                    std::string to_lane = prev_lane(lane);
                    if (!to_lane.empty() && to_lane != lane) {
                        lane_event ev{
                            last->particle_id,
                            last->lane,
                            to_lane.c_str(),
                            "downgrade"
                        };
                        cb(&ev, user_data);
                    }
                }
            }
        }

        i = j;
    }

    return 0;
}
