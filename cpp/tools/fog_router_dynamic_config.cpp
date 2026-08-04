// File: cpp/tools/fog_router_dynamic_config.cpp

#include <string>
#include <stdexcept>
#include <sqlite3.h>

// FOG router thresholds configuration
struct FogRouterConfig {
    double tau1;
    double tau2;
};

class FogRouterConfigStore {
public:
    explicit FogRouterConfigStore(sqlite3* db)
        : db_(db) {
        if (!db_) throw std::runtime_error("SQLite DB pointer must not be null");
        createTable();
    }

    FogRouterConfig loadCurrent() {
        const char* sql =
            "SELECT tau1, tau2 "
            "FROM fog_router_config "
            "ORDER BY updated_utc DESC "
            "LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare fog_router_config query");
        }

        FogRouterConfig cfg{defaultTau1_, defaultTau2_};
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            cfg.tau1 = sqlite3_column_double(stmt, 0);
            cfg.tau2 = sqlite3_column_double(stmt, 1);
        }
        sqlite3_finalize(stmt);
        return cfg;
    }

    void saveNew(const FogRouterConfig& cfg) {
        const char* sql =
            "INSERT INTO fog_router_config (tau1, tau2, updated_utc) "
            "VALUES (?, ?, strftime('%s','now'));";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare fog_router_config insert");
        }

        sqlite3_bind_double(stmt, 1, cfg.tau1);
        sqlite3_bind_double(stmt, 2, cfg.tau2);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to insert fog_router_config");
        }
        sqlite3_finalize(stmt);
    }

private:
    sqlite3* db_;
    double   defaultTau1_ = 0.5;
    double   defaultTau2_ = 0.8;

    void createTable() {
        const char* sql =
            "CREATE TABLE IF NOT EXISTS fog_router_config ("
            " tau1        REAL NOT NULL,"
            " tau2        REAL NOT NULL,"
            " updated_utc INTEGER NOT NULL"
            ");";
        char* errMsg = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string msg = "Failed to create fog_router_config: ";
            if (errMsg) msg += errMsg;
            sqlite3_free(errMsg);
            throw std::runtime_error(msg);
        }
    }
};
