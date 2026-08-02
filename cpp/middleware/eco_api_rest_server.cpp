// File: cpp/middleware/eco_api_rest_server.cpp
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <iomanip>

// eco_api_rest_server:
// - Simulates a REST endpoint that returns a JSON eco-report for a requested hex anchor ID.
// - Aggregates data from a local store and real-time sensor snapshots.
// - In real deployment, this would use Drogon or Pistache; here we provide a
//   self-contained C++ HTTP-like handler without external frameworks.

namespace eco {

struct HexEcoRecord {
    std::string hex_id;
    double eco_composite;
    double soil_moisture_pct;
    double water_quality_index;
    double canopy_fraction;
    double heat_island_intensity;
};

class LocalEcoStore {
public:
    LocalEcoStore() {
        // Seed with example data; in reality, load from SQLite and sensor feeds.
        add_record({"PHX-HX-0-0", 0.78, 34.0, 0.82, 0.21, 1.5});
        add_record({"PHX-HX-1-0", 0.65, 28.0, 0.76, 0.18, 2.0});
        add_record({"PHX-HX-0-1", 0.59, 40.0, 0.70, 0.12, 2.4});
    }

    const HexEcoRecord* get(const std::string& hex_id) const {
        auto it = store_.find(hex_id);
        if (it == store_.end()) return nullptr;
        return &it->second;
    }

private:
    std::unordered_map<std::string, HexEcoRecord> store_;

    void add_record(const HexEcoRecord& r) {
        store_[r.hex_id] = r;
    }
};

class EcoApiRestServer {
public:
    EcoApiRestServer()
        : store_() {}

    // Simulated HTTP GET handler: /eco_report?hex_id=PHX-HX-0-0
    std::string handle_get(const std::string& query) {
        std::string hex_id = parse_hex_id(query);
        if (hex_id.empty()) {
            return build_error("Missing or invalid hex_id parameter.");
        }

        const HexEcoRecord* rec = store_.get(hex_id);
        if (!rec) {
            return build_error("Hex anchor ID not found.");
        }

        return build_response(*rec);
    }

private:
    LocalEcoStore store_;

    static std::string parse_hex_id(const std::string& query) {
        // Very simple query parser for "hex_id=..."
        std::size_t pos = query.find("hex_id=");
        if (pos == std::string::npos) return "";
        pos += 7;
        std::size_t end = query.find('&', pos);
        if (end == std::string::npos) end = query.size();
        return query.substr(pos, end - pos);
    }

    static std::string build_error(const std::string& msg) {
        std::ostringstream oss;
        oss << "{"
            << "\"status\":\"error\","
            << "\"message\":\"" << msg << "\""
            << "}";
        return oss.str();
    }

    static std::string build_response(const HexEcoRecord& r) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);
        oss << "{"
            << "\"status\":\"ok\","
            << "\"hex_id\":\"" << r.hex_id << "\","
            << "\"eco_composite\":" << r.eco_composite << ","
            << "\"soil_moisture_pct\":" << r.soil_moisture_pct << ","
            << "\"water_quality_index\":" << r.water_quality_index << ","
            << "\"canopy_fraction\":" << r.canopy_fraction << ","
            << "\"heat_island_intensity\":" << r.heat_island_intensity
            << "}";
        return oss.str();
    }
};

} // namespace eco

int main() {
    using namespace eco;

    EcoApiRestServer server;

    // Simulate incoming REST queries.
    std::string q1 = "hex_id=PHX-HX-0-0";
    std::string q2 = "hex_id=PHX-HX-2-2";

    std::cout << "GET /eco_report?" << q1 << "\n";
    std::cout << server.handle_get(q1) << "\n\n";

    std::cout << "GET /eco_report?" << q2 << "\n";
    std::cout << server.handle_get(q2) << "\n";

    return 0;
}
