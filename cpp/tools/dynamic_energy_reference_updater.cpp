// File: cpp/tools/dynamic_energy_reference_updater.cpp

#include <sqlite3.h>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: dynamic_energy_reference_updater database.sqlite observed_unix_s carbon_g_per_kwh\n";
        return 2;
    }

    sqlite3* database = nullptr;
    if (sqlite3_open(argv[1], &database) != SQLITE_OK) return 1;

    try {
        sqlite3_stmt* grid = nullptr;
        sqlite3_prepare_v2(database,
            "INSERT INTO grid_carbon_hour VALUES(?,?) "
            "ON CONFLICT(observed_unix_s) DO UPDATE SET carbon_g_per_kwh=excluded.carbon_g_per_kwh;",
            -1, &grid, nullptr);

        sqlite3_bind_int64(grid, 1, static_cast<sqlite3_int64>(std::stoll(argv[2])));
        sqlite3_bind_double(grid, 2, std::stod(argv[3]));
        if (sqlite3_step(grid) != SQLITE_DONE) throw std::runtime_error("grid update failed");
        sqlite3_finalize(grid);

        if (sqlite3_exec(database,
            "INSERT INTO dynamic_energy_reference "
            "SELECT observed_unix_s,rolling_carbon_g_per_kwh,remaining_budget_g,hourly_budget_g,"
            "reference_energy_kwh FROM current_energy_reference_hour;",
            nullptr, nullptr, nullptr) != SQLITE_OK) {
            throw std::runtime_error("reference update failed");
        }

        sqlite3_close(database);
    } catch (const std::exception& error) {
        sqlite3_close(database);
        std::cerr << "{\"error\":\"" << error.what() << "\"}\n";
        return 1;
    }
}
