// filename: cpp/core/prometheus_praxis_diff_analyzer.hpp
// destination: github.com/mk-bluebird/Prometheus-Praxis
//
// Prometheus-Praxis Diff Analyzer
//
// This header provides a unified-diff parser and basic analysis utilities.
// It is designed for collaborators who want to:
//   - Inspect changes to ALN shards, Rust crates, or configuration files.
//   - Build tooling that links diffs to ALNv2 safety and KER invariants.
//   - Integrate with Rust (alncore) by passing patch payloads and deriving
//     continuity or governance signals.
//
// The parser expects standard "git diff --unified" style payloads.

#ifndef INCLUDED_PROMETHEUS_PRAXIS_DIFF_ANALYZER_HPP
#define INCLUDED_PROMETHEUS_PRAXIS_DIFF_ANALYZER_HPP

#include <string>
#include <vector>
#include <sstream>
#include <regex>
#include <iostream>
#include <map>

namespace praxis_analysis {

enum class ChangeType {
    CONTEXT,
    ADDITION,
    DELETION
};

struct AnalyzedLine {
    int old_line_num;
    int new_line_num;
    ChangeType type;
    std::string payload;
};

struct ChunkHunk {
    int old_start;
    int old_count;
    int new_start;
    int new_count;
    std::vector<AnalyzedLine> active_lines;
};

struct FileDelta {
    std::string origin_file;
    std::string target_file;
    std::vector<ChunkHunk> hunks;
    bool is_new_resource{false};
};

class NativeDiffParser {
public:
    /// Parse a unified patch payload into a vector of FileDelta structures.
    /// Each FileDelta describes origin/target paths, hunks, and per-line changes.
    static std::vector<FileDelta> analyze_unified_patch(const std::string& patch_payload) {
        std::vector<FileDelta> deltas;
        std::stringstream ss(patch_payload);
        std::string current_line;
        FileDelta* active_delta = nullptr;

        std::regex hunk_pattern(R"(@@\s+-(\d+),?(\d*)\s+\+(\d+),?(\d*)\s+@@)");

        while (std::getline(ss, current_line)) {
            if (current_line.rfind("--- a/", 0) == 0) {
                FileDelta new_delta;
                new_delta.origin_file = current_line.substr(6);
                deltas.push_back(std::move(new_delta));
                active_delta = &deltas.back();
            } else if (current_line.rfind("+++ b/", 0) == 0 && active_delta) {
                active_delta->target_file = current_line.substr(6);
            } else if (current_line.rfind("new file mode", 0) == 0 && active_delta) {
                active_delta->is_new_resource = true;
            } else if (current_line.rfind("@@", 0) == 0 && active_delta) {
                std::smatch match_results;
                if (std::regex_search(current_line, match_results, hunk_pattern)) {
                    ChunkHunk new_hunk;
                    new_hunk.old_start = std::stoi(match_results[1].str());
                    new_hunk.old_count = match_results[2].str().empty() ? 1 : std::stoi(match_results[2].str());
                    new_hunk.new_start = std::stoi(match_results[3].str());
                    new_hunk.new_count = match_results[4].str().empty() ? 1 : std::stoi(match_results[4].str());
                    active_delta->hunks.push_back(std::move(new_hunk));
                }
            } else if (active_delta && !active_delta->hunks.empty()) {
                auto& current_hunk = active_delta->hunks.back();
                if (!current_line.empty() && current_line[0] == '+') {
                    current_hunk.active_lines.push_back({
                        -1,
                        current_hunk.new_start++,
                        ChangeType::ADDITION,
                        current_line.substr(1)
                    });
                } else if (!current_line.empty() && current_line[0] == '-') {
                    current_hunk.active_lines.push_back({
                        current_hunk.old_start++,
                        -1,
                        ChangeType::DELETION,
                        current_line.substr(1)
                    });
                } else if (!current_line.empty() && current_line[0] == ' ') {
                    current_hunk.active_lines.push_back({
                        current_hunk.old_start++,
                        current_hunk.new_start++,
                        ChangeType::CONTEXT,
                        current_line.substr(1)
                    });
                }
            }
        }
        return deltas;
    }

