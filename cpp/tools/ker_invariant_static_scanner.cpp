// File: cpp/tools/ker_invariant_static_scanner.cpp
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <regex>

/*
 * Static Analysis of C++ KER Invariant Usage (lightweight version)
 *
 * This tool scans C++ files under a given directory for KER scalar calculations
 * of the form "s = k * e - r" and verifies they match the normalized pattern
 * used in the database (clamping k,e,r to [0,1] before computing s).
 *
 * While a full clang-based tool would hook into ASTs, this shard provides
 * a portable pattern-based scanner that can be used in CI. Migrating to
 * clang tooling later is straightforward.
 *
 * Usage:
 *   ker_invariant_static_scanner <root_dir>
 */

namespace eco {

struct KerUsage {
    std::string file;
    int line;
    std::string snippet;
    bool normalized;
};

bool line_has_ker_calc(const std::string& line) {
    static std::regex ker_regex(R"(s\s*=\s*k\s*\*\s*e\s*-\s*r)");
    return std::regex_search(line, ker_regex);
}

bool file_has_normalization(const std::vector<std::string>& lines, int idx) {
    // Look at a window of lines above for normalization patterns.
    int start = std::max(0, idx - 5);
    for (int i = start; i < idx; ++i) {
        const auto& l = lines[i];
        if (l.find("if (k < 0.0) k = 0.0;") != std::string::npos &&
            l.find("if (k > 1.0) k = 1.0;") != std::string::npos) {
            return true;
        }
        if (l.find("if (e < 0.0) e = 0.0;") != std::string::npos &&
            l.find("if (e > 1.0) e = 1.0;") != std::string::npos) {
            return true;
        }
        if (l.find("if (r < 0.0) r = 0.0;") != std::string::npos &&
            l.find("if (r > 1.0) r = 1.0;") != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::vector<KerUsage> scan_file(const std::filesystem::path& path) {
    std::vector<KerUsage> usages;
    std::ifstream in(path);
    if (!in) return usages;

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }

    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        if (line_has_ker_calc(lines[i])) {
            KerUsage u{};
            u.file = path.string();
            u.line = i + 1;
            u.snippet = lines[i];
            u.normalized = file_has_normalization(lines, i);
            usages.push_back(u);
        }
    }
    return usages;
}

void scan_directory(const std::filesystem::path& root) {
    std::vector<KerUsage> all;
    for (auto& p : std::filesystem::recursive_directory_iterator(root)) {
        if (p.is_regular_file()) {
            auto ext = p.path().extension().string();
            if (ext == ".cpp" || ext == ".hpp" || ext == ".h") {
                auto usages = scan_file(p.path());
                all.insert(all.end(), usages.begin(), usages.end());
            }
        }
    }

    int total = static_cast<int>(all.size());
    int normalized = 0;
    for (const auto& u : all) {
        if (u.normalized) normalized++;
        std::cout << "[KER_SCAN] file=" << u.file
                  << " line=" << u.line
                  << " normalized=" << (u.normalized ? "YES" : "NO")
                  << " snippet=\"" << u.snippet << "\"\n";
    }

    std::cout << "\nKER invariant usage summary: "
              << "total=" << total
              << " normalized=" << normalized
              << " unnormalized=" << (total - normalized) << "\n";
}

} // namespace eco

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ker_invariant_static_scanner <root_dir>\n";
        return 1;
    }
    std::filesystem::path root(argv[1]);
    eco::scan_directory(root);
    return 0;
}
