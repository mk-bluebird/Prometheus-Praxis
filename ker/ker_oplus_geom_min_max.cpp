#include "ker_oplus_geom_min_max.hpp"

#include <cmath>
#include <algorithm>
#include <string>

namespace {

bool in_unit_interval(float v) {
    return v >= 0.0f && v <= 1.0f;
}

} // namespace

extern "C" {

int32_t ker_oplus_geom_min_max(
    const ker_particle2026v1* left,
    const ker_particle2026v1* right,
    ker_composition2026v1* out_comp
) {
    if (!left || !right || !out_comp) {
        return KER_STATUS_NULL_ARG;
    }

    if (!in_unit_interval(left->K)  || !in_unit_interval(right->K) ||
        !in_unit_interval(left->E)  || !in_unit_interval(right->E) ||
        !in_unit_interval(left->R)  || !in_unit_interval(right->R)) {
        return KER_STATUS_INVALID_RANGE;
    }

    const float k_prod = left->K * right->K;
    const float k_comb = std::sqrt(k_prod);
    const float e_comb = std::min(left->E, right->E);
    const float r_comb = std::max(left->R, right->R);

    // Canonical member ordering: idmin,idmax (lexicographic).
    std::string id_left  = left->particle_id ? left->particle_id : "";
    std::string id_right = right->particle_id ? right->particle_id : "";
    std::string id_min   = id_left <= id_right ? id_left : id_right;
    std::string id_max   = id_left <= id_right ? id_right : id_left;
    static thread_local std::string members_buf;
    static thread_local std::string combined_id_buf;

    members_buf = id_min;
    members_buf.append(",");
    members_buf.append(id_max);

    combined_id_buf = id_min;
    combined_id_buf.append("+");
    combined_id_buf.append(id_max);

    out_comp->left_particle_id  = left->particle_id;
    out_comp->right_particle_id = right->particle_id;
    out_comp->combined_id       = combined_id_buf.c_str();
    out_comp->K_combined        = k_comb;
    out_comp->E_combined        = e_comb;
    out_comp->R_combined        = r_comb;
    out_comp->members           = members_buf.c_str();
    out_comp->rule_id           = "keroplusgeomminmaxv1";
    // evidencehex and signinghex are not computed in this function.
    out_comp->evidencehex       = nullptr;
    out_comp->signinghex        = nullptr;

    return KER_STATUS_OK;
}

} // extern "C"
