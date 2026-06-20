// filename: src/cpp/cyboquatic_guard/blastguard.cpp
// repo: mk-bluebird/eco_restoration_shard

#include "blastguard.hpp"
#include <cmath>

namespace cyboquatic_guard {

namespace {
double distance_metric(double r_center, double r_neighbor) {
    return r_center + r_neighbor;
}
} // namespace

BlastGuardResult BlastGuard::check_neighbors_safe(
    const BlastNode& center,
    const std::vector<BlastNode>& neighbors) const
{
    BlastGuardResult result;
    result.safe = true;
    result.reason.clear();

    for (const auto& n : neighbors) {
        const double d = distance_metric(center.radius_m, n.radius_m);
        if (d <= 0.0) {
            continue;
        }
        if (!center.carbon_negative_ok || !center.restoration_ok) {
            result.safe = false;
            result.reason = "Center node fails carbon-negative or restoration requirements";
            return result;
        }
        if (!n.carbon_negative_ok || !n.restoration_ok) {
            result.safe = false;
            result.reason = "Neighbor node fails carbon-negative or restoration requirements";
            return result;
        }
    }

    if (result.reason.empty()) {
        result.reason = "Blast adjacency safe for Cyboquatic nodes";
    }

    return result;
}

} // namespace cyboquatic_guard
