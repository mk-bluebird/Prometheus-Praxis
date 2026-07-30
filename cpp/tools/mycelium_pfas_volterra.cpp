// File: cpp/tools/mycelium_pfas_volterra.cpp
// Repo path: cpp/tools/mycelium_pfas_volterra.cpp
//
// Purpose:
//   Non-actuating C++ tool that:
//     - Reads mycelial oscillation features from SQLite.
//     - Applies a fitted Volterra series model to estimate r_pfas_est.
//     - Writes r_pfas_est into mycelium_pfas_telemetry.
//   This is governance- and ecosafety-safe: it only processes data.

#include <sqlite3.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <sstream>

struct VolterraKernels {
    int          K1;
    int          K2;
    double       h0;
    std::vector<double> h1;            // size K1+1
    std::vector<std::vector<double>> h2; // size (K2+1) x (K2+1)
};

struct OscRecord {
    std::string segment_id;
    std::string yyyymmdd;
    double      osc_feature;
};

// Simple JSON-like parser for h1,h2 vectors from stored strings.
// In production, use a robust JSON library; here we expect comma-separated values.
std::vector<double> parse_h1(const std::string& s) {
    std::vector<double> v;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (!tok.empty()) {
            v.push_back(std::stod(tok));
        }
    }
    return v;
}

std::vector<std::vector<double>> parse_h2(const std::string& s, int K2) {
    // Expect K2+1 rows, separated by ';', each row comma-separated.
    std::vector<std::vector<double>> h2;
    std::stringstream ss(s);
    std::string rowStr;
    int rowCount = 0;
    while (std::getline(ss, rowStr, ';')) {
        if (rowStr.empty()) continue;
        std::vector<double> row;
        std::stringstream rs(rowStr);
        std::string tok;
        while (std::getline(rs, tok, ',')) {
            if (!tok.empty()) {
                row.push_back(std::stod(tok));
            }
        }
        h2.push_back(row);
        ++rowCount;
    }
    // Basic sanity check.
    if (rowCount != K2 + 1) {
        throw std::runtime_error("parse_h2: unexpected row count");
    }
    return h2;
}

// Fetch Volterra kernels for a given segment_id.
VolterraKernels fetch_volterra_kernels(sqlite3* db, const std::string& segment_id) {
    VolterraKernels k;

    const char* sql =
        "SELECT K1, K2, h0, h1_json, h2_json "
        "FROM mycelium_volterra_kernels "
        "WHERE segment_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare kernels query");
    }

    sqlite3_bind_text(stmt, 1, segment_id.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        k.K1 = sqlite3_column_int(stmt, 0);
        k.K2 = sqlite3_column_int(stmt, 1);
        k.h0 = sqlite3_column_double(stmt, 2);
        const char* h1s = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char* h2s = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        std::string h1_json = h1s ? h1s : "";
        std::string h2_json = h2s ? h2s : "";
        k.h1 = parse_h1(h1_json);
        k.h2 = parse_h2(h2_json, k.K2);
    } else {
        sqlite3_finalize(stmt);
        throw std::runtime_error("No kernels for segment " + segment_id);
    }

    sqlite3_finalize(stmt);
    return k;
}

// Fetch mycelial oscillation records for a given segment.
std::vector<OscRecord> fetch_osc_records(sqlite3* db, const std::string& segment_id) {
    std::vector<OscRecord> recs;

    const char* sql =
        "SELECT segment_id, yyyymmdd, osc_feature "
        "FROM mycelium_osc_raw "
        "WHERE segment_id = ? "
        "ORDER BY yyyymmdd;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare osc query");
    }

    sqlite3_bind_text(stmt, 1, segment_id.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        OscRecord r;
        const char* seg = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.segment_id = seg ? seg : "";
        r.yyyymmdd   = date ? date : "";
        r.osc_feature = sqlite3_column_double(stmt, 2);
        recs.push_back(r);
    }

    sqlite3_finalize(stmt);
    return recs;
}

// Apply first- and second-order Volterra model to estimate r_pfas_est.
// For simplicity, we use osc_feature directly as u(t); in practice,
// u(t) would be a PFAS-related input, and osc_feature would be y(t).
double estimate_r_pfas(const VolterraKernels& k,
                       const std::vector<double>& uHistory) {
    double y = k.h0;
    // First-order term.
    for (int i = 0; i <= k.K1 && i < static_cast<int>(uHistory.size()); ++i) {
        y += k.h1[i] * uHistory[uHistory.size() - 1 - i];
    }
    // Second-order term.
    for (int i = 0; i <= k.K2 && i < static_cast<int>(uHistory.size()); ++i) {
        for (int j = 0; j <= k.K2 && j < static_cast<int>(uHistory.size()); ++j) {
            double ui = uHistory[uHistory.size() - 1 - i];
            double uj = uHistory[uHistory.size() - 1 - j];
            y += k.h2[i][j] * ui * uj;
        }
    }
    // Map to [0,1] risk coordinate with a simple logistic; tune as needed.
    double r_est = 1.0 / (1.0 + std::exp(-y));
    if (r_est < 0.0) r_est = 0.0;
    if (r_est > 1.0) r_est = 1.0;
    return r_est;
}

// Insert r_pfas_est into mycelium_pfas_telemetry.
void insert_pfas_est(sqlite3* db, const OscRecord& rec, double r_pfas_est) {
    const char* sql =
        "INSERT OR REPLACE INTO mycelium_pfas_telemetry "
        "(segment_id, yyyymmdd, osc_feature, r_pfas_est, evidence_hex, created_utc) "
        "VALUES (?, ?, ?, ?, ?, datetime('now'));";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare telemetry insert");
    }

    std::string evidence_hex = "0x20260729PHXMyceliumPFASVolterra2026v1";
    sqlite3_bind_text(stmt, 1, rec.segment_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, rec.yyyymmdd.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, rec.osc_feature);
    sqlite3_bind_double(stmt, 4, r_pfas_est);
    sqlite3_bind_text(stmt, 5, evidence_hex.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to insert mycelium_pfas_telemetry");
    }

    sqlite3_finalize(stmt);
}

int main(int argc, char** argv) {
    const char* db_path = "dbcyboquaticdailyprogress.sqlite";
    if (argc > 1) {
        db_path = argv[1];
    }

    sqlite3* db = nullptr;
    if (sqlite3_open(db_path, &db) != SQLITE_OK) {
        std::cerr << "Failed to open SQLite DB: " << db_path << "\n";
        return 1;
    }

    try {
        // For simplicity, process one segment; in production, iterate over all.
        std::string segment_id = "PHX-CANAL-SEG-001";

        // Fetch Volterra kernels and oscillation records.
        VolterraKernels kernels = fetch_volterra_kernels(db, segment_id);
        auto oscRecords = fetch_osc_records(db, segment_id);

        // Maintain history of osc_feature as u(t).
        std::vector<double> uHistory;

        for (const auto& rec : oscRecords) {
            uHistory.push_back(rec.osc_feature);
            double r_est = estimate_r_pfas(kernels, uHistory);
            insert_pfas_est(db, rec, r_est);
        }

        std::cout << "Mycelium PFAS estimates written for segment "
                  << segment_id << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}
