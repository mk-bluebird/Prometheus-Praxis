// File: cpp/tools/eco_scenario_library_spec.cpp
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>

/*
 * Eco‑restoration scenario library specification and helpers.
 *
 * This file encodes a concrete, language‑neutral scenario format that can be
 * shared across C++, Kotlin, Java, and Lua tools. Scenario files are simple
 * UTF‑8 key=value blocks grouped by section, with stable keys for hex IDs,
 * canal nodes, PFAS settings, and workloads.
 *
 * On‑disk layout (repository‑relative):
 *
 *   scenarios/
 *     phoenix/
 *       HEX-0001/
 *         scenario_pfas.conf
 *         scenario_blast.conf
 *         scenario_workload.conf
 *       HEX-0002/
 *         ...
 *
 * Each `.conf` file is an INI‑like text file:
 *
 *   [scenario]
 *   hex_id=HEX-0001
 *   canal_node=PHX_CANAL_NODE_A
 *   domain=CYBOQUATIC
 *
 *   [pfas]
 *   initial_mass_kg=0.001
 *   initial_sorbed_fraction=0.50
 *   initial_cold_survival_factor=1.00
 *   base_degradation_rate_per_day=0.01
 *   cold_temp_C=12.0
 *   current_temp_C=10.0
 *   sorption_increment_per_day=0.001
 *
 *   [workload]
 *   flow_rate_m3_s=0.30
 *   head_m=6.0
 *   efficiency=0.80
 *   topology_stress_norm=0.30
 *   max_energy_J=5000000.0
 *   max_deltaVt=1.0
 *
 * Kotlin and Java tools can parse these files using standard
 * java.util.Properties or simple line‑based readers; Lua tools can parse
 * them with a minimal INI parser; ALN shards reference scenarios by
 * `hex_id` and `canal_node` and treat these files as non‑actuating inputs.
 */

struct ScenarioSection {
    std::map<std::string, std::string> kv;
};

struct EcoScenario {
    ScenarioSection scenario;  // [scenario]
    ScenarioSection pfas;      // [pfas]
    ScenarioSection workload;  // [workload]
    ScenarioSection blast;     // [blast] optional
};

static std::string trim(const std::string &s) {
    std::size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n')) {
        ++start;
    }
    std::size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n')) {
        --end;
    }
    return s.substr(start, end - start);
}

static EcoScenario parse_scenario_file(const std::string &path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Failed to open scenario file: " + path);
    }

    EcoScenario scenario;
    ScenarioSection *current = nullptr;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            std::string section = line.substr(1, line.size() - 2);
            if (section == "scenario") {
                current = &scenario.scenario;
            } else if (section == "pfas") {
                current = &scenario.pfas;
            } else if (section == "workload") {
                current = &scenario.workload;
            } else if (section == "blast") {
                current = &scenario.blast;
            } else {
                current = nullptr;
            }
            continue;
        }
        if (!current) {
            continue;
        }
        std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        (*current).kv[key] = value;
    }

    return scenario;
}

static void write_scenario_file(const EcoScenario &scenario, const std::string &path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open scenario output: " + path);
    }

    auto dump_section = [&out](const std::string &name, const ScenarioSection &sec) {
        if (sec.kv.empty()) {
            return;
        }
        out << "[" << name << "]\n";
        for (const auto &kv : sec.kv) {
            out << kv.first << "=" << kv.second << "\n";
        }
        out << "\n";
    };

    dump_section("scenario", scenario.scenario);
    dump_section("pfas", scenario.pfas);
    dump_section("workload", scenario.workload);
    dump_section("blast", scenario.blast);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cout << "Eco‑restoration scenario library spec\n\n"
                  << "Scenario files are organized under scenarios/<domain>/<hex_id>/.\n"
                  << "Each file is an INI‑like text with [scenario], [pfas], [workload], [blast] sections.\n"
                  << "C++, Kotlin, Java, Lua, and ALN tools should treat these files as read‑only\n"
                  << "inputs and share the same keys for hex IDs, canal nodes, PFAS, and workloads.\n\n"
                  << "Usage:\n"
                  << "  eco_scenario_library_spec <path_to_scenario.conf>\n";
        return 0;
    }

    try {
        EcoScenario s = parse_scenario_file(argv[1]);
        std::cout << "Parsed scenario from " << argv[1] << "\n";
        std::cout << "hex_id=" << s.scenario.kv.at("hex_id")
                  << " canal_node=" << s.scenario.kv.at("canal_node") << "\n";
        if (!s.pfas.kv.empty()) {
            std::cout << "PFAS initial_mass_kg=" << s.pfas.kv.at("initial_mass_kg")
                      << " sorbed_fraction=" << s.pfas.kv.at("initial_sorbed_fraction") << "\n";
        }
        if (!s.workload.kv.empty()) {
            std::cout << "Workload flow_rate_m3_s=" << s.workload.kv.at("flow_rate_m3_s")
                      << " head_m=" << s.workload.kv.at("head_m") << "\n";
        }
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
