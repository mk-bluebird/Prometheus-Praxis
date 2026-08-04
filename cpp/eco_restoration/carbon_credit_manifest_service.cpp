// File: cpp/eco_restoration/carbon_credit_manifest_service.cpp

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <sqlite3.h>

namespace prometheus_praxis {
namespace eco_restoration {

struct ManifestEntry {
    std::string table_name;
    std::string row_digest;
};

struct CarbonCreditManifest {
    int manifest_id;
    std::string snapshot_ts;
    std::string did;
    std::string merkle_root;
    std::string legal_metadata;
};

class CarbonCreditManifestService {
public:
    explicit CarbonCreditManifestService(const std::string& db_path)
        : db_path_(db_path), db_(nullptr) {
        open_db();
        install_schema();
    }

    ~CarbonCreditManifestService() {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    int build_and_store_manifest(const std::string& did,
                                 const std::string& legal_metadata) {
        std::string snapshot_ts = current_timestamp();
        std::vector<ManifestEntry> entries = collect_table_digests(snapshot_ts);
        std::string merkle_root = compute_merkle_root(entries);

        const char* sql =
            "INSERT INTO carbon_credit_manifest("
            "  snapshot_ts, did, merkle_root, legal_metadata, created_at"
            ") VALUES(?, ?, ?, ?, datetime('now'));";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Prepare insert carbon_credit_manifest failed: " +
                                     std::string(sqlite3_errmsg(db_)));
        }

        sqlite3_bind_text(stmt, 1, snapshot_ts.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, did.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, merkle_root.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, legal_metadata.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Insert carbon_credit_manifest step failed: " +
                                     std::string(sqlite3_errmsg(db_)));
        }
        sqlite3_finalize(stmt);

        int manifest_id = static_cast<int>(sqlite3_last_insert_rowid(db_));
        store_manifest_entries(manifest_id, entries);
        return manifest_id;
    }

    CarbonCreditManifest load_manifest(int manifest_id) {
        const char* sql =
            "SELECT manifest_id, snapshot_ts, did, merkle_root, legal_metadata "
            "FROM carbon_credit_manifest WHERE manifest_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Prepare select carbon_credit_manifest failed: " +
                                     std::string(sqlite3_errmsg(db_)));
        }

        sqlite3_bind_int(stmt, 1, manifest_id);

        CarbonCreditManifest m{};
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            m.manifest_id   = sqlite3_column_int(stmt, 0);
            m.snapshot_ts   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            m.did           = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            m.merkle_root   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            m.legal_metadata= reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        }
        sqlite3_finalize(stmt);
        return m;
    }

private:
    std::string db_path_;
    sqlite3* db_;

    void open_db() {
        int rc = sqlite3_open(db_path_.c_str(), &db_);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db_);
            sqlite3_close(db_);
            db_ = nullptr;
            throw std::runtime_error("Cannot open DB: " + msg);
        }
    }

    void exec_sql(const std::string& sql) {
        char* errmsg = nullptr;
        int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            throw std::runtime_error("SQLite error: " + msg);
        }
    }

    void install_schema() {
        const char* sql_manifest =
            "CREATE TABLE IF NOT EXISTS carbon_credit_manifest ("
            "  manifest_id   INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  snapshot_ts   TEXT NOT NULL,"
            "  did           TEXT NOT NULL,"
            "  merkle_root   TEXT NOT NULL,"
            "  legal_metadata TEXT NOT NULL,"
            "  created_at    TEXT NOT NULL"
            ");";
        exec_sql(sql_manifest);

        const char* sql_entries =
            "CREATE TABLE IF NOT EXISTS carbon_credit_manifest_entry ("
            "  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  manifest_id   INTEGER NOT NULL,"
            "  table_name    TEXT NOT NULL,"
            "  row_digest    TEXT NOT NULL,"
            "  FOREIGN KEY(manifest_id) REFERENCES carbon_credit_manifest(manifest_id)"
            ");";
        exec_sql(sql_entries);
    }

    std::string current_timestamp() const {
        const char* sql = "SELECT datetime('now');";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        std::string ts = "";
        if (rc == SQLITE_OK) {
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                ts = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            }
        }
        sqlite3_finalize(stmt);
        return ts;
    }

    std::vector<std::string> eco_tables() const {
        std::vector<std::string> tables = {
            "hex_restoration_commitment",
            "ker_e",
            "ker_r",
            "hex_thermal_recovery",
            "ker_carbon_reachability_grid"
        };
        return tables;
    }

    std::string table_row_digest(const std::string& table_name,
                                 const std::string& snapshot_ts) {
        const char* sql_base =
            "SELECT group_concat(row_text, '|') "
            "FROM (SELECT printf('%s:%s', rowid, json) AS row_text FROM %s_snapshot WHERE snapshot_ts = ?);";
        std::string sql(sql_base);
        std::size_t pos = sql.find("%s_snapshot");
        if (pos != std::string::npos) {
            sql.replace(pos, 11, table_name + "_snapshot");
        }

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return "";
        }

        sqlite3_bind_text(stmt, 1, snapshot_ts.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        std::string concat_rows;
        if (rc == SQLITE_ROW) {
            const char* txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (txt) concat_rows = txt;
        }
        sqlite3_finalize(stmt);

        return concat_rows;
    }

    std::vector<ManifestEntry> collect_table_digests(const std::string& snapshot_ts) {
        std::vector<ManifestEntry> entries;
        for (const auto& tbl : eco_tables()) {
            ManifestEntry e;
            e.table_name = tbl;
            e.row_digest = table_row_digest(tbl, snapshot_ts);
            entries.push_back(e);
        }
        return entries;
    }

    std::string compute_merkle_root(const std::vector<ManifestEntry>& entries) const {
        if (entries.empty()) return "";

        std::vector<std::string> level;
        for (const auto& e : entries) {
            level.push_back(e.table_name + ":" + e.row_digest);
        }

        while (level.size() > 1) {
            std::vector<std::string> next;
            for (std::size_t i = 0; i < level.size(); i += 2) {
                if (i + 1 < level.size()) {
                    std::string combined = level[i] + "|" + level[i + 1];
                    next.push_back(combined);
                } else {
                    next.push_back(level[i]);
                }
            }
            level.swap(next);
        }

        return level[0];
    }

    void store_manifest_entries(int manifest_id,
                                const std::vector<ManifestEntry>& entries) {
        const char* sql =
            "INSERT INTO carbon_credit_manifest_entry(manifest_id, table_name, row_digest) "
            "VALUES(?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return;
        }

        for (const auto& e : entries) {
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
            sqlite3_bind_int(stmt, 1, manifest_id);
            sqlite3_bind_text(stmt, 2, e.table_name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, e.row_digest.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
    }
};

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "carbon_credit.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    try {
        CarbonCreditManifestService service(db_path);
        int manifest_id = service.build_and_store_manifest(
            "did:example:governance",
            "Carbon credit manifest for annual eco-restoration snapshot"
        );

        CarbonCreditManifest m = service.load_manifest(manifest_id);
        std::cout << "Stored manifest_id=" << m.manifest_id
                  << " snapshot_ts=" << m.snapshot_ts
                  << " did=" << m.did
                  << " merkle_root=" << m.merkle_root << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "CarbonCreditManifestService error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
