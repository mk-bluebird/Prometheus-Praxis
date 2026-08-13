// File: cpp/eco_restoration/geospatial_smt_deadline_verifier.hpp
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <limits>
#include <stdexcept>
#include <vector>

namespace eco_restoration {

/*
Bit-vector-compatible fixed-point geometry:
- Coordinates are signed two's-complement 64-bit integers in one declared scale.
- Input conversion must use one declared rounding rule, preferably
  round-to-nearest with ties-to-even, before entering this module.
- Products and orientation determinants use signed 128-bit intermediates,
  equivalent to sign-extending fixed-width operands before multiplication.
- No floating-point operations are used after coordinate quantization.

For polygon A and declared cell polygon R:
SAT iff there exists a fixed-point point in A intersect R.
UNSAT iff A and R are disjoint under the same closed-boundary convention.
Boundary contact is SAT because it establishes a nonempty intersection.
*/
struct FixedPointVertex {
    std::int64_t x{};
    std::int64_t y{};
};

using FixedPointPolygon = std::vector<FixedPointVertex>;

enum class SmtResult {
    Sat,
    Unsat
};

inline __int128 orientation(const FixedPointVertex& a,
                            const FixedPointVertex& b,
                            const FixedPointVertex& c) {
    return static_cast<__int128>(b.x - a.x) * static_cast<__int128>(c.y - a.y) -
           static_cast<__int128>(b.y - a.y) * static_cast<__int128>(c.x - a.x);
}

inline bool on_closed_segment(const FixedPointVertex& a,
                              const FixedPointVertex& b,
                              const FixedPointVertex& point) {
    return orientation(a, b, point) == 0 &&
           point.x >= std::min(a.x, b.x) && point.x <= std::max(a.x, b.x) &&
           point.y >= std::min(a.y, b.y) && point.y <= std::max(a.y, b.y);
}

inline bool closed_segments_intersect(const FixedPointVertex& a,
                                      const FixedPointVertex& b,
                                      const FixedPointVertex& c,
                                      const FixedPointVertex& d) {
    const __int128 o1 = orientation(a, b, c);
    const __int128 o2 = orientation(a, b, d);
    const __int128 o3 = orientation(c, d, a);
    const __int128 o4 = orientation(c, d, b);

    if (o1 == 0 && on_closed_segment(a, b, c)) return true;
    if (o2 == 0 && on_closed_segment(a, b, d)) return true;
    if (o3 == 0 && on_closed_segment(c, d, a)) return true;
    if (o4 == 0 && on_closed_segment(c, d, b)) return true;

    const bool ab_straddles = (o1 < 0 && o2 > 0) || (o1 > 0 && o2 < 0);
    const bool cd_straddles = (o3 < 0 && o4 > 0) || (o3 > 0 && o4 < 0);
    return ab_straddles && cd_straddles;
}

inline bool point_in_closed_convex_polygon(const FixedPointVertex& point,
                                           const FixedPointPolygon& polygon) {
    if (polygon.size() < 3) throw std::invalid_argument("polygon needs at least three vertices");

    int sign = 0;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const __int128 value = orientation(
            polygon[i], polygon[(i + 1) % polygon.size()], point);
        if (value == 0) continue;
        const int current = value > 0 ? 1 : -1;
        if (sign != 0 && sign != current) return false;
        sign = current;
    }
    return true;
}

inline SmtResult fixed_point_polygon_cell_intersection(
    const FixedPointPolygon& polygon, const FixedPointPolygon& cell_polygon) {
    if (polygon.size() < 3 || cell_polygon.size() < 3) {
        throw std::invalid_argument("both polygons need at least three vertices");
    }

    for (const auto& vertex : polygon) {
        if (point_in_closed_convex_polygon(vertex, cell_polygon)) return SmtResult::Sat;
    }
    for (const auto& vertex : cell_polygon) {
        if (point_in_closed_convex_polygon(vertex, polygon)) return SmtResult::Sat;
    }
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const auto& a = polygon[i];
        const auto& b = polygon[(i + 1) % polygon.size()];
        for (std::size_t j = 0; j < cell_polygon.size(); ++j) {
            const auto& c = cell_polygon[j];
            const auto& d = cell_polygon[(j + 1) % cell_polygon.size()];
            if (closed_segments_intersect(a, b, c, d)) return SmtResult::Sat;
        }
    }
    return SmtResult::Unsat;
}