    /// Compute a coarse summary over all FileDelta entries:
    /// total additions, deletions, and context lines.
    static void summarize(const std::vector<FileDelta>& deltas,
                          std::size_t& additions,
                          std::size_t& deletions,
                          std::size_t& contexts) {
        additions = 0;
        deletions = 0;
        contexts = 0;

        for (const auto& delta : deltas) {
            for (const auto& hunk : delta.hunks) {
                for (const auto& line : hunk.active_lines) {
                    switch (line.type) {
                        case ChangeType::ADDITION:
                            ++additions;
                            break;
                        case ChangeType::DELETION:
                            ++deletions;
                            break;
                        case ChangeType::CONTEXT:
                            ++contexts;
                            break;
                    }
                }
            }
        }
    }

    /// Print a human-readable summary to std::ostream for quick inspection.
    static void print_summary(const std::vector<FileDelta>& deltas, std::ostream& os = std::cout) {
        std::size_t additions = 0;
        std::size_t deletions = 0;
        std::size_t contexts = 0;
        summarize(deltas, additions, deletions, contexts);

        os << "Diff summary:\n";
        os << "  Files changed: " << deltas.size() << "\n";
        os << "  Additions:     " << additions << "\n";
        os << "  Deletions:     " << deletions << "\n";
        os << "  Context lines: " << contexts << "\n\n";

        for (const auto& delta : deltas) {
            os << "File: " << delta.origin_file << " -> " << delta.target_file;
            if (delta.is_new_resource) {
                os << " (new resource)";
            }
            os << "\n";

            for (const auto& hunk : delta.hunks) {
                os << "  Hunk: -"
                   << hunk.old_start << "," << hunk.old_count
                   << " +"
                   << hunk.new_start << "," << hunk.new_count
                   << "\n";

                for (const auto& line : hunk.active_lines) {
                    char tag = ' ';
                    switch (line.type) {
                        case ChangeType::CONTEXT:  tag = ' '; break;
                        case ChangeType::ADDITION: tag = '+'; break;
                        case ChangeType::DELETION: tag = '-'; break;
                    }
                    os << "    " << tag << " " << line.payload << "\n";
                }
            }
            os << "\n";
        }
    }

    /// Detect whether a diff touches ALN governance or Prometheus-Praxis safety-critical shards.
    /// This is useful for:
    ///   - Deciding when to run full KER replay in Rust (alncore).
    ///   - Flagging diffs that may require continuity proofs or extra review.
    ///
    /// Heuristic: look for filenames and lines mentioning "PrometheusPraxis", "continuity_proof",
    /// "clinical_kernel", "governance_guard", or "corridor." blocks.
    static bool touches_governance(const std::vector<FileDelta>& deltas) {
        for (const auto& delta : deltas) {
            if (delta.origin_file.find("PrometheusPraxis") != std::string::npos ||
                delta.target_file.find("PrometheusPraxis") != std::string::npos) {
                return true;
            }
            for (const auto& hunk : delta.hunks) {
                for (const auto& line : hunk.active_lines) {
                    const std::string& p = line.payload;
                    if (p.find("prometheuspraxis.continuity_proof") != std::string::npos ||
                        p.find("prometheuspraxis.clinical_kernel") != std::string::npos ||
                        p.find("prometheuspraxis.governance_guard") != std::string::npos ||
                        p.find("prometheuspraxis.rights_risk_guard") != std::string::npos ||
                        p.find("corridor.eco_restoration") != std::string::npos ||
                        p.find("corridor.healthcare_cybernetics") != std::string::npos ||
                        p.find("corridor.smartcity_payments") != std::string::npos) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    /// Basic keyword histogram over payloads.
    /// Collaborators can use this to build their own rules (e.g., to detect ALNv2 spec changes).
    static std::map<std::string, std::size_t> keyword_histogram(
        const std::vector<FileDelta>& deltas,
        const std::vector<std::string>& keywords)
    {
        std::map<std::string, std::size_t> counts;
        for (const auto& key : keywords) {
            counts[key] = 0;
        }

        for (const auto& delta : deltas) {
            for (const auto& hunk : delta.hunks) {
                for (const auto& line : hunk.active_lines) {
                    for (const auto& key : keywords) {
                        if (line.payload.find(key) != std::string::npos) {
                            ++counts[key];
                        }
                    }
                }
            }
        }

        return counts;
    }
};

} // namespace praxis_analysis

#endif // INCLUDED_PROMETHEUS_PRAXIS_DIFF_ANALYZER_HPP
