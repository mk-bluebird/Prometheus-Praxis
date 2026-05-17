// filename: cpp/econet_spine_inspect.hpp
// destination: mk-bluebird/eco_restoration_shard/cpp/econet_spine_inspect.hpp
// role: C++ header for non‑actuating Cyboquatic machinery analytics

#pragma once

#include <string>
#include <vector>

struct EcoNodeEnergyCarbonCpp {
    std::string node_id;
    long n_events;
    double e_req_accept_j;
    double e_surplus_accept_j;
    double r_carbon_avg;
    double r_biodiv_avg;
    double dv_avg;
};

struct EcoCandidateEcorestorativeCpp {
    std::string source_type;
    std::string source_id;
    double impact_carbon;
    double impact_biodiv;
    double vt_sensitivity_avg;
    double dv_avg;
};

std::vector<EcoNodeEnergyCarbonCpp>
econet_query_best_nodes_for_energy_tailwind(const std::string &db_path, int limit);

std::vector<EcoCandidateEcorestorativeCpp>
econet_query_candidate_ecorestorative(const std::string &db_path, int limit);
