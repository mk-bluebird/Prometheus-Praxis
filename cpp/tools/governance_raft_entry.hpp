// File: cpp/tools/governance_raft_entry.hpp

#ifndef ECOSAFETY_CORE_V2_GOVERNANCE_RAFT_ENTRY_HPP
#define ECOSAFETY_CORE_V2_GOVERNANCE_RAFT_ENTRY_HPP

#include <string>

// Raft log entry struct for governance lane promotion.
struct GovernanceRaftEntry {
    int         term;
    std::string cluster_id;
    std::string node_id;
    std::string particle_id;
    std::string lane_prev;
    std::string lane_next;
    std::string evidence_hex;
    double      vt_residual;
    double      K;
    double      E;
    double      R;
    std::string signing_did;
    std::string signature_hex; // digital signature over the entry fields

    // Serialize entry to a canonical string (e.g., JSON) for signing/logging.
    std::string serialize() const {
        // Minimal JSON; in production use a robust JSON library.
        return std::string("{") +
            "\"term\":" + std::to_string(term) + "," +
            "\"cluster_id\":\"" + cluster_id + "\"," +
            "\"node_id\":\"" + node_id + "\"," +
            "\"particle_id\":\"" + particle_id + "\"," +
            "\"lane_prev\":\"" + lane_prev + "\"," +
            "\"lane_next\":\"" + lane_next + "\"," +
            "\"evidence_hex\":\"" + evidence_hex + "\"," +
            "\"vt_residual\":" + std::to_string(vt_residual) + "," +
            "\"K\":" + std::to_string(K) + "," +
            "\"E\":" + std::to_string(E) + "," +
            "\"R\":" + std::to_string(R) + "," +
            "\"signing_did\":\"" + signing_did + "\"," +
            "\"signature_hex\":\"" + signature_hex + "\"" +
            "}";
    }
};

#endif // ECOSAFETY_CORE_V2_GOVERNANCE_RAFT_ENTRY_HPP
