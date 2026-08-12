// File: cpp/tools/ppx_realtime_telemetry_server.cpp
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include <sqlite3.h>

namespace ppx::eco_restoration {

struct Telemetry {
    std::string machine_id;
    std::string station_id;
    double k{}, e{}, r{}, roh{}, vt_current{}, vt_next{};
};

struct Decision {
    std::string action;
    std::string reason;
    double delta_vt{};
};

class LaneEvaluator {
public:
    virtual ~LaneEvaluator() = default;
    virtual Decision evaluate(const Telemetry& telemetry) const = 0;
};

class CorridorLaneEvaluator final : public LaneEvaluator {
public:
    Decision evaluate(const Telemetry& t) const override {
        for (const double value : {t.k, t.e, t.r, t.roh}) {
            if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
                return {"HALT", "invalid_normalized_telemetry", 0.0};
            }
        }
        const double delta = t.vt_next - t.vt_current;
        if (t.machine_id.empty() || t.station_id.empty()) return {"HALT", "missing_identity", delta};
        if (t.roh >= 0.25 || delta > 0.02) return {"HALT", "residual_or_harm_corridor", delta};
        if (t.k < 0.65 || t.e < 0.65 || t.r > 0.25) return {"DERATE", "ker_lane_threshold", delta};
        return {"PROCEED", "all_governance_gates_passed", delta};
    }
};

class ResultSink {
public:
    virtual ~ResultSink() = default;
    virtual void persist(const Telemetry&, const Decision&) = 0;
};

class SqliteResultSink final : public ResultSink {
public:
    explicit SqliteResultSink(const char* path) {
        if (sqlite3_open_v2(path, &database_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
            throw std::runtime_error("cannot open SQLite database");
        }
        execute("CREATE TABLE IF NOT EXISTS ppx_realtime_decision("
                "machine_id TEXT,station_id TEXT,observed_utc TEXT,k REAL,e REAL,r REAL,roh REAL,"
                "vt_current REAL,vt_next REAL,delta_vt REAL,action TEXT,reason TEXT) STRICT;");
    }

    ~SqliteResultSink() override { sqlite3_close(database_); }

    void persist(const Telemetry& t, const Decision& d) override {
        sqlite3_stmt* raw = nullptr;
        const char* query =
            "INSERT INTO ppx_realtime_decision VALUES(?,?,?, ?,?,?,?,?,?,?,?,?);";
        if (sqlite3_prepare_v2(database_, query, -1, &raw, nullptr) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(database_));
        }
        std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, sqlite3_finalize);
        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const std::string timestamp = std::to_string(now);
        sqlite3_bind_text(raw, 1, t.machine_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(raw, 2, t.station_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(raw, 3, timestamp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(raw, 4, t.k); sqlite3_bind_double(raw, 5, t.e);
        sqlite3_bind_double(raw, 6, t.r); sqlite3_bind_double(raw, 7, t.roh);
        sqlite3_bind_double(raw, 8, t.vt_current); sqlite3_bind_double(raw, 9, t.vt_next);
        sqlite3_bind_double(raw, 10, d.delta_vt);
        sqlite3_bind_text(raw, 11, d.action.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(raw, 12, d.reason.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(raw) != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(database_));
    }

private:
    sqlite3* database_{};

    void execute(const char* query) {
        if (sqlite3_exec(database_, query, nullptr, nullptr, nullptr) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(database_));
        }
    }
};

class LuaSubscriberBroadcaster {
public:
    explicit LuaSubscriberBroadcaster(std::vector<std::string> socket_paths)
        : paths_(std::move(socket_paths)), descriptor_(socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0)) {
        if (descriptor_ < 0) throw std::runtime_error("cannot create subscriber datagram socket");
    }

    ~LuaSubscriberBroadcaster() { close(descriptor_); }

    void publish(const Telemetry& t, const Decision& d) const {
        const std::string frame = "machine_id=" + t.machine_id + "\tstation_id=" + t.station_id +
            "\tK=" + std::to_string(t.k) + "\tE=" + std::to_string(t.e) +
            "\tR=" + std::to_string(t.r) + "\tdelta_vt=" + std::to_string(d.delta_vt) +
            "\taction=" + d.action + "\treason=" + d.reason;
        for (const std::string& path : paths_) {
            sockaddr_un address{};
            address.sun_family = AF_UNIX;
            if (path.size() >= sizeof(address.sun_path)) continue;
            std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
            sendto(descriptor_, frame.data(), frame.size(), MSG_DONTWAIT,
                   reinterpret_cast<const sockaddr*>(&address), sizeof(address));
        }
    }

private:
    std::vector<std::string> paths_;
    int descriptor_{};
};

class TelemetryServer {
public:
    TelemetryServer(std::string socket_path, const LaneEvaluator& evaluator,
                    ResultSink& sink, const LuaSubscriberBroadcaster& broadcaster)
        : path_(std::move(socket_path)), evaluator_(evaluator), sink_(sink), broadcaster_(broadcaster) {}

