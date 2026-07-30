// File: cpp/tools/neurorights_audit_trail_and_heat_island_tda.cpp
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>

namespace praxis {
namespace tools {

// ----------------------------------------------------------
// 29. Neurorights audit trail with Merkle non-rollback proofs
// ----------------------------------------------------------
//
// We define an append-only log where each entry records:
//   - psych-state update (band, score, capability_index)
//   - associated reliability_token summary
//   - timestamp
//   - previous entry hash
//   - current entry hash
//
// A Merkle-like chained hash enables efficient proofs that capability_index
// never decreased over a time interval by presenting:
//   - the entry hashes along the chain
//   - capability_index values
//   - a monotonicity check over the interval.

// Simplified hash function stand-in (non-cryptographic).
unsigned simple_mix_hash(const std::string& s) {
    unsigned h = 2166136261u;
    for (unsigned char c : s) {
        h ^= static_cast<unsigned>(c);
        h *= 16777619u;
    }
    return h;
}

enum class PsychBand {
    NORMAL,
    MODERATE,
    HIGH
};

std::string to_string(PsychBand b) {
    switch (b) {
        case PsychBand::NORMAL:   return "NORMAL";
        case PsychBand::MODERATE: return "MODERATE";
        case PsychBand::HIGH:     return "HIGH";
    }
    return "UNKNOWN";
}

struct ReliabilityTokenSummary {
    bool   valid;
    double snr_db;
    double drift_pct_per_hr;
    double issued_time_hr;
    double expiry_time_hr;
};

struct AuditEntry {
    double  time_hr;
    PsychBand band;
    double  psych_score;       // [0,1]
    double  capability_index;  // [0,1], neurorights capability index
    ReliabilityTokenSummary token;

    unsigned prev_hash;
    unsigned entry_hash;
};

std::string serialize_entry_core(const AuditEntry& e) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);
    oss << "time=" << e.time_hr
        << ",band=" << to_string(e.band)
        << ",psych=" << e.psych_score
        << ",cap=" << e.capability_index
        << ",token_valid=" << (e.token.valid ? "true" : "false")
        << ",snr_db=" << e.token.snr_db
        << ",drift=" << e.token.drift_pct_per_hr
        << ",issued=" << e.token.issued_time_hr
        << ",expiry=" << e.token.expiry_time_hr;
    return oss.str();
}

AuditEntry make_entry(double time_hr,
                      PsychBand band,
                      double psych_score,
                      double capability_index,
                      const ReliabilityTokenSummary& token,
                      unsigned prev_hash) {
    AuditEntry e{};
    e.time_hr          = time_hr;
    e.band             = band;
    e.psych_score      = psych_score;
    e.capability_index = capability_index;
    e.token            = token;
    e.prev_hash        = prev_hash;

    std::string core = serialize_entry_core(e);
    std::ostringstream oss;
    oss << core << ",prev_hash=" << prev_hash;
    e.entry_hash = simple_mix_hash(oss.str());
    return e;
}

// Merkle-chain (hash chain) construction: each entry hash depends on core
// fields and prev_hash, creating an append-only audit trail.
//
// Proof of non-rollback over [t_start, t_end]:
//   - Provide entries from index i_start to i_end.
//   - Show that hashes form a valid chain (each prev_hash matches).
//   - Show capability_index is monotone non-decreasing over that subsequence.

struct NonRollbackProof {
    std::vector<AuditEntry> entries;
    bool   hash_chain_valid;
    bool   capability_monotone;
};

NonRollbackProof build_non_rollback_proof(const std::vector<AuditEntry>& log,
                                          std::size_t idx_start,
                                          std::size_t idx_end) {
    NonRollbackProof proof{};
    if (log.empty() || idx_start >= log.size() || idx_end >= log.size() || idx_start > idx_end) {
        proof.hash_chain_valid = false;
        proof.capability_monotone = false;
        return proof;
    }

    proof.entries.reserve(idx_end - idx_start + 1);
    for (std::size_t i = idx_start; i <= idx_end; ++i) {
        proof.entries.push_back(log[i]);
    }

    // Check hash chain.
    bool chain_ok = true;
    for (std::size_t i = 1; i < proof.entries.size(); ++i) {
        const auto& prev = proof.entries[i-1];
        const auto& curr = proof.entries[i];
        if (curr.prev_hash != prev.entry_hash) {
            chain_ok = false;
            break;
        }
    }

    // Check capability monotonicity.
    bool mono_ok = true;
    for (std::size_t i = 1; i < proof.entries.size(); ++i) {
        double cap_prev = proof.entries[i-1].capability_index;
        double cap_curr = proof.entries[i].capability_index;
        if (cap_curr < cap_prev) {
            mono_ok = false;
            break;
        }
    }

    proof.hash_chain_valid    = chain_ok;
    proof.capability_monotone = mono_ok;
    return proof;
}