/*
Finite-state consensus deadline model:
state=(phase, clock_ticks, retry_count, queue_depth, delivered_count).
Bounds for bounded verification:
0<=clock_ticks<=clock_limit,
0<=retry_count<=retry_limit,
0<=queue_depth<=queue_limit.

The bounded liveness property is checked over every reachable state:
nonterminal(state) implies successors(state) is nonempty.
The transition relation must include a deadline action that either delivers,
retries within retry_limit, or reaches an explicit terminal failure state.
*/
enum class ConsensusPhase {
    Waiting,
    Dispatching,
    Retrying,
    Delivered,
    Expired
};

struct ConsensusState {
    ConsensusPhase phase{ConsensusPhase::Waiting};
    std::uint16_t clock_ticks{};
    std::uint16_t retry_count{};
    std::uint16_t queue_depth{};
    std::uint16_t delivered_count{};

    bool operator==(const ConsensusState& other) const noexcept {
        return phase == other.phase && clock_ticks == other.clock_ticks &&
               retry_count == other.retry_count && queue_depth == other.queue_depth &&
               delivered_count == other.delivered_count;
    }
};

struct ConsensusBounds {
    std::uint16_t clock_limit{};
    std::uint16_t retry_limit{};
    std::uint16_t queue_limit{};
};

inline bool terminal(const ConsensusState& state) {
    return state.phase == ConsensusPhase::Delivered ||
           state.phase == ConsensusPhase::Expired;
}

inline std::vector<ConsensusState> consensus_successors(
    const ConsensusState& state, const ConsensusBounds& bounds) {
    if (state.clock_ticks > bounds.clock_limit || state.retry_count > bounds.retry_limit ||
        state.queue_depth > bounds.queue_limit) {
        throw std::invalid_argument("state violates declared verification bounds");
    }
    if (terminal(state)) return {};

    std::vector<ConsensusState> next;
    if (state.phase == ConsensusPhase::Waiting && state.queue_depth > 0) {
        next.push_back({ConsensusPhase::Dispatching, state.clock_ticks,
                        state.retry_count, static_cast<std::uint16_t>(state.queue_depth - 1),
                        state.delivered_count});
    }
    if (state.phase == ConsensusPhase::Dispatching) {
        next.push_back({ConsensusPhase::Delivered, state.clock_ticks, state.retry_count,
                        state.queue_depth, static_cast<std::uint16_t>(state.delivered_count + 1)});
        if (state.retry_count < bounds.retry_limit) {
            next.push_back({ConsensusPhase::Retrying, state.clock_ticks,
                            static_cast<std::uint16_t>(state.retry_count + 1),
                            state.queue_depth, state.delivered_count});
        }
    }
    if (state.clock_ticks < bounds.clock_limit) {
        next.push_back({state.phase, static_cast<std::uint16_t>(state.clock_ticks + 1),
                        state.retry_count, state.queue_depth, state.delivered_count});
    } else {
        next.push_back({ConsensusPhase::Expired, state.clock_ticks, state.retry_count,
                        state.queue_depth, state.delivered_count});
    }
    if (state.phase == ConsensusPhase::Retrying && state.queue_depth < bounds.queue_limit) {
        next.push_back({ConsensusPhase::Waiting, state.clock_ticks, state.retry_count,
                        static_cast<std::uint16_t>(state.queue_depth + 1), state.delivered_count});
    }
    return next;
}

inline bool bounded_liveness_holds(const ConsensusState& initial,
                                   const ConsensusBounds& bounds) {
    std::deque<ConsensusState> pending{initial};
    std::vector<ConsensusState> visited;

    while (!pending.empty()) {
        const ConsensusState current = pending.front();
        pending.pop_front();
        if (std::find(visited.begin(), visited.end(), current) != visited.end()) continue;
        visited.push_back(current);

        const auto next = consensus_successors(current, bounds);
        if (!terminal(current) && next.empty()) return false;
        for (const auto& successor : next) pending.push_back(successor);
    }
    return true;
}

}  // namespace eco_restoration
