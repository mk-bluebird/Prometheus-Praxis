// File: cpp/tools/spatiotemporal_transformer_embeddings.cpp

#include <string>
#include <vector>
#include <stdexcept>
#include <sqlite3.h>

// Embedding record for a hex-time pair
struct TelemetryEmbedding {
    std::string h3_index;
    long        ts_utc;
    std::vector<float> embedding; // transformer hidden state
};

class EmbeddingStore {
public:
    explicit EmbeddingStore(sqlite3* db, int dim)
        : db_(db), dim_(dim) {
        if (!db_) throw std::runtime_error("SQLite DB pointer must not be null");
        createTable();
    }

    void insertEmbedding(const TelemetryEmbedding& emb) {
        const char* sql =
            "INSERT INTO telemetry_embeddings "
            " (h3_index, ts_utc, dim, embedding_blob, created_utc) "
            "VALUES (?, ?, ?, ?, strftime('%s','now'));";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare telemetry_embeddings insert");
        }

        sqlite3_bind_text(stmt, 1, emb.h3_index.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, emb.ts_utc);
        sqlite3_bind_int(stmt,    3, dim_);

        // Serialize embedding vector to BLOB
        std::string blob;
        blob.resize(dim_ * sizeof(float));
        std::memcpy(&blob[0], emb.embedding.data(), dim_ * sizeof(float));

        sqlite3_bind_blob(stmt, 4, blob.data(), static_cast<int>(blob.size()), SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to insert telemetry embedding");
        }

        sqlite3_finalize(stmt);
    }

private:
    sqlite3* db_;
    int      dim_;

    void createTable() {
        const char* sql =
            "CREATE TABLE IF NOT EXISTS telemetry_embeddings ("
            " h3_index      TEXT NOT NULL,"
            " ts_utc        INTEGER NOT NULL,"
            " dim           INTEGER NOT NULL,"
            " embedding_blob BLOB NOT NULL,"
            " created_utc   INTEGER NOT NULL,"
            " PRIMARY KEY (h3_index, ts_utc)"
            ");";
        char* errMsg = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string msg = "Failed to create telemetry_embeddings: ";
            if (errMsg) msg += errMsg;
            sqlite3_free(errMsg);
            throw std::runtime_error(msg);
        }
    }
};
