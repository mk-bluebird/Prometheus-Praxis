// File: cpp/tools/runtime_consent_corridor_enforcer.cpp
#include <iostream>
#include <string>
#include <unordered_map>

// This file sketches a runtime consent corridor enforcer for an MCP-like server.
// It checks consent_level for each neuro-flagged query against module_consent_state,
// and decides whether to allow or block the flow. Instead of actual DB/HTTP wiring,
// it emits SQL that the MCP layer can execute, and prints the decision.

namespace eco {

enum class ConsentDecision {
    ALLOW,
    BLOCK
};

struct NeuroQuery {
    std::string request_id;
    std::string module_id;
    bool neuro_flagged;
    int required_consent_level; // e.g. 0=NONE,1=BASIC,2=ADVANCED,3=NEURO-ADJACENT
};

struct ModuleConsentState {
    std::string module_id;
    int consent_level; // current consent level granted for the module
    bool revoked;
};

struct ConsentCheckResult {
    NeuroQuery query;
    ModuleConsentState state;
    ConsentDecision decision;
    std::string reason;
};

// Simple in-memory cache of consent state (would be populated from SQLite).
class ConsentStateCache {
public:
    void add_state(const ModuleConsentState& s) {
        states[s.module_id] = s;
    }

    bool get_state(const std::string& module_id, ModuleConsentState& out) const {
        auto it = states.find(module_id);
        if (it == states.end()) return false;
        out = it->second;
        return true;
    }

private:
    std::unordered_map<std::string, ModuleConsentState> states;
};

ConsentCheckResult check_consent(const NeuroQuery& q,
                                 const ConsentStateCache& cache) {
    ModuleConsentState st{};
    bool found = cache.get_state(q.module_id, st);

    ConsentCheckResult res{};
    res.query = q;
    res.state = st;

    if (!q.neuro_flagged) {
        res.decision = ConsentDecision::ALLOW;
        res.reason = "Query not neuro-flagged; consent corridor not applied.";
        return res;
    }

    if (!found) {
        res.decision = ConsentDecision::BLOCK;
        res.reason = "No consent state found for module; blocking neuro-flagged query.";
        return res;
    }

    if (st.revoked) {
        res.decision = ConsentDecision::BLOCK;
        res.reason = "Consent revoked for module; blocking neuro-flagged query.";
        return res;
    }

    if (st.consent_level < q.required_consent_level) {
        res.decision = ConsentDecision::BLOCK;
        res.reason = "Insufficient consent_level for required neuro corridor.";
        return res;
    }

    res.decision = ConsentDecision::ALLOW;
    res.reason = "Consent corridor satisfied; allowing neuro-flagged query.";
    return res;
}

// Emit SQL audit record for the consent decision.
void emit_consent_audit_sql(const ConsentCheckResult& res) {
    std::cout << "INSERT INTO consent_corridor_audit "
              << "(request_id, module_id, neuro_flagged, required_consent_level, "
              << "effective_consent_level, revoked, decision, reason) VALUES ('"
              << res.query.request_id << "', '"
              << res.query.module_id << "', "
              << (res.query.neuro_flagged ? 1 : 0) << ", "
              << res.query.required_consent_level << ", "
              << res.state.consent_level << ", "
              << (res.state.revoked ? 1 : 0) << ", '"
              << (res.decision == ConsentDecision::ALLOW ? "ALLOW" : "BLOCK") << "', '"
              << res.reason << "');\n";
}

} // namespace eco

int main() {
    using namespace eco;

    ConsentStateCache cache;
    cache.add_state({"module_NEURO_001", 3, false});
    cache.add_state({"module_NEURO_002", 1, false});
    cache.add_state({"module_NEURO_003", 2, true}); // revoked

    std::vector<NeuroQuery> queries = {
        {"req_1", "module_NEURO_001", true, 2},
        {"req_2", "module_NEURO_002", true, 2},
        {"req_3", "module_NEURO_003", true, 1},
        {"req_4", "module_NEURO_004", true, 1},
        {"req_5", "module_STD_001", false, 0}
    };

    for (const auto& q : queries) {
        ConsentCheckResult res = check_consent(q, cache);
        std::cout << "Runtime consent check for " << q.request_id << " (module "
                  << q.module_id << "): decision="
                  << (res.decision == ConsentDecision::ALLOW ? "ALLOW" : "BLOCK")
                  << " reason=" << res.reason << "\n";
        emit_consent_audit_sql(res);
    }

    return 0;
}