    void run() const {
        const int listener = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (listener < 0) throw std::runtime_error("cannot create telemetry socket");
        std::unique_ptr<int, decltype(&close)> closer(new int(listener), [](int* fd) { close(*fd); delete fd; });

        unlink(path_.c_str());
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        if (path_.size() >= sizeof(address.sun_path)) throw std::runtime_error("socket path too long");
        std::memcpy(address.sun_path, path_.c_str(), path_.size() + 1);
        if (bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 ||
            chmod(path_.c_str(), 0660) != 0 || listen(listener, 32) != 0) {
            throw std::runtime_error("cannot bind telemetry socket");
        }

        for (;;) {
            const int client = accept4(listener, nullptr, nullptr, SOCK_CLOEXEC);
            if (client < 0) continue;
            std::array<char, 1024> buffer{};
            const ssize_t bytes = recv(client, buffer.data(), buffer.size() - 1, 0);
            if (bytes > 0) {
                try {
                    const Telemetry telemetry = parse(std::string_view(buffer.data(), static_cast<std::size_t>(bytes)));
                    const Decision decision = evaluator_.evaluate(telemetry);
                    sink_.persist(telemetry, decision);
                    broadcaster_.publish(telemetry, decision);
                } catch (const std::exception&) {}
            }
            close(client);
        }
    }

private:
    std::string path_;
    const LaneEvaluator& evaluator_;
    ResultSink& sink_;
    const LuaSubscriberBroadcaster& broadcaster_;

    static Telemetry parse(std::string_view line) {
        Telemetry t{};
        std::istringstream input{std::string(line)};
        if (!(input >> t.machine_id >> t.station_id >> t.k >> t.e >> t.r >> t.roh >> t.vt_current >> t.vt_next)) {
            throw std::invalid_argument("expected machine station K E R RoH Vt VtNext");
        }
        return t;
    }
};

struct HeatRegression {
    double intercept{};
    double energy_coefficient{};
    double r_squared{};
};

HeatRegression fit_heat_regression(const std::vector<std::pair<double, double>>& observations) {
    if (observations.size() < 2) throw std::invalid_argument("at least two observations required");
    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
    for (const auto& [energy_j, delta_t] : observations) {
        sx += energy_j; sy += delta_t; sxx += energy_j * energy_j; sxy += energy_j * delta_t;
    }
    const double n = static_cast<double>(observations.size());
    const double denominator = n * sxx - sx * sx;
    if (denominator == 0.0) throw std::invalid_argument("energy observations lack variation");
    const double beta1 = (n * sxy - sx * sy) / denominator;
    const double beta0 = (sy - beta1 * sx) / n;
    double sse = 0.0, sst = 0.0;
    const double mean = sy / n;
    for (const auto& [x, y] : observations) {
        sse += (y - (beta0 + beta1 * x)) * (y - (beta0 + beta1 * x));
        sst += (y - mean) * (y - mean);
    }
    return {beta0, beta1, sst == 0.0 ? 1.0 : 1.0 - sse / sst};
}

}  // namespace ppx::eco_restoration
