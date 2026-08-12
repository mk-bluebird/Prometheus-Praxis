// File: cpp/tools/coroutine_unix_telemetry_server.cpp
#include <coroutine>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <sqlite3.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

class Scheduler {
public:
    Scheduler() : epoll_(epoll_create1(EPOLL_CLOEXEC)) {
        if (epoll_ < 0) throw std::runtime_error("epoll creation failed");
    }
    ~Scheduler() { close(epoll_); }

    void wait_readable(int fd, std::coroutine_handle<> handle) {
        epoll_event event{};
        event.events = EPOLLIN | EPOLLONESHOT;
        event.data.fd = fd;
        if (epoll_ctl(epoll_, EPOLL_CTL_ADD, fd, &event) < 0)
            epoll_ctl(epoll_, EPOLL_CTL_MOD, fd, &event);
        waiting_[fd] = handle;
    }

    void run() {
        while (true) {
            epoll_event event{};
            if (epoll_wait(epoll_, &event, 1, -1) <= 0) continue;
            const auto found = waiting_.find(event.data.fd);
            if (found == waiting_.end()) continue;
            const auto handle = found->second;
            waiting_.erase(found);
            handle.resume();
            if (handle.done()) handle.destroy();
        }
    }

private:
    int epoll_;
    std::unordered_map<int, std::coroutine_handle<>> waiting_;
};

struct TelemetryTask {
    struct promise_type {
        TelemetryTask get_return_object() { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { std::terminate(); }
    };
};

struct Readable {
    Scheduler& scheduler;
    int fd;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) { scheduler.wait_readable(fd, handle); }
    void await_resume() const noexcept {}
};

void persist(sqlite3* database, const char* frame) {
    long long anchor{}, timestamp{};
    double temperature{}, water_quality{};
    if (std::sscanf(frame, "%lld,%lld,%lf,%lf", &anchor, &timestamp, &temperature, &water_quality) != 4 ||
        anchor < 0 || timestamp < 0 || water_quality < 0.0 || water_quality > 1.0)
        throw std::invalid_argument("invalid telemetry frame");

    sqlite3_stmt* raw = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT OR IGNORE INTO unix_socket_telemetry VALUES(?,?,?,?);", -1, &raw, nullptr);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, sqlite3_finalize);
    sqlite3_bind_int64(statement.get(), 1, anchor);
    sqlite3_bind_int64(statement.get(), 2, timestamp);
    sqlite3_bind_double(statement.get(), 3, temperature);
    sqlite3_bind_double(statement.get(), 4, water_quality);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) throw std::runtime_error("telemetry persistence failed");
}

TelemetryTask handle_client(Scheduler& scheduler, int client, sqlite3* database) {
    co_await Readable{scheduler, client};
    char buffer[256]{};
    const ssize_t bytes = recv(client, buffer, sizeof(buffer) - 1, 0);
    try {
        if (bytes <= 0) throw std::runtime_error("empty frame");
        persist(database, buffer);
        send(client, "{\"status\":\"accepted\"}\n", 22, MSG_NOSIGNAL);
    } catch (...) {
        send(client, "{\"status\":\"rejected\"}\n", 22, MSG_NOSIGNAL);
    }
    close(client);
}

TelemetryTask accept_clients(Scheduler& scheduler, int listener, sqlite3* database) {
    while (true) {
        co_await Readable{scheduler, listener};
        for (;;) {
            const int client = accept4(listener, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
            if (client < 0) break;
            handle_client(scheduler, client, database);
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    sqlite3* database = nullptr;
    if (sqlite3_open(argv[1], &database) != SQLITE_OK) return 1;
    sqlite3_exec(database,
        "CREATE TABLE IF NOT EXISTS unix_socket_telemetry("
        "hex_anchor INTEGER NOT NULL,observed_unix_s INTEGER NOT NULL,"
        "temperature_c REAL NOT NULL,water_quality_index REAL NOT NULL "
        "CHECK(water_quality_index BETWEEN 0 AND 1),"
        "PRIMARY KEY(hex_anchor,observed_unix_s)) STRICT;", nullptr, nullptr, nullptr);

    const int listener = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (std::strlen(argv[2]) >= sizeof(address.sun_path)) return 2;
    std::strcpy(address.sun_path, argv[2]);
    unlink(argv[2]);
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 || listen(listener, 128) < 0)
        return 1;

    Scheduler scheduler;
    accept_clients(scheduler, listener, database);
    scheduler.run();
}
