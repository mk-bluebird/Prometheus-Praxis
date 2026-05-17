// filename: cpp/econet_spine_inspect.cpp
// destination: mk-bluebird/eco_restoration_shard/cpp/econet_spine_inspect.cpp

#include "econet_spine_inspect.hpp"
#include <sqlite3.h>

std::vector<EcoNodeEnergyCarbonCpp>
econet_query_best_nodes_for_energy_tailwind(const std::string &db_path, int limit) {
    std::vector<EcoNodeEnergyCarbonCpp> out;
    sqlite3 *db = nullptr;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        return out;
    }
    const char *sql =
        "SELECT node_id, n_events, e_req_accept_j, e_surplus_accept_j,"
        "       r_carbon_avg, r_biodiv_avg, dv_avg "
        "FROM v_node_energy_carbon "
        "WHERE n_events >= 5 "
        "  AND dv_avg <= 0.0 "
        "  AND e_surplus_accept_j >= e_req_accept_j "
        "ORDER BY r_carbon_avg ASC, e_req_accept_j ASC "
        "LIMIT ?1;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return out;
    }
    sqlite3_bind_int(stmt, 1, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        EcoNodeEnergyCarbonCpp row;
        row.node_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        row.n_events = sqlite3_column_int64(stmt, 1);
        row.e_req_accept_j = sqlite3_column_double(stmt, 2);
        row.e_surplus_accept_j = sqlite3_column_double(stmt, 3);
        row.r_carbon_avg = sqlite3_column_double(stmt, 4);
        row.r_biodiv_avg = sqlite3_column_double(stmt, 5);
        row.dv_avg = sqlite3_column_double(stmt, 6);
        out.push_back(row);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return out;
}

std::vector<EcoCandidateEcorestorativeCpp>
econet_query_candidate_ecorestorative(const std::string &db_path, int limit) {
    std::vector<EcoCandidateEcorestorativeCpp> out;
    sqlite3 *db = nullptr;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        return out;
    }
    const char *sql =
        "SELECT source_type, source_id, impact_carbon, impact_biodiv,"
        "       vt_sensitivity_avg, dv_avg "
        "FROM v_candidate_ecorestorative "
        "ORDER BY impact_carbon DESC, impact_biodiv DESC "
        "LIMIT ?1;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return out;
    }
    sqlite3_bind_int(stmt, 1, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        EcoCandidateEcorestorativeCpp row;
        row.source_type = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        row.source_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        row.impact_carbon = sqlite3_column_double(stmt, 2);
        row.impact_biodiv = sqlite3_column_double(stmt, 3);
        row.vt_sensitivity_avg = sqlite3_column_double(stmt, 4);
        row.dv_avg = sqlite3_column_double(stmt, 5);
        out.push_back(row);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return out;
}
