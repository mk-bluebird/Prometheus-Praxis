// filename: edge/include/econet_blastradius_client.hpp
// destination: EcoNet/edge/include/econet_blastradius_client.hpp
// purpose: C++ header for FOG/edge agents to call the Rust cdylib.

#pragma once

#include <string>

extern "C" {
    char* econet_blastradius_spine_init_json(const char* root_path_utf8,
                                             const char* region_utf8,
                                             double min_restoration_score);
    char* econet_blastradius_spine_improvement_json(const char* root_path_utf8,
                                                    const char* lane_utf8);
    void  econet_blastradius_spine_free_string(char* ptr);
}

inline std::string econet_blastradius_list_region(const std::string& repo_root,
                                                  const std::string& region,
                                                  double min_restoration_score) {
    char* raw = econet_blastradius_spine_init_json(repo_root.c_str(),
                                                   region.c_str(),
                                                   min_restoration_score);
    if (!raw) {
        return {};
    }
    std::string out(raw);
    econet_blastradius_spine_free_string(raw);
    return out;
}

inline std::string econet_blastradius_list_improvement(const std::string& repo_root,
                                                       const std::string& lane) {
    char* raw = econet_blastradius_spine_improvement_json(repo_root.c_str(),
                                                          lane.c_str());
    if (!raw) {
        return {};
    }
    std::string out(raw);
    econet_blastradius_spine_free_string(raw);
    return out;
}
