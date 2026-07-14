// file: eco_restoration_shard/cyboquatic_progress/20260712/cpp/cyboquatic_governance_particle.cpp
#include <cstdint>
#include <string>
#include <vector>
#include <sstream>

namespace cyboquatic {

static inline double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

struct GovernanceParticle {
    std::string did;
    std::string crate_id;
    std::string domain;
    std::string subtask_id;
    std::string evidence_hex;
    double k_score;
    double e_score;
    double r_score;
    double lyapunov_vt;
};

static inline std::string to_sql_insert(const GovernanceParticle& p) {
    std::ostringstream o;
    o << "INSERT INTO daily_progress "
      << "(yyyymmdd, crateid, domain, subtaskid, nodeid, sampleid, timestamputc, "
      << "evidencehex, kfactor, efactor, rfactor, priorcrateid, didbound, vtafter) VALUES ("
      << "'20260712', "
      << "'" << p.crate_id << "', "
      << "'" << p.domain << "', "
      << "'" << p.subtask_id << "', "
      << "'PHX-GOV-NODE-01', "
      << "'PHX-GOV-SAMPLE-0001', "
      << "'2026-07-12T23:31:00Z', "
      << "'" << p.evidence_hex << "', "
      << p.k_score << ", "
      << p.e_score << ", "
      << p.r_score << ", "
      << "'cyboquatic_governance_particle_20260711', "
      << "'" << p.did << "', "
      << p.lyapunov_vt << ");";
    return o.str();
}

} // namespace cyboquatic
