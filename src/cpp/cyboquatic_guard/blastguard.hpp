// filename: src/cpp/cyboquatic_guard/blastguard.hpp
// repo: mk-bluebird/eco_restoration_shard

#pragma once

#include <string>
#include <vector>

namespace cyboquatic_guard {

struct BlastNode {
    std::string node_id;
    double radius_m;
    bool carbon_negative_ok;
    bool restoration_ok;
};

struct BlastGuardResult {
    bool safe;
    std::string reason;
};

class BlastGuard {
public:
    BlastGuard() = default;

    BlastGuardResult check_neighbors_safe(
        const BlastNode& center,
        const std::vector<BlastNode>& neighbors) const;
};

} // namespace cyboquatic_guard
