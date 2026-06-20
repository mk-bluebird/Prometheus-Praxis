// filename: src/cpp/cyboquatic_guard/kerguard.cpp
// repo: mk-bluebird/eco_restoration_shard

#include "kerguard.hpp"

namespace cyboquatic_guard {

KerGuardResult KerGuard::check_upgrade(const KerState& state) const {
    KerGuardResult result;
    result.monotone_ok = true;
    result.lyapunov_ok = true;
    result.reason.clear();

    if (state.k_new < state.k_old) {
        result.monotone_ok = false;
        result.reason = "K_new < K_old";
    }
    if (state.e_new < state.e_old) {
        result.monotone_ok = false;
        if (!result.reason.empty()) {
            result.reason += "; ";
        }
        result.reason += "E_new < E_old";
    }
    if (state.r_new > state.r_old) {
        result.monotone_ok = false;
        if (!result.reason.empty()) {
            result.reason += "; ";
        }
        result.reason += "R_new > R_old";
    }

    if (state.vt_new > state.vt_old + state.vt_epsilon) {
        result.lyapunov_ok = false;
        if (!result.reason.empty()) {
            result.reason += "; ";
        }
        result.reason += "Lyapunov violation: Vt_new > Vt_old + epsilon";
    }

    if (result.reason.empty()) {
        result.reason = "KER upgrade monotone and Lyapunov-safe";
    }

    return result;
}

} // namespace cyboquatic_guard
