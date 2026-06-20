// filename: src/cpp/cyboquatic_guard/laneguard.hpp
// repo: mk-bluebird/eco_restoration_shard

#pragma once

#include <string>

namespace cyboquatic_guard {

struct LaneGuardConfig {
    double min_k;
    double min_e;
    double max_r;
    double max_roh;
};

struct LaneContext {
    std::string lane;
    std::string region;
    std::string node_id;
    double k;
    double e;
    double r;
    double vt;
    double roh;
    bool carbon_negative_ok;
    bool restoration_ok;
    bool blast_safe;
};

struct LaneGuardResult {
    bool admissible;
    std::string reason;
};

class LaneGuard {
public:
    explicit LaneGuard(const LaneGuardConfig& config);

    LaneGuardResult check_lane_admissible(const LaneContext& ctx) const;

private:
    LaneGuardConfig config_;
};

} // namespace cyboquatic_guard
