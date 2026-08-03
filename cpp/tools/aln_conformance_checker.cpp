// File: cpp/tools/aln_conformance_checker.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <algorithm>

namespace eco {

struct AlnStateRange {
    double min;
    double max;
};

struct AlnSpec {
    std::string particleName;
    std::map<std::string, AlnStateRange> stateRanges;
};

struct AlnInvariant {
    std::string entity;
    std::string name;
    std::string expression;
    std::string bound_column;
};

struct SqlTrigger {
    std::string name;
    std::string table;
    std::vector<std::string> columns_checked;
};

struct ConformanceResult {
    bool ok;
    std::vector<std::string> missing_invariants;
    std::vector<std::string> inconsistent_triggers;
};

struct CppPfasParams {
    double max_mass_kg;
    double max_cold_survival_factor;
};

struct CppKerParams {
    double k_min;
    double k_max;
    double e_min;
    double e_max;
    double r_min;
    double r_max;
};

AlnSpec parse_aln_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Failed to open ALN file: " + path);
    }

    AlnSpec spec;
    std::string line;
    while (std::getline(in, line)) {
        std::string trimmed;
        for (char c : line) {
            if (c == '\r' || c == '\n') continue;
            trimmed.push_back(c);
        }

        if (trimmed.find("GOVERNANCE_PARTICLE") != std::string::npos) {
            std::size_t pos = trimmed.find("GOVERNANCE_PARTICLE");
            pos += std::string("GOVERNANCE_PARTICLE").size();
            while (pos < trimmed.size() && (trimmed[pos] == ' ' || trimmed[pos] == '\t')) ++pos;
            std::size_t end = trimmed.find('{', pos);
            spec.particleName = trimmed.substr(pos, end - pos);
        } else if (trimmed.find(": [") != std::string::npos) {
            std::size_t nameEnd = trimmed.find(':');
            std::string name = trimmed.substr(0, nameEnd);
            name.erase(std::remove_if(name.begin(), name.end(), ::isspace), name.end());

            std::size_t rangeStart = trimmed.find('[', nameEnd);
            std::size_t rangeEnd   = trimmed.find(']', rangeStart);
            std::string range = trimmed.substr(rangeStart + 1, rangeEnd - rangeStart - 1);

            double minVal = 0.0;
            double maxVal = 0.0;
            std::size_t commaPos = range.find(',');
            if (commaPos != std::string::npos) {
                std::string minStr = range.substr(0, commaPos);
                std::string maxStr = range.substr(commaPos + 1);
                minVal = std::stod(minStr);
                maxVal = std::stod(maxStr);
            }

            spec.stateRanges[name] = AlnStateRange{minVal, maxVal};
        }
    }

    return spec;
}

std::vector<AlnInvariant> parse_aln_invariants(std::istream& in) {
    std::vector<AlnInvariant> invariants;
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("INVARIANT ", 0) == 0) {
            AlnInvariant inv;
            std::size_t dot = line.find('.');
            std::size_t colon = line.find(':');
            std::size_t colpos = line.find(":: column=");
            if (dot == std::string::npos || colon == std::string::npos || colpos == std::string::npos) {
                continue;
            }
            inv.entity = line.substr(10, dot - 10);
            inv.name = line.substr(dot + 1, colon - dot - 1);
            inv.expression = line.substr(colon + 1, colpos - colon - 1);

            std::string col = line.substr(colpos + 10);
            while (!col.empty() && (col.back() == ' ' || col.back() == '\n' || col.back() == '\r')) {
                col.pop_back();
            }
            inv.bound_column = col;
            invariants.push_back(inv);
        }
    }
    return invariants;
}

ConformanceResult check_invariant_trigger_conformance(
        const std::vector<AlnInvariant>& invariants,
        const std::vector<SqlTrigger>& triggers) {

    ConformanceResult res;
    res.ok = true;

    std::unordered_map<std::string, std::vector<std::string>> column_to_triggers;
    for (const auto& tr : triggers) {
        for (const auto& col : tr.columns_checked) {
            column_to_triggers[col].push_back(tr.name);
        }
    }

    for (const auto& inv : invariants) {
        auto it = column_to_triggers.find(inv.bound_column);
        if (it == column_to_triggers.end()) {
            res.ok = false;
            std::ostringstream oss;
            oss << inv.entity << "." << inv.name << " (column " << inv.bound_column << ")";
            res.missing_invariants.push_back(oss.str());
        } else {
            bool entity_seen = false;
            for (const auto& trname : it->second) {
                if (trname.find(inv.entity) != std::string::npos) {
                    entity_seen = true;
                    break;
                }
            }
            if (!entity_seen) {
                res.ok = false;
                std::ostringstream oss;
                oss << "Triggers for column " << inv.bound_column
                    << " do not reference entity " << inv.entity;
                res.inconsistent_triggers.push_back(oss.str());
            }
        }
    }

    return res;
}