// ----------------------------------------------------------
// 30. TDA of heat islands via persistent homology
// ----------------------------------------------------------
//
// We sketch a persistent-homology analysis pipeline:
//
//   - Input: time series of Phoenix surface temperature maps T_k(x,y) on a grid.
//   - Construct filtrations (e.g., superlevel sets of temperature) at each time:
//       F_k(τ) = { (x,y) | T_k(x,y) >= τ }.
//   - Compute persistent homology (e.g., H_0, H_1) over τ to identify "hot loops":
//       1D features that persist across thresholds and time.
//   - Birth-death of hot loops indicate appearance and disappearance of
//     persistent heat islands.
//   - Use these intervals to calibrate hex-anchored corridor boundaries:
//       corridors enclosing long-lived hot loops receive tighter RoH boundaries
//       and stronger eco-restoration actions, anchored to hex standards.

struct HotLoopFeature {
    double birth_temp;   // temperature threshold at which loop appears
    double death_temp;   // threshold at which loop disappears
    double birth_time;   // time index when loop first appears
    double death_time;   // time index when loop disappears
};

struct CorridorCalibration {
    double roh_threshold;  // calibrated RoH threshold (<= 0.30)
    double action_intensity; // eco-restoration intensity (e.g., corridor planting rate)
};

CorridorCalibration calibrate_corridor_from_hot_loop(const HotLoopFeature& loop) {
    // Simple heuristic:
    //   - Longer-lived and hotter loops -> lower RoH threshold and higher action intensity.
    double temp_persistence = loop.death_temp - loop.birth_temp;
    double time_persistence = loop.death_time - loop.birth_time;

    double persistence_score = temp_persistence * time_persistence;

    // Map persistence_score into RoH threshold [0.20, 0.30]
    double roh_min = 0.20;
    double roh_max = 0.30;
    double roh_span = roh_max - roh_min;
    double roh_thresh = roh_max - roh_span * std::tanh(persistence_score / 10.0);

    // Map persistence_score into action_intensity [0.3, 1.0]
    double action_min = 0.3;
    double action_max = 1.0;
    double action_span = action_max - action_min;
    double action_intensity = action_min + action_span * std::tanh(persistence_score / 10.0);

    return CorridorCalibration{roh_thresh, action_intensity};
}

// ----------------------------------------------------------
// Demonstration main
// ----------------------------------------------------------

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // 29. Neurorights audit trail demo.
    ReliabilityTokenSummary token{
        true, 13.0, 1.5, 100.0, 124.0
    };

    std::vector<AuditEntry> log;
    unsigned prev_hash = 0;

    // Append three entries with increasing capability_index.
    log.push_back(make_entry(101.0, PsychBand::NORMAL, 0.40, 0.80, token, prev_hash));
    prev_hash = log.back().entry_hash;
    log.push_back(make_entry(103.0, PsychBand::MODERATE, 0.55, 0.82, token, prev_hash));
    prev_hash = log.back().entry_hash;
    log.push_back(make_entry(106.0, PsychBand::MODERATE, 0.60, 0.85, token, prev_hash));

    NonRollbackProof proof = build_non_rollback_proof(log, 0, 2);

    std::cout << "Neurorights audit trail (non-rollback proof):\n";
    std::cout << "  Hash chain valid? " << (proof.hash_chain_valid ? "YES" : "NO") << "\n";
    std::cout << "  Capability index monotone over interval? "
              << (proof.capability_monotone ? "YES" : "NO") << "\n";
    if (proof.hash_chain_valid && proof.capability_monotone) {
        std::cout << "  Proof demonstrates non-rollback of capabilities over the "
                     "selected time interval.\n\n";
    }

    // 30. Heat island TDA calibration demo.
    HotLoopFeature loop{
        40.0, // birth_temp
        45.0, // death_temp
        0.0,  // birth_time (days)
        30.0  // death_time (days)
    };

    CorridorCalibration calib = calibrate_corridor_from_hot_loop(loop);

    std::cout << "Heat-island TDA calibration (hot loop feature):\n";
    std::cout << "  Birth_temp=" << loop.birth_temp << "°C, Death_temp=" << loop.death_temp << "°C\n";
    std::cout << "  Birth_time=" << loop.birth_time << " days, Death_time=" << loop.death_time << " days\n";
    std::cout << "  Calibrated corridor RoH threshold: " << calib.roh_threshold << "\n";
    std::cout << "  Calibrated eco-restoration action intensity: " << calib.action_intensity << "\n";
    std::cout << "  Longer-lived, hotter loops tighten RoH corridors and increase "
                 "green-corridor maintenance intensity in Phoenix.\n";

    return 0;
}

} // namespace tools
} // namespace praxis
