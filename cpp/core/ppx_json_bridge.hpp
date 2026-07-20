#ifndef INCLUDED_PROMETHEUS_PRAXIS_JSON_BRIDGE_HPP
#define INCLUDED_PROMETHEUS_PRAXIS_JSON_BRIDGE_HPP

// Minimal JSON bridge for Prometheus-Praxis diagnostics.
// Inspired by header-only C++ serialization libraries like alpaca. [web:35]

#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <utility>

namespace ppx_json {

struct LabelKV {
    std::string name;
    std::string value;
};

struct MetricSample {
    std::string name;
    std::vector<LabelKV> labels;
    double value;
    std::string epoch_iso8601;
};

struct DiffLine {
    int old_line_num;
    int new_line_num;
    std::string type;   // "context", "addition", "deletion"
    std::string payload;
};

struct DiffHunk {
    int old_start;
    int old_count;
    int new_start;
    int new_count;
    std::vector<DiffLine> lines;
};

struct FileDelta {
    std::string origin_file;
    std::string target_file;
    bool is_new_resource;
    std::vector<DiffHunk> hunks;
};

struct DiagnosticSnapshot {
    std::vector<MetricSample> metrics;
    std::vector<FileDelta> deltas;
    std::string timestamp_iso8601;
};

// Very simple JSON text builder (not a full JSON parser).
inline std::string escape(const std::string& s) {
    std::ostringstream oss;
    for (char c : s) {
        if (c == '"' || c == '\\') {
            oss << '\\' << c;
        } else if (c == '\n') {
            oss << "\\n";
        } else {
            oss << c;
        }
    }
    return oss.str();
}

inline std::string to_json(const DiagnosticSnapshot& snap) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"timestamp\":\"" << escape(snap.timestamp_iso8601) << "\",";
    oss << "\"metrics\":[";
    for (std::size_t i = 0; i < snap.metrics.size(); ++i) {
        const auto& m = snap.metrics[i];
        oss << "{";
        oss << "\"name\":\"" << escape(m.name) << "\",";
        oss << "\"labels\":[";
        for (std::size_t j = 0; j < m.labels.size(); ++j) {
            const auto& lbl = m.labels[j];
            oss << "{";
            oss << "\"name\":\"" << escape(lbl.name) << "\",";
            oss << "\"value\":\"" << escape(lbl.value) << "\"";
            oss << "}";
            if (j + 1 < m.labels.size()) {
                oss << ",";
            }
        }
        oss << "],";
        oss << "\"value\":" << m.value << ",";
        oss << "\"epoch\":\"" << escape(m.epoch_iso8601) << "\"";
        oss << "}";
        if (i + 1 < snap.metrics.size()) {
            oss << ",";
        }
    }
    oss << "],";
    oss << "\"deltas\":[";
    for (std::size_t i = 0; i < snap.deltas.size(); ++i) {
        const auto& d = snap.deltas[i];
        oss << "{";
        oss << "\"origin_file\":\"" << escape(d.origin_file) << "\",";
        oss << "\"target_file\":\"" << escape(d.target_file) << "\",";
        oss << "\"is_new_resource\":" << (d.is_new_resource ? "true" : "false") << ",";
        oss << "\"hunks\":[";
        for (std::size_t h = 0; h < d.hunks.size(); ++h) {
            const auto& hk = d.hunks[h];
            oss << "{";
            oss << "\"old_start\":" << hk.old_start << ",";
            oss << "\"old_count\":" << hk.old_count << ",";
            oss << "\"new_start\":" << hk.new_start << ",";
            oss << "\"new_count\":" << hk.new_count << ",";
            oss << "\"lines\":[";
            for (std::size_t ln = 0; ln < hk.lines.size(); ++ln) {
                const auto& l = hk.lines[ln];
                oss << "{";
                oss << "\"old_line_num\":" << l.old_line_num << ",";
                oss << "\"new_line_num\":" << l.new_line_num << ",";
                oss << "\"type\":\"" << escape(l.type) << "\",";
                oss << "\"payload\":\"" << escape(l.payload) << "\"";
                oss << "}";
                if (ln + 1 < hk.lines.size()) {
                    oss << ",";
                }
            }
            oss << "]";
            oss << "}";
            if (h + 1 < d.hunks.size()) {
                oss << ",";
            }
        }
        oss << "]";
        oss << "}";
        if (i + 1 < snap.deltas.size()) {
            oss << ",";
        }
    }
    oss << "]";
    oss << "}";
    return oss.str();
}

} // namespace ppx_json

#endif // INCLUDEDPROMETHEUS_PRAXIS_JSON_BRIDGE_HPP
