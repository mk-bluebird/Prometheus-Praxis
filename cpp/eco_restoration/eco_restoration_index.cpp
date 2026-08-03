// File: cpp/eco_restoration/eco_restoration_index.cpp
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <sqlite3.h>

namespace eco_restoration {

struct ModuleInfo {
    std::string relpath;
    std::string summary;
};

class EcoRestorationIndex {
public:
    explicit EcoRestorationIndex(const std::string& dbPath)
        : dbPath_(dbPath), db_(nullptr) {
        openDatabase();
    }

    ~EcoRestorationIndex() {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    // Registry function: return all eco-restoration C++ modules known to the MCP file index.
    std::vector<ModuleInfo> get_cpp_modules() const {
        std::vector<ModuleInfo> modules;

        const char* sql =
            "SELECT relpath, description "
            "FROM mcp_file "
            "JOIN mcp_repo ON mcp_file.repoid = mcp_repo.repoid "
            "WHERE mcp_repo.reponame = 'eco_restoration_shard' "
            "  AND mcp_file.filekind = 'CPP' "
            "  AND mcp_file.active = 1;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare mcp_file query");
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* relpath = sqlite3_column_text(stmt, 0);
            const unsigned char* desc    = sqlite3_column_text(stmt, 1);

            ModuleInfo info;
            info.relpath = relpath ? reinterpret_cast<const char*>(relpath) : "";
            info.summary = desc    ? reinterpret_cast<const char*>(desc)    : "";

            modules.push_back(info);
        }

        sqlite3_finalize(stmt);
        return modules;
    }

    // Helper to print modules for CLI use by agents or tooling.
    void print_cpp_modules() const {
        auto modules = get_cpp_modules();
        for (const auto& m : modules) {
            std::cout << m.relpath << " :: " << m.summary << "\n";
        }
    }

private:
    void openDatabase() {
        if (sqlite3_open(dbPath_.c_str(), &db_) != SQLITE_OK) {
            std::string msg = "Failed to open SQLite MCP index at " + dbPath_;
            throw std::runtime_error(msg);
        }
        // Enforce foreign keys to match EcoNet MCP schema.
        sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    }

    std::string dbPath_;
    sqlite3* db_;
};

} // namespace eco_restoration

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: eco_restoration_index <mcp_index_db_path>\n";
        return 1;
    }

    try {
        eco_restoration::EcoRestorationIndex index(argv[1]);
        index.print_cpp_modules();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
