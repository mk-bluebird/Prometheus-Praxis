// File: cpp/tools/did_carbon_credit_manifest.cpp

#include <stdexcept>
#include <string>
#include <vector>
#include <sqlite3.h>

// Snapshot metadata for carbon credit evidence
struct SnapshotMetadata {
    std::string snapshot_id;
    std::string eco_restoration_sqlite_path;
    std::string registry_name;
    std::string jurisdiction_code;
    std::string did_owner;          // governance particle DID
    std::string verification_report; // URI or summary
    std::string third_party_audit;   // URI or summary
};

// Manifest record to be stored and signed
struct ManifestRecord {
    std::string snapshot_id;
    std::string did_owner;
    std::string merkle_root_hex;
    std::string registry_name;
    std::string jurisdiction_code;
    std::string verification_report;
    std::string third_party_audit;
    std::string signature_hex;      // DID-signed manifest hash
};

class CarbonCreditManifestService {
public:
    explicit CarbonCreditManifestService(sqlite3* db)
        : db_(db) {
        if (!db_) throw std::runtime_error("SQLite DB pointer must not be null");
        createTable();
    }

    ManifestRecord buildManifest(const SnapshotMetadata& meta,
                                 const std::string& merkle_root_hex,
                                 const std::string& signature_hex) {
        ManifestRecord rec{};
        rec.snapshot_id        = meta.snapshot_id;
        rec.did_owner          = meta.did_owner;
        rec.merkle_root_hex    = merkle_root_hex;
        rec.registry_name      = meta.registry_name;
        rec.jurisdiction_code  = meta.jurisdiction_code;
        rec.verification_report = meta.verification_report;
        rec.third_party_audit   = meta.third_party_audit;
        rec.signature_hex      = signature_hex;

        storeManifest(rec);
        return rec;
    }

    ManifestRecord loadManifest(const std::string& snapshot_id) {
        const char* sql =
            "SELECT snapshot_id, did_owner, merkle_root_hex, registry_name, "
            "       jurisdiction_code, verification_report, third_party_audit, signature_hex "
            "FROM carbon_credit_manifest "
            "WHERE snapshot_id = ? "
            "ORDER BY created_utc DESC "
            "LIMIT 1;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare manifest query");
        }

        sqlite3_bind_text(stmt, 1, snapshot_id.c_str(), -1, SQLITE_TRANSIENT);

        ManifestRecord rec{};
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            rec.snapshot_id       = columnText(stmt, 0);
            rec.did_owner         = columnText(stmt, 1);
            rec.merkle_root_hex   = columnText(stmt, 2);
            rec.registry_name     = columnText(stmt, 3);
            rec.jurisdiction_code = columnText(stmt, 4);
            rec.verification_report = columnText(stmt, 5);
            rec.third_party_audit   = columnText(stmt, 6);
            rec.signature_hex     = columnText(stmt, 7);
        }

        sqlite3_finalize(stmt);
        return rec;
    }

private:
    sqlite3* db_;

    void createTable() {
        const char* sql =
            "CREATE TABLE IF NOT EXISTS carbon_credit_manifest ("
            " snapshot_id        TEXT PRIMARY KEY,"
            " did_owner          TEXT NOT NULL,"
            " merkle_root_hex    TEXT NOT NULL,"
            " registry_name      TEXT NOT NULL,"
            " jurisdiction_code  TEXT NOT NULL,"
            " verification_report TEXT NOT NULL,"
            " third_party_audit   TEXT NOT NULL,"
            " signature_hex      TEXT NOT NULL,"
            " created_utc        INTEGER NOT NULL"
            ");";
        char* errMsg = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string msg = "Failed to create carbon_credit_manifest: ";
            if (errMsg) msg += errMsg;
            sqlite3_free(errMsg);
            throw std::runtime_error(msg);
        }
    }

    void storeManifest(const ManifestRecord& rec) {
        const char* sql =
            "INSERT OR REPLACE INTO carbon_credit_manifest "
            " (snapshot_id, did_owner, merkle_root_hex, registry_name, "
            "  jurisdiction_code, verification_report, third_party_audit, "
            "  signature_hex, created_utc) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, strftime('%s','now'));";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare manifest insert");
        }

        sqlite3_bind_text(stmt, 1, rec.snapshot_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, rec.did_owner.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, rec.merkle_root_hex.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, rec.registry_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, rec.jurisdiction_code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, rec.verification_report.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, rec.third_party_audit.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 8, rec.signature_hex.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to insert carbon credit manifest");
        }

        sqlite3_finalize(stmt);
    }

    static std::string columnText(sqlite3_stmt* stmt, int idx) {
        const unsigned char* txt = sqlite3_column_text(stmt, idx);
        return txt ? reinterpret_cast<const char*>(txt) : "";
    }
};
