// File: cpp/tools/sqlite_lane_write_benchmark.cpp

#include <sqlite3.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace eco_restoration {

struct Counters {
    std::atomic<std::uint64_t> inserted{0};
    std::atomic<std::uint64_t> busy{0};
    std::atomic<std::uint64_t> failed{0};
};

bool execute(sqlite3* database, const char* sql) {
    return sqlite3_exec(database, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

void writer(
    const std::string& database_path,
    std::uint32_t worker_id,
    std::chrono::steady_clock::time_point deadline,
    Counters& counters) {

    sqlite3* database = nullptr;
    if (sqlite3_open_v2(database_path.c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
        ++counters.failed;
        return;
    }

    sqlite3_busy_timeout(database, 2500);
    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT INTO lane_decision_benchmark("
        "worker_id,observed_unix_ms,knowledge,impact,risk,decision"
        ") VALUES(?,?,?,?,?,?);",
        -1, &statement, nullptr);

    std::uint64_t sequence = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        if (!execute(database, "BEGIN IMMEDIATE;")) {
            ++counters.busy;
            continue;
        }

        bool transaction_ok = true;
        for (int row = 0; row < 32; ++row) {
            const double knowledge = 0.70 + 0.01 * static_cast<double>((sequence + row) % 20);
            const double impact = 0.60 + 0.01 * static_cast<double>((sequence + row) % 25);
            const double risk = 0.10 + 0.01 * static_cast<double>((sequence + row) % 50);
            const auto unix_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            sqlite3_bind_int(statement, 1, static_cast<int>(worker_id));
            sqlite3_bind_int64(statement, 2, unix_ms);
            sqlite3_bind_double(statement, 3, knowledge);
            sqlite3_bind_double(statement, 4, impact);
            sqlite3_bind_double(statement, 5, risk);
            sqlite3_bind_int(statement, 6, risk > 0.35 ? 1 : 0);

            const int result = sqlite3_step(statement);
            if (result != SQLITE_DONE) {
                transaction_ok = false;
                if (result == SQLITE_BUSY || result == SQLITE_LOCKED) ++counters.busy;
                else ++counters.failed;
                sqlite3_reset(statement);
                sqlite3_clear_bindings(statement);
                break;
            }

            ++counters.inserted;
            ++sequence;
            sqlite3_reset(statement);
            sqlite3_clear_bindings(statement);
        }

        if (transaction_ok) {
            if (!execute(database, "COMMIT;")) {
                ++counters.busy;
                execute(database, "ROLLBACK;");
            }
        } else {
            execute(database, "ROLLBACK;");
        }
    }

    sqlite3_finalize(statement);
    sqlite3_close(database);
}

}  // namespace eco_restoration

int main(int argc, char** argv) {
    using namespace eco_restoration;

    if (argc != 5) {
        std::cerr << "usage: sqlite_lane_write_benchmark database.sqlite WAL|DELETE workers duration_seconds\n";
        return 2;
    }

    const std::string database_path = argv[1];
    const std::string journal_mode = argv[2];
    const int workers = std::atoi(argv[3]);
    const int duration_seconds = std::atoi(argv[4]);

    if ((journal_mode != "WAL" && journal_mode != "DELETE") || workers < 1 || duration_seconds < 1) {
        std::cerr << "invalid benchmark parameters\n";
        return 2;
    }

    sqlite3* setup = nullptr;
    if (sqlite3_open(database_path.c_str(), &setup) != SQLITE_OK) return 1;

    const std::string pragma = "PRAGMA journal_mode=" + journal_mode + ";"
                               "PRAGMA synchronous=NORMAL;"
                               "PRAGMA foreign_keys=ON;";
    const bool ready = execute(setup, pragma.c_str()) &&
        execute(setup,
            "CREATE TABLE IF NOT EXISTS lane_decision_benchmark("
            "event_id INTEGER PRIMARY KEY,worker_id INTEGER NOT NULL,"
            "observed_unix_ms INTEGER NOT NULL,knowledge REAL NOT NULL,"
            "impact REAL NOT NULL,risk REAL NOT NULL,decision INTEGER NOT NULL"
            ") STRICT;");
    sqlite3_close(setup);
    if (!ready) return 1;

    Counters counters;
    const auto started = std::chrono::steady_clock::now();
    const auto deadline = started + std::chrono::seconds(duration_seconds);
    std::vector<std::thread> threads;

    for (int worker = 0; worker < workers; ++worker) {
        threads.emplace_back(writer, database_path, static_cast<std::uint32_t>(worker), deadline,
                             std::ref(counters));
    }
    for (std::thread& thread : threads) thread.join();

    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();

    std::cout << "{\"journal_mode\":\"" << journal_mode
              << "\",\"workers\":" << workers
              << ",\"seconds\":" << elapsed
              << ",\"inserted\":" << counters.inserted.load()
              << ",\"busy\":" << counters.busy.load()
              << ",\"failed\":" << counters.failed.load()
              << ",\"inserts_per_second\":"
              << static_cast<double>(counters.inserted.load()) / elapsed
              << "}\n";
}
