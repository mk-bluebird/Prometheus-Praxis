// File: cpp/tools/quic_telemetry_ingest_core.cpp

#include <openssl/evp.h>
#include <sqlite3.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace eco_restoration {

struct TelemetryFrame {
    std::string frame_id;
    std::string owner_did;
    std::string canonical_payload;
    std::vector<unsigned char> signature;
};

class TelemetryIngestCore {
public:
    explicit TelemetryIngestCore(sqlite3* database) : database_(database) {
        if (database_ == nullptr) throw std::invalid_argument("SQLite database is required");
        sqlite3_exec(database_,
            "CREATE TABLE IF NOT EXISTS did_public_key("
            "owner_did TEXT PRIMARY KEY,public_key BLOB NOT NULL,active INTEGER NOT NULL CHECK(active IN (0,1))"
            ") STRICT;"
            "CREATE TABLE IF NOT EXISTS verified_telemetry("
            "frame_id TEXT PRIMARY KEY,owner_did TEXT NOT NULL,canonical_payload TEXT NOT NULL,"
            "received_unix_s INTEGER NOT NULL,state TEXT NOT NULL CHECK(state IN ('VERIFIED','REJECTED'))"
            ") STRICT;",
            nullptr, nullptr, nullptr);
    }

    bool accept_authenticated_frame(const TelemetryFrame& frame, std::int64_t received_unix_s) {
        if (frame.frame_id.empty() || frame.owner_did.empty() ||
            frame.canonical_payload.empty() || frame.signature.empty()) {
            persist(frame, received_unix_s, "REJECTED");
            return false;
        }

        const std::vector<unsigned char> public_key = active_key(frame.owner_did);
        const bool valid = verify(frame.canonical_payload, frame.signature, public_key);
        persist(frame, received_unix_s, valid ? "VERIFIED" : "REJECTED");
        return valid;
    }

private:
    std::vector<unsigned char> active_key(std::string_view owner_did) const {
        sqlite3_stmt* statement = nullptr;
        sqlite3_prepare_v2(database_,
            "SELECT public_key FROM did_public_key WHERE owner_did=? AND active=1;",
            -1, &statement, nullptr);
        sqlite3_bind_text(statement, 1, owner_did.data(), static_cast<int>(owner_did.size()), SQLITE_TRANSIENT);

        std::vector<unsigned char> key;
        if (sqlite3_step(statement) == SQLITE_ROW) {
            const auto* bytes = static_cast<const unsigned char*>(sqlite3_column_blob(statement, 0));
            const int size = sqlite3_column_bytes(statement, 0);
            if (bytes != nullptr && size == 32) key.assign(bytes, bytes + size);
        }
        sqlite3_finalize(statement);
        return key;
    }

    static bool verify(
        std::string_view payload,
        const std::vector<unsigned char>& signature,
        const std::vector<unsigned char>& public_key) {

        if (public_key.size() != 32U || signature.size() != 64U) return false;

        EVP_PKEY* key = EVP_PKEY_new_raw_public_key(
            EVP_PKEY_ED25519, nullptr, public_key.data(), public_key.size());
        if (key == nullptr) return false;

        EVP_MD_CTX* context = EVP_MD_CTX_new();
        if (context == nullptr) {
            EVP_PKEY_free(key);
            return false;
        }

        const int initialized = EVP_DigestVerifyInit(context, nullptr, nullptr, nullptr, key);
        const int verified = initialized == 1
            ? EVP_DigestVerify(context, signature.data(), signature.size(),
                               reinterpret_cast<const unsigned char*>(payload.data()), payload.size())
            : 0;

        EVP_MD_CTX_free(context);
        EVP_PKEY_free(key);
        return verified == 1;
    }

    void persist(const TelemetryFrame& frame, std::int64_t received_unix_s, std::string_view state) const {
        sqlite3_stmt* statement = nullptr;
        sqlite3_prepare_v2(database_,
            "INSERT INTO verified_telemetry VALUES(?,?,?,?,?) "
            "ON CONFLICT(frame_id) DO NOTHING;",
            -1, &statement, nullptr);
        sqlite3_bind_text(statement, 1, frame.frame_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, frame.owner_did.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 3, frame.canonical_payload.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(statement, 4, static_cast<sqlite3_int64>(received_unix_s));
        sqlite3_bind_text(statement, 5, state.data(), static_cast<int>(state.size()), SQLITE_TRANSIENT);

        if (sqlite3_step(statement) != SQLITE_DONE) {
            sqlite3_finalize(statement);
            throw std::runtime_error("telemetry persistence failed");
        }
        sqlite3_finalize(statement);
    }

    sqlite3* database_;
};

}  // namespace eco_restoration
