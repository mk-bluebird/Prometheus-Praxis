// File: cpp/eco_restoration/pfas_model_registry.cpp

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <sqlite3.h>

namespace prometheus_praxis {
namespace eco_restoration {

struct PFASModelMeta {
    int    model_id;
    std::string segment_id;
    std::string architecture;
    std::string parent_model_id;
    std::string version_tag;
    std::string created_at;
};

class PFASModelRegistry {
public:
    explicit PFASModelRegistry(const std::string& db_path)
        : db_path_(db_path), db_(nullptr) {
        open_db();
        install_schema();
    }

    ~PFASModelRegistry() {
        if (db_) sqlite3_close(db_);
    }

    int store_model(const std::string& segment_id,
                    const std::string& architecture,
                    const std::string& parent_model_id,
                    const std::string& version_tag,
                    const std::vector<unsigned char>& model_blob) {
        const char* sql =
            "INSERT INTO pfas_model_registry("
            "  segment_id, architecture, parent_model_id, version_tag, model_blob, created_at"
            ") VALUES(?, ?, ?, ?, ?, datetime('now'));";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Prepare insert pfas_model_registry failed: " +
                                     std::string(sqlite3_errmsg(db_)));
        }

        sqlite3_bind_text(stmt, 1, segment_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, architecture.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, parent_model_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, version_tag.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_blob(stmt, 5, model_blob.data(),
                          static_cast<int>(model_blob.size()), SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Insert pfas_model_registry step failed: " +
                                     std::string(sqlite3_errmsg(db_)));
        }
        sqlite3_finalize(stmt);

        int model_id = static_cast<int>(sqlite3_last_insert_rowid(db_));
        return model_id;
    }

    std::vector<unsigned char> load_model_blob(int model_id) {
        const char* sql =
            "SELECT model_blob FROM pfas_model_registry WHERE model_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Prepare select pfas_model_registry failed: " +
                                     std::string(sqlite3_errmsg(db_)));
        }

        sqlite3_bind_int(stmt, 1, model_id);

        rc = sqlite3_step(stmt);
        std::vector<unsigned char> blob;
        if (rc == SQLITE_ROW) {
            const void* data = sqlite3_column_blob(stmt, 0);
            int size = sqlite3_column_bytes(stmt, 0);
            blob.assign(static_cast<const unsigned char*>(data),
                        static_cast<const unsigned char*>(data) + size);
        }
        sqlite3_finalize(stmt);

        if (blob.empty()) {
            throw std::runtime_error("Model blob not found for model_id=" + std::to_string(model_id));
        }
        return blob;
    }

    PFASModelMeta load_model_meta(int model_id) {
        const char* sql =
            "SELECT model_id, segment_id, architecture, parent_model_id, version_tag, created_at "
            "FROM pfas_model_registry WHERE model_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Prepare select meta failed: " +
                                     std::string(sqlite3_errmsg(db_)));
        }

        sqlite3_bind_int(stmt, 1, model_id);

        PFASModelMeta meta{};
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            meta.model_id       = sqlite3_column_int(stmt, 0);
            meta.segment_id     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            meta.architecture   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            meta.parent_model_id= reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            meta.version_tag    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            meta.created_at     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        }
        sqlite3_finalize(stmt);
        return meta;
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
        const char* sql_registry =
            "CREATE TABLE IF NOT EXISTS pfas_model_registry ("
            "  model_id      INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  segment_id    TEXT NOT NULL,"
            "  architecture  TEXT NOT NULL,"
            "  parent_model_id TEXT,"
            "  version_tag   TEXT NOT NULL,"
            "  model_blob    BLOB NOT NULL,"
            "  created_at    TEXT NOT NULL"
            ");";
        exec_sql(sql_registry);

        const char* sql_invariants =
            "CREATE TABLE IF NOT EXISTS pfas_model_invariants ("
            "  model_id      INTEGER PRIMARY KEY,"
            "  lyap_monotone INTEGER NOT NULL,"
            "  last_checked  TEXT NOT NULL,"
            "  FOREIGN KEY(model_id) REFERENCES pfas_model_registry(model_id)"
            ");";
        exec_sql(sql_invariants);
    }
};

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "pfas_model_registry.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    try {
        PFASModelRegistry registry(db_path);

        std::vector<unsigned char> dummy_blob = {0x01, 0x02, 0x03};
        int id = registry.store_model(
            "segment_A",
            "LSTM_encoder_decoder",
            "",
            "v1.0",
            dummy_blob
        );

        PFASModelMeta meta = registry.load_model_meta(id);
        std::cout << "Stored PFAS model_id=" << meta.model_id
                  << " segment=" << meta.segment_id
                  << " arch=" << meta.architecture
                  << " version=" << meta.version_tag << std::endl;

        std::vector<unsigned char> blob = registry.load_model_blob(id);
        std::cout << "Loaded PFAS model blob of size=" << blob.size() << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "PFASModelRegistry error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
