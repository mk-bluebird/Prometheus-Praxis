// File: cpp/tools/aln_conformance_checker.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include <algorithm>

/**
 * @brief Simple ALN v2 spec parser for KER and PFAS modules.
 *
 * This tool reads ALN particles (e.g., eco_multilang_binding.aln2,
 * qpudatashard_ker_pfas_refined.aln2) and extracts corridor parameters
 * (e.g., cold_survival_factor ranges, PFAS mass bounds) to compare against
 * C++ implementation parameters used in eco_restoration modules.[59]
 */

struct AlnStateRange {
    double min;
    double max;
};

struct AlnSpec {
    std::string particleName;
    std::map<std::string, AlnStateRange> stateRanges;
};

static AlnSpec parse_aln_file(const std::string& path) {
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
            // STATE field line: name : [min, max];
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

// Example C++ PFAS implementation parameters to compare against ALN spec.
struct CppPfasParams {
    double max_mass_kg;
    double max_cold_survival_factor;
};

static void check_pfas_conformance(const AlnSpec& spec, const CppPfasParams& cpp) {
    auto massIt = spec.stateRanges.find("mass_kg");
    auto coldIt = spec.stateRanges.find("cold_survival_factor");

    bool ok = true;

    if (massIt != spec.stateRanges.end()) {
        double alnMaxMass = massIt->second.max;
        if (cpp.max_mass_kg > alnMaxMass) {
            ok = false;
            std::cerr << "[ALN CONFORMANCE FAIL] PFAS max_mass_kg (C++) = "
                      << cpp.max_mass_kg
                      << " exceeds ALN corridor max " << alnMaxMass << "\n";
        } else {
            std::cout << "[ALN CONFORMANCE OK] PFAS max_mass_kg within corridor: "
                      << cpp.max_mass_kg << " <= " << alnMaxMass << "\n";
        }
    } else {
        std::cerr << "[ALN WARNING] No mass_kg range declared in particle "
                  << spec.particleName << "\n";
    }

    if (coldIt != spec.stateRanges.end()) {
        double alnMaxCold = coldIt->second.max;
        if (cpp.max_cold_survival_factor > alnMaxCold) {
            ok = false;
            std::cerr << "[ALN CONFORMANCE FAIL] PFAS max_cold_survival_factor (C++) = "
                      << cpp.max_cold_survival_factor
                      << " exceeds ALN corridor max " << alnMaxCold << "\n";
        } else {
            std::cout << "[ALN CONFORMANCE OK] PFAS max_cold_survival_factor within corridor: "
                      << cpp.max_cold_survival_factor << " <= " << alnMaxCold << "\n";
        }
    } else {
        std::cerr << "[ALN WARNING] No cold_survival_factor range declared in particle "
                  << spec.particleName << "\n";
    }

    if (!ok) {
        std::cerr << "[ALN CONFORMANCE SUMMARY] DID "
                  << "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7 "
                  << " corridor mismatch detected; adjust C++ PFAS parameters to match ALN.\n";
    }
}

// Example KER implementation parameters.
struct CppKerParams {
    double k_min;
    double k_max;
    double e_min;
    double e_max;
    double r_min;
    double r_max;
};

static void check_ker_semantics(const CppKerParams& cpp) {
    // For KER, we primarily check that k,e,r remain in [0,1] as per governance docs.[59]
    bool ok = true;
    if (cpp.k_min < 0.0 || cpp.k_max > 1.0) {
        ok = false;
        std::cerr << "[ALN CONFORMANCE FAIL] C++ K range [" << cpp.k_min << "," << cpp.k_max
                  << "] violates [0,1] corridor.\n";
    }
    if (cpp.e_min < 0.0 || cpp.e_max > 1.0) {
        ok = false;
        std::cerr << "[ALN CONFORMANCE FAIL] C++ E range [" << cpp.e_min << "," << cpp.e_max
                  << "] violates [0,1] corridor.\n";
    }
    if (cpp.r_min < 0.0 || cpp.r_max > 1.0) {
        ok = false;
        std::cerr << "[ALN CONFORMANCE FAIL] C++ R range [" << cpp.r_min << "," << cpp.r_max
                  << "] violates [0,1] corridor.\n";
    }

    if (ok) {
        std::cout << "[ALN CONFORMANCE OK] C++ KER parameter ranges within [0,1] corridors.\n";
    } else {
        std::cerr << "[ALN CONFORMANCE SUMMARY] Adjust C++ KER ranges to remain within [0,1].\n";
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: aln_conformance_checker <aln-file-path>\n";
        return 1;
    }

    std::string alnPath = argv[1];

    try {
        AlnSpec spec = parse_aln_file(alnPath);

        // Example C++ parameters; in a real integration, these could be
        // loaded from config or compiled constants matching cpp PFAS modules.
        CppPfasParams cppPfas{ /*max_mass_kg=*/1000.0, /*max_cold_survival_factor=*/10.0 };
        CppKerParams cppKer{0.0, 1.0, 0.0, 1.0, 0.0, 1.0};

        std::cout << "Checking ALN particle: " << spec.particleName << "\n";
        check_pfas_conformance(spec, cppPfas);
        check_ker_semantics(cppKer);
    } catch (const std::exception& ex) {
        std::cerr << "ALN conformance checker error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
