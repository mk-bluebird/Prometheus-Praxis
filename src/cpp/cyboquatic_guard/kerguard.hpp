// filename: src/cpp/cyboquatic_guard/kerguard.hpp
// repo: mk-bluebird/eco_restoration_shard

#pragma once

namespace cyboquatic_guard {

struct KerState {
    double k_old;
    double e_old;
    double r_old;
    double vt_old;
    double k_new;
    double e_new;
    double r_new;
    double vt_new;
    double vt_epsilon;
};

struct KerGuardResult {
    bool monotone_ok;
    bool lyapunov_ok;
    std::string reason;
};

class KerGuard {
public:
    KerGuard() = default;

    KerGuardResult check_upgrade(const KerState& state) const;
};

} // namespace cyboquatic_guard
