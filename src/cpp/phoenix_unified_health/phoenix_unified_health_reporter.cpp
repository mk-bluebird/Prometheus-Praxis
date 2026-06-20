// filename: src/cpp/phoenix_unified_health/phoenix_unified_health_reporter.cpp
// repo: mk-bluebird/eco_restoration_shard

#include "phoenix_unified_health_reporter.hpp"

#include <sqlite3.h>
#include <fstream>
#include <iostream>

namespace phoenix_unified_health {

PhoenixUnifiedHealthReporter::PhoenixUnifiedHealthReporter(const ReporterConfig& config)
    : config_(config) {}

void PhoenixUnifiedHealthReporter::write_csv_header(std::ostream& out) const {
    out << "stewarddid,regioncode,dayutc,"
        << "kmeanday,emeanday,rmeanday,vtmaxday,"
        << "ecounitissuedday,rohmaxday,rohok,"
        << "rresponsibilityday,rportfoliodiversityday,rtopologyday,"
        << "lifeforcedeltaday,biokarmadeltaday\n";
}

int PhoenixUnifiedHealthReporter::generate_reports() {
    sqlite3* db = nullptr;
    if (sqlite3_open(config_.sqlite_path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Failed to open SQLite DB: " << config_.sqlite_path << "\n";
        return 1;
    }

    const char* sql =
        "SELECT stewarddid, regioncode, dayutc, "
        "kmeanday, emeanday, rmeanday, vtmaxday, "
        "ecounitissuedday, rohmaxday, rohok, "
        "rresponsibilityday, rportfoliodiversityday, rtopologyday, "
        "lifeforcedeltaday, biokarmadeltaday "
        "FROM vunifieddailyevolutionmanifestphx";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement\n";
        sqlite3_close(db);
        return 1;
    }

    std::ofstream csv_file(config_.csv_output_path);
    if (!csv_file.is_open()) {
        std::cerr << "Failed to open CSV output: " << config_.csv_output_path << "\n";
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return 1;
    }
    write_csv_header(csv_file);

    std::ofstream json_file(config_.json_output_path);
    if (!json_file.is_open()) {
        std::cerr << "Failed to open JSON output: " << config_.json_output_path << "\n";
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return 1;
    }
    json_file << "[\n";

    bool first = true;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* stewarddid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* regioncode = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* dayutc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        double kmeanday = sqlite3_column_double(stmt, 3);
        double emeanday = sqlite3_column_double(stmt, 4);
        double rmeanday = sqlite3_column_double(stmt, 5);
        double vtmaxday = sqlite3_column_double(stmt, 6);
        double ecounitissuedday = sqlite3_column_double(stmt, 7);
        double rohmaxday = sqlite3_column_double(stmt, 8);
        int rohok = sqlite3_column_int(stmt, 9);
        double rresponsibilityday = sqlite3_column_double(stmt, 10);
        double rportfoliodiversityday = sqlite3_column_double(stmt, 11);
        double rtopologyday = sqlite3_column_double(stmt, 12);
        double lifeforcedeltaday = sqlite3_column_double(stmt, 13);
        double biokarmadeltaday = sqlite3_column_double(stmt, 14);

        csv_file
            << (stewarddid ? stewarddid : "") << ","
            << (regioncode ? regioncode : "") << ","
            << (dayutc ? dayutc : "") << ","
            << kmeanday << ","
            << emeanday << ","
            << rmeanday << ","
            << vtmaxday << ","
            << ecounitissuedday << ","
            << rohmaxday << ","
            << rohok << ","
            << rresponsibilityday << ","
            << rportfoliodiversityday << ","
            << rtopologyday << ","
            << lifeforcedeltaday << ","
            << biokarmadeltaday << "\n";

        if (!first) {
            json_file << ",\n";
        }
        first = false;

        json_file << "  {"
                  << "\"stewarddid\":\"" << (stewarddid ? stewarddid : "") << "\","
                  << "\"regioncode\":\"" << (regioncode ? regioncode : "") << "\","
                  << "\"dayutc\":\"" << (dayutc ? dayutc : "") << "\","
                  << "\"kmeanday\":" << kmeanday << ","
                  << "\"emeanday\":" << emeanday << ","
                  << "\"rmeanday\":" << rmeanday << ","
                  << "\"vtmaxday\":" << vtmaxday << ","
                  << "\"ecounitissuedday\":" << ecounitissuedday << ","
                  << "\"rohmaxday\":" << rohmaxday << ","
                  << "\"rohok\":" << rohok << ","
                  << "\"rresponsibilityday\":" << rresponsibilityday << ","
                  << "\"rportfoliodiversityday\":" << rportfoliodiversityday << ","
                  << "\"rtopologyday\":" << rtopologyday << ","
                  << "\"lifeforcedeltaday\":" << lifeforcedeltaday << ","
                  << "\"biokarmadeltaday\":" << biokarmadeltaday
                  << "}";
    }

    json_file << "\n]\n";

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return 0;
}

} // namespace phoenix_unified_health
