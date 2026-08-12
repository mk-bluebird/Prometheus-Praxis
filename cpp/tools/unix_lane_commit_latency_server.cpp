// File: cpp/tools/unix_lane_commit_latency_server.cpp

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

namespace eco_restoration {

struct WireTelemetry {
    std::uint64_t hex_anchor;
    double knowledge;
    double impact;
    double risk;
    double delta_v;
};

bool read_exact(int socket, void* buffer, std::size_t bytes) {
    auto* output = static_cast<unsigned char*>(buffer);
    while (bytes > 0U) {
        const ssize_t received = recv(socket, output, bytes, 0);
        if (received <= 0) return false;
        output += received;
        bytes -= static_cast<std::size_t>(received);
    }
    return true;
}

double percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const std::size_t index = static_cast<std::size_t>(
        std::clamp(p, 0.0, 1.0) * static_cast<double>(values.size() - 1U));
    return values[index];
}

}  // namespace eco_restoration

int main(int argc, char** argv) {
    using namespace eco_restoration;

    if (argc != 4) {
        std::cerr << "usage: unix_lane_commit_latency_server socket_path decisions.sqlite arrival_rate_per_second\n";
        return 2;
    }

    const double arrival_rate = std::stod(argv[3]);
    if (arrival_rate < 0.0) return 2;

    sqlite3* database = nullptr;
    if (sqlite3_open(argv[2], &database) != SQLITE_OK) return 1;
    sqlite3_busy_timeout(database, 2500);

    if (sqlite3_exec(database,
        "PRAGMA journal_mode=WAL;"
        "CREATE TABLE IF NOT EXISTS lane_latency_decision("
        "event_id INTEGER PRIMARY KEY,hex_anchor INTEGER NOT NULL,knowledge REAL NOT NULL,"
        "impact REAL NOT NULL,risk REAL NOT NULL,delta_v REAL NOT NULL,"
        "decision INTEGER NOT NULL,committed_unix_ns INTEGER NOT NULL) STRICT;",
        nullptr, nullptr, nullptr) != SQLITE_OK) {
        sqlite3_close(database);
        return 1;
    }

    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT INTO lane_latency_decision("
        "hex_anchor,knowledge,impact,risk,delta_v,decision,committed_unix_ns"
        ") VALUES(?,?,?,?,?,?,?);",
        -1, &statement, nullptr);

    const std::string socket_path = argv[1];
    unlink(socket_path.c_str());
    const int listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener < 0) return 1;

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, socket_path.c_str(), sizeof(address.sun_path) - 1U);
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        listen(listener, 16) < 0) {
        close(listener);
        return 1;
    }

    std::vector<double> samples;
    for (;;) {
        const int client = accept(listener, nullptr, nullptr);
        if (client < 0) continue;

        WireTelemetry frame{};
        while (read_exact(client, &frame, sizeof(frame))) {
            const auto started = std::chrono::steady_clock::now();
            const int decision = frame.knowledge < 0.60 || frame.impact < 0.55 ||
                                 frame.risk > 0.70 || frame.risk < frame.delta_v ? 2 :
                                 frame.risk > 0.35 ? 1 : 0;
            const auto committed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            sqlite3_exec(database, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr);
            sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(frame.hex_anchor));
            sqlite3_bind_double(statement, 2, frame.knowledge);
            sqlite3_bind_double(statement, 3, frame.impact);
            sqlite3_bind_double(statement, 4, frame.risk);
            sqlite3_bind_double(statement, 5, frame.delta_v);
            sqlite3_bind_int(statement, 6, decision);
            sqlite3_bind_int64(statement, 7, committed_ns);

            const bool inserted = sqlite3_step(statement) == SQLITE_DONE;
            sqlite3_reset(statement);
            sqlite3_clear_bindings(statement);
            sqlite3_exec(database, inserted ? "COMMIT;" : "ROLLBACK;", nullptr, nullptr, nullptr);

            const double latency_s = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - started).count();
            samples.push_back(latency_s);
            const unsigned char response = inserted ? static_cast<unsigned char>(decision) : 255U;
            send(client, &response, 1, 0);

            if (samples.size() == 1000U) {
                double sum = 0.0;
                for (double value : samples) sum += value;
                const double mean = sum / samples.size();
                const double service_rate = 1.0 / std::max(mean, 1e-12);
                const double predicted_response_s = arrival_rate < service_rate
                    ? 1.0 / (service_rate - arrival_rate)
                    : -1.0;

                std::cout << "{\"samples\":" << samples.size()
                          << ",\"mean_commit_seconds\":" << mean
                          << ",\"p95_commit_seconds\":" << percentile(samples, 0.95)
                          << ",\"service_rate_per_second\":" << service_rate
                          << ",\"arrival_rate_per_second\":" << arrival_rate
                          << ",\"predicted_m_m_1_response_seconds\":" << predicted_response_s
                          << "}\n";
                samples.clear();
            }
        }
        close(client);
    }
}
