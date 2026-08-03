// File: cpp/tools/eco_config_loader.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cctype>
#include <algorithm>

namespace eco_config {

struct CanalNodeConfig {
    std::string node_code;
    std::string description;
    std::string ker_band;      // RESEARCH / EXPPROD / PROD
    std::string fog_band;      // FOG:COLD_SURVIVAL_MONITOR / FOG:RESTORATION_PREFERRED / FOG:NEEDS_DIAGNOSTIC
    std::string canal_plane;   // HYDRAULICS / ENERGY / TOPOLOGY / BIODIVERSITY
};

struct HexAnchorConfig {
    std::string hex_id;
    std::string domain;
    std::string subdomain;
    std::string owner_did;
};

struct WorkloadCorridorConfig {
    double max_energy_J;
    double max_deltaVt;
    double w_energy;
    double w_topology;
};

class EcoConfigLoader {
public:
    explicit EcoConfigLoader(const std::string& path)
        : path_(path) {}

    std::vector<CanalNodeConfig> load_canal_nodes() const {
        std::string content = read_file();
        return parse_canal_nodes(content);
    }

    std::vector<HexAnchorConfig> load_hex_anchors() const {
        std::string content = read_file();
        return parse_hex_anchors(content);
    }

    WorkloadCorridorConfig load_workload_corridor() const {
        std::string content = read_file();
        return parse_workload_corridor(content);
    }

    void print_flat_kv(const std::string& conf_path) const {
        std::ifstream file(conf_path);
        if (!file.is_open()) {
            std::cerr << "Cannot open config: " << conf_path << "\n";
            return;
        }
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::size_t pos = line.find('=');
            if (pos == std::string::npos) continue;
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            std::cout << key << " = " << value << "\n";
        }
    }

private:
    std::string path_;

    std::string read_file() const {
        std::ifstream in(path_);
        if (!in) {
            throw std::runtime_error("Failed to open config file: " + path_);
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    static std::string trim(const std::string& s) {
        std::size_t start = 0;
        while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
            ++start;
        }
        std::size_t end = s.size();
        while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
            --end;
        }
        return s.substr(start, end - start);
    }

    static std::string get_value(const std::string& content, const std::string& key) {
        std::string pattern = "\"" + key + "\"";
        std::size_t pos = content.find(pattern);
        if (pos == std::string::npos) return "";
        pos = content.find(':', pos);
        if (pos == std::string::npos) return "";
        ++pos;
        while (pos < content.size() && std::isspace(static_cast<unsigned char>(content[pos]))) {
            ++pos;
        }
        if (pos < content.size() && content[pos] == '"') {
            ++pos;
            std::size_t end = content.find('"', pos);
            if (end == std::string::npos) return "";
            return content.substr(pos, end - pos);
        } else {
            std::size_t end = pos;
            while (end < content.size() &&
                   !std::isspace(static_cast<unsigned char>(content[end])) &&
                   content[end] != ',' && content[end] != '}') {
                ++end;
            }
            return trim(content.substr(pos, end - pos));
        }
    }

    static double get_double(const std::string& content, const std::string& key) {
        std::string v = get_value(content, key);
        if (v.empty()) return 0.0;
        return std::stod(v);
    }

    static std::vector<CanalNodeConfig> parse_canal_nodes(const std::string& content) {
        std::vector<CanalNodeConfig> nodes;
        std::string key = "\"canal_nodes\"";
        std::size_t pos = content.find(key);
        if (pos == std::string::npos) return nodes;
        pos = content.find('[', pos);
        if (pos == std::string::npos) return nodes;
        std::size_t end = content.find(']', pos);
        if (end == std::string::npos) return nodes;
        std::string array = content.substr(pos + 1, end - pos - 1);

        std::size_t cur = 0;
        while (cur < array.size()) {
            std::size_t obj_start = array.find('{', cur);
            if (obj_start == std::string::npos) break;
            std::size_t obj_end = array.find('}', obj_start);
            if (obj_end == std::string::npos) break;
            std::string obj = array.substr(obj_start, obj_end - obj_start + 1);

            CanalNodeConfig cfg;
            cfg.node_code    = get_value(obj, "node_code");
            cfg.description  = get_value(obj, "description");
            cfg.ker_band     = get_value(obj, "ker_band");
            cfg.fog_band     = get_value(obj, "fog_band");
            cfg.canal_plane  = get_value(obj, "canal_plane");

            nodes.push_back(cfg);
            cur = obj_end + 1;
        }

        return nodes;
    }

    static std::vector<HexAnchorConfig> parse_hex_anchors(const std::string& content) {
        std::vector<HexAnchorConfig> anchors;
        std::string key = "\"hex_anchors\"";
        std::size_t pos = content.find(key);
        if (pos == std::string::npos) return anchors;
        pos = content.find('[', pos);
        if (pos == std::string::npos) return anchors;
        std::size_t end = content.find(']', pos);
        if (end == std::string::npos) return anchors;
        std::string array = content.substr(pos + 1, end - pos - 1);

        std::size_t cur = 0;
        while (cur < array.size()) {
            std::size_t obj_start = array.find('{', cur);
            if (obj_start == std::string::npos) break;
            std::size_t obj_end = array.find('}', obj_start);
            if (obj_end == std::string::npos) break;
            std::string obj = array.substr(obj_start, obj_end - obj_start + 1);

            HexAnchorConfig cfg;
            cfg.hex_id     = get_value(obj, "hex_id");
            cfg.domain     = get_value(obj, "domain");
            cfg.subdomain  = get_value(obj, "subdomain");
            cfg.owner_did  = get_value(obj, "owner_did");

            anchors.push_back(cfg);
            cur = obj_end + 1;
        }

        return anchors;
    }

    static WorkloadCorridorConfig parse_workload_corridor(const std::string& content) {
        WorkloadCorridorConfig cfg{};
        cfg.max_energy_J  = get_double(content, "max_energy_J");
        cfg.max_deltaVt   = get_double(content, "max_deltaVt");
        cfg.w_energy      = get_double(content, "w_energy");
        cfg.w_topology    = get_double(content, "w_topology");
        return cfg;
    }
};

} // namespace eco_config

int main() {
    try {
        eco_config::EcoConfigLoader loader("eco_config.json");
        auto canal_nodes = loader.load_canal_nodes();
        auto hex_anchors = loader.load_hex_anchors();
        auto corridor    = loader.load_workload_corridor();

        std::cout << "Loaded " << canal_nodes.size() << " canal_nodes\n";
        std::cout << "Loaded " << hex_anchors.size() << " hex_anchors\n";
        std::cout << "Workload corridor max_energy_J=" << corridor.max_energy_J
                  << " max_deltaVt=" << corridor.max_deltaVt << "\n";

        loader.print_flat_kv("eco.conf");
    } catch (const std::exception& ex) {
        std::cerr << "eco_config_loader error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
