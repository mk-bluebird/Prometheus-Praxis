// File: cpp/eco_restoration/cyboquatic_robot_route_schema.cpp

#include <iostream>
#include <string>
#include <stdexcept>
#include <sqlite3.h>

namespace prometheus_praxis {
namespace eco_restoration {

void exec_sql(sqlite3* db, const std::string& sql) {
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "";
        sqlite3_free(errmsg);
        throw std::runtime_error("SQLite error: " + msg);
    }
}

// Schema for canal sample points, robot energy costs, and route plan.
void install_cyboquatic_robot_route_schema(sqlite3* db) {
    const std::string sql_sample_points = R"SQL(
        CREATE TABLE IF NOT EXISTS canal_sample_points (
            point_id        INTEGER PRIMARY KEY AUTOINCREMENT,
            canal_id        TEXT NOT NULL,
            lat             REAL NOT NULL,
            lon             REAL NOT NULL,
            priority        REAL DEFAULT 1.0,
            required        INTEGER NOT NULL DEFAULT 1
        );

        CREATE INDEX IF NOT EXISTS idx_canal_sample_points_canal
            ON canal_sample_points(canal_id);
    )SQL";

    const std::string sql_energy_costs = R"SQL(
        CREATE TABLE IF NOT EXISTS robot_energy_costs (
            from_point_id   INTEGER NOT NULL,
            to_point_id     INTEGER NOT NULL,
            distance_m      REAL NOT NULL,
            propulsion_energy_kwh REAL NOT NULL,
            sampling_energy_kwh   REAL NOT NULL,
            total_energy_kwh      REAL NOT NULL,
            PRIMARY KEY(from_point_id, to_point_id),
            FOREIGN KEY(from_point_id) REFERENCES canal_sample_points(point_id),
            FOREIGN KEY(to_point_id)   REFERENCES canal_sample_points(point_id)
        );

        CREATE INDEX IF NOT EXISTS idx_robot_energy_costs_from
            ON robot_energy_costs(from_point_id);
    )SQL";

    const std::string sql_route_plan = R"SQL(
        CREATE TABLE IF NOT EXISTS robot_route_plan (
            plan_id         INTEGER PRIMARY KEY AUTOINCREMENT,
            robot_id        TEXT NOT NULL,
            canal_id        TEXT NOT NULL,
            total_energy_kwh REAL NOT NULL,
            stop_count      INTEGER NOT NULL,
            created_at      TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS robot_route_segment (
            plan_id         INTEGER NOT NULL,
            seq_index       INTEGER NOT NULL,
            from_point_id   INTEGER NOT NULL,
            to_point_id     INTEGER NOT NULL,
            segment_energy_kwh REAL NOT NULL,
            PRIMARY KEY(plan_id, seq_index),
            FOREIGN KEY(plan_id) REFERENCES robot_route_plan(plan_id) ON DELETE CASCADE,
            FOREIGN KEY(from_point_id) REFERENCES canal_sample_points(point_id),
            FOREIGN KEY(to_point_id)   REFERENCES canal_sample_points(point_id)
        );
    )SQL";

    exec_sql(db, sql_sample_points);
    exec_sql(db, sql_energy_costs);
    exec_sql(db, sql_route_plan);
}

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "cyboquatic_route.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    sqlite3* db = nullptr;
    int rc = sqlite3_open(db_path.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to open DB: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    try {
        install_cyboquatic_robot_route_schema(db);
        std::cout << "Cyboquatic maintenance robot route schema installed into "
                  << db_path << std::endl;

        std::cout << "\n-- Example query: sample points --\n";
        std::cout << "SELECT * FROM canal_sample_points LIMIT 10;\n";

        std::cout << "\n-- Example query: energy costs --\n";
        std::cout << "SELECT * FROM robot_energy_costs LIMIT 10;\n";

        std::cout << "\n-- Example query: route plan and segments --\n";
        std::cout << "SELECT * FROM robot_route_plan LIMIT 10;\n";
        std::cout << "SELECT * FROM robot_route_segment ORDER BY plan_id, seq_index LIMIT 20;\n";
    } catch (const std::exception& ex) {
        std::cerr << "Cyboquatic route schema error: " << ex.what() << std::endl;
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}
