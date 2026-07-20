// filename: cpp/diff/ppx_diff_analyzer.hpp
// destination: github.com/mk-bluebird/Prometheus-Praxis/cpp/diff/ppx_diff_analyzer.hpp
//
// Unified diff analyzer for local agent workflows.
// Parses "git diff --unified" style patches into structured hunks and lines.

#ifndef INCLUDED_PROMETHEUS_PRAXIS_DIFF_ANALYZER_HPP
#define INCLUDEDPROMETHEUS_PRAXIS_DIFF_ANALYZER_HPP

#include <string>
#include <vector>
#include <sstream>
#include <regex>

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
                    new_hunk.old_count = match_results[2].str().empty()
                        ? 1
                        : std::stoi(match_results[2].str());
                    new_hunk.new_start = std::stoi(match_results[3].str());
                    new_hunk.new_count = match_results[4].str().empty()
                        ? 1
                        : std::stoi(match_results[4].str());
                    active_delta->hunks.push_back(std::move(new_hunk));
                }
            } else if (active_delta && !active_delta->hunks.empty()) {
                auto& current_hunk = active_delta->hunks.back();
                if (current_line.empty()) {
                    current_hunk.active_lines.push_back({
                        current_hunk.old_start++,
                        current_hunk.new_start++,
                        ChangeType::CONTEXT,
                        ""
                    });
                } else {
                    char prefix = current_line[0];
                    std::string payload = current_line.substr(1);
                    if (prefix == ' ') {
                        current_hunk.active_lines.push_back({
                            current_hunk.old_start++,
                            current_hunk.new_start++,
                            ChangeType::CONTEXT,
                            payload
                        });
                    } else if (prefix == '+') {
                        current_hunk.active_lines.push_back({
                            -1,
                            current_hunk.new_start++,
                            ChangeType::ADDITION,
                            payload
                        });
                    } else if (prefix == '-') {
                        current_hunk.active_lines.push_back({
                            current_hunk.old_start++,
                            -1,
                            ChangeType::DELETION,
                            payload
                        });
                    } else {
                        current_hunk.active_lines.push_back({
                            current_hunk.old_start++,
                            current_hunk.new_start++,
                            ChangeType::CONTEXT,
                            current_line
                        });
                    }
                }
            }
        }

        return deltas;
    }
};

} // namespace praxis_analysis

#endif // INCLUDED_PROMETHEUS_PRAXIS_DIFF_ANALYZER_HPP
