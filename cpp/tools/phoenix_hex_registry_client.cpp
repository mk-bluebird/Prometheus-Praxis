// File: cpp/tools/phoenix_hex_registry_client.cpp
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <sqlite3.h>

namespace phoenix_hex {

struct HexRisk {
    std::string hex_id;
    double r_hydraulics;
    double r_energy;
    double r_topology;
    double r_biodiversity;
    double Vt; // Lyapunov residual across planes
};

class HexRegistryClient {
public:
    explicit HexRegistryClient(const std::string& db_path)
        : db_path_(db_path), db_(nullptr) {
        open();
    }

    ~HexRegistryClient() {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    std::vector<HexRisk> load_hex_risks() const {
        std::vector<HexRisk> out;

        const char* sql =
            "SELECT hex_id, "
            "       r_hydraulics, r_energy, r_topology, r_biodiversity, "
            "       w_h, w_e, w_t, w_b "
            "  FROM phoenix_hex_registry;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare phoenix_hex_registry query");
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            HexRisk hr{};
            const unsigned char* hex = sqlite3_column_text(stmt, 0);
            hr.hex_id = hex ? reinterpret_cast<const char*>(hex) : "";

            hr.r_hydraulics  = sqlite3_column_double(stmt, 1);
            hr.r_energy      = sqlite3_column_double(stmt, 2);
            hr.r_topology    = sqlite3_column_double(stmt, 3);
            hr.r_biodiversity= sqlite3_column_double(stmt, 4);

            double w_h = sqlite3_column_double(stmt, 5);
            double w_e = sqlite3_column_double(stmt, 6);
            double w_t = sqlite3_column_double(stmt, 7);
            double w_b = sqlite3_column_double(stmt, 8);

            // Lyapunov residual V_t = sum w_j r_j^2, matching governance math.[59]
            hr.Vt = w_h * hr.r_hydraulics  * hr.r_hydraulics
                  + w_e * hr.r_energy      * hr.r_energy
                  + w_t * hr.r_topology    * hr.r_topology
                  + w_b * hr.r_biodiversity* hr.r_biodiversity;

            out.push_back(hr);
        }

        sqlite3_finalize(stmt);
        return out;
    }

    void print_hex_risks() const {
        auto hexes = load_hex_risks();
        for (const auto& h : hexes) {
            std::cout << h.hex_id
                      << " r_hyd=" << h.r_hydraulics
                      << " r_energy=" << h.r_energy
                      << " r_top=" << h.r_topology
                      << " r_bio=" << h.r_biodiversity
                      << " Vt=" << h.Vt
                      << "\n";
        }
    }

private:
    void open() {
        if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) {
            throw std::runtime_error("Failed to open SQLite DB: " + db_path_);
        }
        sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    }

    std::string db_path_;
    sqlite3* db_;
};

} // namespace phoenix_hex

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: phoenix_hex_registry_client <phoenix_db_path>\n";
        return 1;
    }
    try {
        phoenix_hex::HexRegistryClient client(argv[1]);
        client.print_hex_risks();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
