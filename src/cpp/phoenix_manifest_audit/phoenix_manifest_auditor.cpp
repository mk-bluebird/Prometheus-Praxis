// filename: src/cpp/phoenix_manifest_audit/phoenix_manifest_auditor.cpp
// repo: mk-bluebird/eco_restoration_shard

#include "phoenix_manifest_auditor.hpp"

#include <sqlite3.h>
#include <iostream>

namespace phoenix_manifest_audit {

PhoenixManifestAuditor::PhoenixManifestAuditor(const AuditConfig& config)
    : config_(config) {}

void PhoenixManifestAuditor::audit_row(
    const std::string& stewarddid,
    const std::string& regioncode,
    const std::string& dayutc,
    double rohmaxday,
    int rohok,
    int kerdeployableday,
    int lyapunovokday)
{
    const bool expected_rohok = (rohmaxday <= 0.30);
    if (rohok != static_cast<int>(expected_rohok)) {
        std::cerr << "[AUDIT] RoH mismatch for " << stewarddid
                  << " " << regioncode << " " << dayutc << "\n";
    }
    if (kerdeployableday != 1) {
        std::cerr << "[AUDIT] kerdeployableday != 1 for manifest row "
                  << stewarddid << " " << regioncode << " " << dayutc << "\n";
    }
    if (lyapunovokday != 1) {
        std::cerr << "[AUDIT] lyapunovokday != 1 for manifest row "
                  << stewarddid << " " << regioncode << " " << dayutc << "\n";
    }
}

int PhoenixManifestAuditor::run_audit() {
    sqlite3* db = nullptr;
    if (sqlite3_open(config_.sqlite_path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Failed to open SQLite DB: " << config_.sqlite_path << "\n";
        return 1;
    }

    const char* sql =
        "SELECT stewarddid, regioncode, dayutc, rohmaxday, rohok, "
        "kerdeployableday, lyapunovokday "
        "FROM stewarddailystatephx";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement\n";
        sqlite3_close(db);
        return 1;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* sdid = sqlite3_column_text(stmt, 0);
        const unsigned char* rcode = sqlite3_column_text(stmt, 1);
        const unsigned char* day = sqlite3_column_text(stmt, 2);
        double rohmaxday = sqlite3_column_double(stmt, 3);
        int rohok = sqlite3_column_int(stmt, 4);
        int kerdeployableday = sqlite3_column_int(stmt, 5);
        int lyapunovokday = sqlite3_column_int(stmt, 6);

        audit_row(
            sdid ? reinterpret_cast<const char*>(sdid) : "",
            rcode ? reinterpret_cast<const char*>(rcode) : "",
            day ? reinterpret_cast<const char*>(day) : "",
            rohmaxday,
            rohok,
            kerdeployableday,
            lyapunovokday);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}

} // namespace phoenix_manifest_audit
