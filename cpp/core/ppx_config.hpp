#ifndef INCLUDED_PROMETHEUS_PRAXIS_CONFIG_HPP
#define INCLUDEDPROMETHEUS_PRAXIS_CONFIG_HPP

// Minimal configuration loader for Prometheus-Praxis.
// Supports simple INI/TOML-like syntax:
//   [section]
//   key = value
//
// Common pattern in C++ utility libraries and config helpers. [web:64][web:69][web:71]

#include <string>
#include <map>
#include <optional>
#include <fstream>
#include <sstream>
#include <cctype>

namespace ppx_config {

struct ConfigValue {
    std::string raw;
};

using SectionMap = std::map<std::string, std::map<std::string, ConfigValue>>;

struct Config {
    SectionMap sections;

    std::optional<ConfigValue> get(const std::string& section,
                                   const std::string& key) const {
        auto it = sections.find(section);
        if (it == sections.end()) return std::nullopt;
        auto kv = it->second.find(key);
        if (kv == it->second.end()) return std::nullopt;
        return kv->second;
    }

    std::string get_or(const std::string& section,
                       const std::string& key,
                       const std::string& def) const {
        auto v = get(section, key);
        if (!v.has_value()) return def;
        return v->raw;
    }
};

inline std::string trim(const std::string& s) {
    std::size_t start = 0;
    while (start < s.size() &&
           std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    std::size_t end = s.size();
    while (end > start &&
           std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

inline bool parse_file(const std::string& path, Config& cfg) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    std::string line;
    std::string current_section = "default";

    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty()) continue;
        if (t[0] == '#' || t[0] == ';') continue;

        if (t.front() == '[' && t.back() == ']') {
            current_section = trim(t.substr(1, t.size() - 2));
            if (current_section.empty()) {
                current_section = "default";
            }
            continue;
        }

        auto pos = t.find('=');
        if (pos == std::string::npos) {
            continue;
        }

        std::string key = trim(t.substr(0, pos));
        std::string val = trim(t.substr(pos + 1));
        cfg.sections[current_section][key] = ConfigValue{val};
    }

    return true;
}

} // namespace ppx_config

#endif // INCLUDEDPROMETHEUS_PRAXIS_CONFIG_HPP