void print_conformance_report(const ConformanceResult& res) {
    if (res.ok) {
        std::cout << "ALN v2 trigger/invariant conformance check: OK\n";
        return;
    }
    std::cout << "ALN v2 trigger/invariant conformance check: FAILED\n";
    if (!res.missing_invariants.empty()) {
        std::cout << "Missing invariants (no corresponding trigger/column):\n";
        for (const auto& s : res.missing_invariants) {
            std::cout << "  - " << s << "\n";
        }
    }
    if (!res.inconsistent_triggers.empty()) {
        std::cout << "Inconsistent triggers:\n";
        for (const auto& s : res.inconsistent_triggers) {
            std::cout << "  - " << s << "\n";
        }
    }
}

void check_pfas_conformance(const AlnSpec& spec, const CppPfasParams& cpp) {
    auto massIt = spec.stateRanges.find("mass_kg");
    auto coldIt = spec.stateRanges.find("cold_survival_factor");

    bool ok = true;

    if (massIt != spec.stateRanges.end()) {
        double alnMaxMass = massIt->second.max;
        if (cpp.max_mass_kg > alnMaxMass) {
            ok = false;
            std::cerr << "PFAS max_mass_kg (C++) = "
                      << cpp.max_mass_kg
                      << " exceeds ALN corridor max " << alnMaxMass << "\n";
        } else {
            std::cout << "PFAS max_mass_kg within corridor: "
                      << cpp.max_mass_kg << " <= " << alnMaxMass << "\n";
        }
    } else {
        std::cerr << "No mass_kg range declared in particle "
                  << spec.particleName << "\n";
    }

    if (coldIt != spec.stateRanges.end()) {
        double alnMaxCold = coldIt->second.max;
        if (cpp.max_cold_survival_factor > alnMaxCold) {
            ok = false;
            std::cerr << "PFAS max_cold_survival_factor (C++) = "
                      << cpp.max_cold_survival_factor
                      << " exceeds ALN corridor max " << alnMaxCold << "\n";
        } else {
            std::cout << "PFAS max_cold_survival_factor within corridor: "
                      << cpp.max_cold_survival_factor << " <= " << alnMaxCold << "\n";
        }
    } else {
        std::cerr << "No cold_survival_factor range declared in particle "
                  << spec.particleName << "\n";
    }

    if (!ok) {
        std::cerr << "PFAS corridor mismatch detected; adjust C++ PFAS parameters to match ALN.\n";
    }
}

void check_ker_semantics(const CppKerParams& cpp) {
    bool ok = true;
    if (cpp.k_min < 0.0 || cpp.k_max > 1.0) {
        ok = false;
        std::cerr << "C++ K range [" << cpp.k_min << "," << cpp.k_max
                  << "] violates [0,1] corridor.\n";
    }
    if (cpp.e_min < 0.0 || cpp.e_max > 1.0) {
        ok = false;
        std::cerr << "C++ E range [" << cpp.e_min << "," << cpp.e_max
                  << "] violates [0,1] corridor.\n";
    }
    if (cpp.r_min < 0.0 || cpp.r_max > 1.0) {
        ok = false;
        std::cerr << "C++ R range [" << cpp.r_min << "," << cpp.r_max
                  << "] violates [0,1] corridor.\n";
    }

    if (ok) {
        std::cout << "C++ KER parameter ranges within [0,1] corridors.\n";
    } else {
        std::cerr << "Adjust C++ KER ranges to remain within [0,1].\n";
    }
}

} // namespace eco

int main(int argc, char** argv) {
    using namespace eco;

    if (argc < 2) {
        std::cerr << "Usage: aln_conformance_checker <aln-file-path>\n";
        return 1;
    }

    std::string alnPath = argv[1];

    try {
        AlnSpec spec = parse_aln_file(alnPath);

        CppPfasParams cppPfas{1000.0, 10.0};
        CppKerParams cppKer{0.0, 1.0, 0.0, 1.0, 0.0, 1.0};

        std::cout << "Checking ALN particle: " << spec.particleName << "\n";
        check_pfas_conformance(spec, cppPfas);
        check_ker_semantics(cppKer);

        std::istringstream aln_invariants(R"ALN(
INVARIANT CarbonAwareCorridor.delta_v_bound: ΔV_t <= min(0.05, γ*s, δ*c) :: column=delta_v_t
INVARIANT CarbonAwareCorridor.ker_positive: s = k*e - r >= 0 :: column=ker_s
INVARIANT PhoenixWaterRights.daily_limit: allocation <= limit :: column=daily_allocation
)ALN");

        auto invariants = parse_aln_invariants(aln_invariants);

        std::vector<SqlTrigger> triggers = {
            {"tr_hex_telemetry_delta_v_t", "hex_telemetry", {"delta_v_t", "ker_s", "carbon_corridor"}},
            {"tr_hex_telemetry_ker_s", "hex_telemetry", {"ker_s"}},
            {"tr_water_rights_daily_allocation", "water_rights_log", {"daily_allocation"}}
        };

        ConformanceResult res = check_invariant_trigger_conformance(invariants, triggers);
        print_conformance_report(res);

        if (!res.ok) {
            return 1;
        }
    } catch (const std::exception& ex) {
        std::cerr << "ALN conformance checker error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
