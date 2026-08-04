// File: cpp/tools/mqtt_sqlite_telemetry_sidecar.cpp

#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <chrono>
#include <atomic>
#include <optional>
#include <random>

#include <sqlite3.h>
#include <mqtt/async_client.h>        // Eclipse Paho C++ client
#include <nlohmann/json.hpp>         // JSON for Modern C++

using json = nlohmann::json;

namespace prometheus_praxis {
namespace telemetry {

struct TelemetryMessage {
    std::string topic;
    std::string payload;
    std::chrono::system_clock::time_point received_at;
};

struct ValidationResult {
    bool valid;
    std::string error;
};

struct BackoffConfig {
    std::size_t initial_delay_ms;
    std::size_t max_delay_ms;
    double backoff_factor;
    std::size_t max_retries;
};

struct BatchConfig {
    std::size_t max_batch_size;
    std::size_t flush_interval_ms;
};

struct MetricsSnapshot {
    std::size_t total_received;
    std::size_t total_valid;
    std::size_t total_invalid;
    std::size_t total_db_success;
    std::size_t total_db_failure;
    double avg_insert_latency_ms;
    double avg_backoff_delay_ms;
};

class JsonSchemaValidator {
public:
    explicit JsonSchemaValidator(const json& schema)
        : schema_(schema) {}

    ValidationResult validate(const json& doc) const {
        // Minimal embedded schema checker:
        // - required fields
        // - type checks for string, number, boolean, object
        if (!schema_.is_object()) {
            return {false, "Schema must be an object"};
        }

        if (schema_.contains("required") && schema_["required"].is_array()) {
            for (const auto& field : schema_["required"]) {
                if (!field.is_string()) continue;
                std::string key = field.get<std::string>();
                if (!doc.contains(key)) {
                    return {false, "Missing required field: " + key};
                }
            }
        }

        if (schema_.contains("properties") && schema_["properties"].is_object()) {
            for (auto it = schema_["properties"].begin(); it != schema_["properties"].end(); ++it) {
                const std::string key = it.key();
                const json& prop = it.value();
                if (!doc.contains(key)) {
                    continue; // required handled above
                }
                const json& value = doc[key];
                if (prop.contains("type") && prop["type"].is_string()) {
                    const std::string type = prop["type"].get<std::string>();
                    if (type == "string" && !value.is_string()) {
                        return {false, "Field '" + key + "' must be string"};
                    } else if (type == "number" && !value.is_number()) {
                        return {false, "Field '" + key + "' must be number"};
                    } else if (type == "boolean" && !value.is_boolean()) {
                        return {false, "Field '" + key + "' must be boolean"};
                    } else if (type == "object" && !value.is_object()) {
                        return {false, "Field '" + key + "' must be object"};
                    } else if (type == "array" && !value.is_array()) {
                        return {false, "Field '" + key + "' must be array"};
                    }
                }
            }
        }

        return {true, ""};
    }

private:
    json schema_;
};

class SqliteTelemetryStore {
public:
    SqliteTelemetryStore(const std::string& db_path,
                         const BackoffConfig& backoff_cfg)
        : db_path_(db_path),
          backoff_cfg_(backoff_cfg),
          db_(nullptr),
          total_success_(0),
          total_failure_(0),
          cumulative_insert_latency_ms_(0.0),
          cumulative_backoff_delay_ms_(0.0),
          wal_checkpoint_interval_(1000),
          insert_count_since_checkpoint_(0) {
        open();
        configureWALMode();
        createSchema();
        prepareStatements();
    }

    ~SqliteTelemetryStore() {
        finalizeStatements();
        if (db_) {
            sqlite3_close(db_);
        }
    }

    bool insertBatch(const std::vector<TelemetryMessage>& batch) {
        auto start_time = std::chrono::steady_clock::now();
        std::size_t attempt = 0;
        std::size_t delay_ms = backoff_cfg_.initial_delay_ms;

        while (attempt <= backoff_cfg_.max_retries) {
            int rc = insertBatchTransaction(batch);
            if (rc == SQLITE_OK) {
                auto end_time = std::chrono::steady_clock::now();
                double latency_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
                cumulative_insert_latency_ms_ += latency_ms;
                total_success_ += batch.size();
                insert_count_since_checkpoint_ += batch.size();
                if (insert_count_since_checkpoint_ >= wal_checkpoint_interval_) {
                    runWalCheckpoint();
                    insert_count_since_checkpoint_ = 0;
                }
                return true;
            }

            // Exponential backoff on conflict or busy
            if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
                attempt++;
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
                cumulative_backoff_delay_ms_ += static_cast<double>(delay_ms);
                delay_ms = static_cast<std::size_t>(
                    std::min<double>(backoff_cfg_.max_delay_ms,
                                     delay_ms * backoff_cfg_.backoff_factor));
                continue;
            } else {
                // Non-retryable error
                total_failure_ += batch.size();
                return false;
            }
        }

        total_failure_ += batch.size();
        return false;
    }

    bool insertDeadLetter(const TelemetryMessage& msg, const std::string& error_reason) {
        if (!stmt_deadletter_insert_) {
            return false;
        }
        std::string received_ts = toIsoTimestamp(msg.received_at);

        sqlite3_reset(stmt_deadletter_insert_);
        sqlite3_clear_bindings(stmt_deadletter_insert_);

        int rc = SQLITE_OK;
        rc = sqlite3_bind_text(stmt_deadletter_insert_, 1, msg.topic.c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) return false;
        rc = sqlite3_bind_text(stmt_deadletter_insert_, 2, msg.payload.c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) return false;
        rc = sqlite3_bind_text(stmt_deadletter_insert_, 3, error_reason.c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) return false;
        rc = sqlite3_bind_text(stmt_deadletter_insert_, 4, received_ts.c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) return false;

        rc = sqlite3_step(stmt_deadletter_insert_);
        sqlite3_reset(stmt_deadletter_insert_);
        return rc == SQLITE_DONE;
    }

    MetricsSnapshot metrics() const {
        MetricsSnapshot m{};
        m.total_db_success = total_success_;
        m.total_db_failure = total_failure_;
        std::size_t total_ops = total_success_ + total_failure_;
        m.avg_insert_latency_ms = total_ops > 0 ? cumulative_insert_latency_ms_ / static_cast<double>(total_ops) : 0.0;
        std::size_t total_backoffs = backoffEvents();
        m.avg_backoff_delay_ms = total_backoffs > 0 ? cumulative_backoff_delay_ms_ / static_cast<double>(total_backoffs) : 0.0;
        return m;
    }

private:
    std::string db_path_;
    BackoffConfig backoff_cfg_;
    sqlite3* db_;
    sqlite3_stmt* stmt_insert_;
    sqlite3_stmt* stmt_deadletter_insert_;

    std::size_t total_success_;
    std::size_t total_failure_;
    double cumulative_insert_latency_ms_;
    double cumulative_backoff_delay_ms_;
    std::size_t wal_checkpoint_interval_;
    std::size_t insert_count_since_checkpoint_;
    std::size_t backoff_events_ = 0;

    void open() {
        int rc = sqlite3_open(db_path_.c_str(), &db_);
        if (rc != SQLITE_OK) {
            std::cerr << "Cannot open SQLite DB: " << sqlite3_errmsg(db_) << std::endl;
            db_ = nullptr;
        }
    }

    void configureWALMode() {
        if (!db_) return;
        char* errmsg = nullptr;
        int rc = sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to set WAL mode: " << (errmsg ? errmsg : "") << std::endl;
            sqlite3_free(errmsg);
        }

        rc = sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to set synchronous mode: " << (errmsg ? errmsg : "") << std::endl;
            sqlite3_free(errmsg);
        }
    }

    void createSchema() {
        if (!db_) return;
        const char* create_main =
            "CREATE TABLE IF NOT EXISTS telemetry ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  topic TEXT NOT NULL,"
            "  payload TEXT NOT NULL,"
            "  received_ts TEXT NOT NULL"
            ");";

        const char* create_deadletter =
            "CREATE TABLE IF NOT EXISTS telemetry_deadletter ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  topic TEXT NOT NULL,"
            "  payload TEXT NOT NULL,"
            "  error_reason TEXT NOT NULL,"
            "  received_ts TEXT NOT NULL"
            ");";

        const char* create_last_ts =
            "CREATE TABLE IF NOT EXISTS telemetry_meta ("
            "  key TEXT PRIMARY KEY,"
            "  value TEXT NOT NULL"
            ");";

        const char* init_last_ts =
            "INSERT OR IGNORE INTO telemetry_meta(key, value) VALUES('last_telemetry_timestamp', '');";

        const char* trigger_update_last_ts =
            "CREATE TRIGGER IF NOT EXISTS trg_update_last_telemetry_ts "
            "AFTER INSERT ON telemetry "
            "BEGIN "
            "  UPDATE telemetry_meta "
            "  SET value = NEW.received_ts "
            "  WHERE key = 'last_telemetry_timestamp' "
            "    AND (value = '' OR value < NEW.received_ts); "
            "END;";

        execSQL(create_main);
        execSQL(create_deadletter);
        execSQL(create_last_ts);
        execSQL(init_last_ts);
        execSQL(trigger_update_last_ts);
    }

    void execSQL(const char* sql) {
        char* errmsg = nullptr;
        int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::cerr << "SQLite exec error: " << (errmsg ? errmsg : "") << std::endl;
            sqlite3_free(errmsg);
        }
    }

    void prepareStatements() {
        if (!db_) return;
        const char* insert_sql =
            "INSERT INTO telemetry(topic, payload, received_ts) VALUES(?1, ?2, ?3);";
        int rc = sqlite3_prepare_v2(db_, insert_sql, -1, &stmt_insert_, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare telemetry insert: " << sqlite3_errmsg(db_) << std::endl;
            stmt_insert_ = nullptr;
        }

        const char* deadletter_sql =
            "INSERT INTO telemetry_deadletter(topic, payload, error_reason, received_ts) "
            "VALUES(?1, ?2, ?3, ?4);";
        rc = sqlite3_prepare_v2(db_, deadletter_sql, -1, &stmt_deadletter_insert_, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare deadletter insert: " << sqlite3_errmsg(db_) << std::endl;
            stmt_deadletter_insert_ = nullptr;
        }
    }

    void finalizeStatements() {
        if (stmt_insert_) {
            sqlite3_finalize(stmt_insert_);
            stmt_insert_ = nullptr;
        }
        if (stmt_deadletter_insert_) {
            sqlite3_finalize(stmt_deadletter_insert_);
            stmt_deadletter_insert_ = nullptr;
        }
    }

    static std::string toIsoTimestamp(const std::chrono::system_clock::time_point& tp) {
        std::time_t t = std::chrono::system_clock::to_time_t(tp);
        std::tm tm{};
#if defined(_WIN32) || defined(_WIN64)
        gmtime_s(&tm, &t);
#else
        gmtime_r(&t, &tm);
#endif
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                      tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                      tm.tm_hour, tm.tm_min, tm.tm_sec);
        return std::string(buf);
    }

    int insertBatchTransaction(const std::vector<TelemetryMessage>& batch) {
        if (!db_ || !stmt_insert_) return SQLITE_ERROR;
        char* errmsg = nullptr;
        int rc = sqlite3_exec(db_, "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            if (errmsg) {
                std::cerr << "BEGIN TRANSACTION failed: " << errmsg << std::endl;
                sqlite3_free(errmsg);
            }
            if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
                backoff_events_ += 1;
            }
            return rc;
        }

        for (const auto& msg : batch) {
            sqlite3_reset(stmt_insert_);
            sqlite3_clear_bindings(stmt_insert_);

            std::string received_ts = toIsoTimestamp(msg.received_at);

            rc = sqlite3_bind_text(stmt_insert_, 1, msg.topic.c_str(), -1, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) {
                std::cerr << "Bind topic failed: " << sqlite3_errmsg(db_) << std::endl;
                sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
                return rc;
            }
            rc = sqlite3_bind_text(stmt_insert_, 2, msg.payload.c_str(), -1, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) {
                std::cerr << "Bind payload failed: " << sqlite3_errmsg(db_) << std::endl;
                sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
                return rc;
            }
            rc = sqlite3_bind_text(stmt_insert_, 3, received_ts.c_str(), -1, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) {
                std::cerr << "Bind ts failed: " << sqlite3_errmsg(db_) << std::endl;
                sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
                return rc;
            }

            rc = sqlite3_step(stmt_insert_);
            if (rc != SQLITE_DONE) {
                std::cerr << "Insert step failed: " << sqlite3_errmsg(db_) << std::endl;
                sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
                if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
                    backoff_events_ += 1;
                }
                return rc;
            }
        }

        rc = sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            if (errmsg) {
                std::cerr << "COMMIT failed: " << errmsg << std::endl;
                sqlite3_free(errmsg);
            }
            if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
                backoff_events_ += 1;
            }
        }
        return rc;
    }

    void runWalCheckpoint() {
        if (!db_) return;
        char* errmsg = nullptr;
        int rc = sqlite3_exec(db_, "PRAGMA wal_checkpoint(FULL);", nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::cerr << "WAL checkpoint failed: " << (errmsg ? errmsg : "") << std::endl;
            sqlite3_free(errmsg);
        }
    }

    std::size_t backoffEvents() const {
        return backoff_events_;
    }
};

class TelemetryBuffer {
public:
    explicit TelemetryBuffer(const BatchConfig& cfg)
        : cfg_(cfg),
          stop_(false),
          total_received_(0),
          total_valid_(0),
          total_invalid_(0) {}

    void push(const TelemetryMessage& msg, bool valid) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(msg);
            total_received_++;
            if (valid) {
                total_valid_++;
            } else {
                total_invalid_++;
            }
        }
        cv_.notify_one();
    }

    std::vector<TelemetryMessage> popBatch() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock,
                     std::chrono::milliseconds(cfg_.flush_interval_ms),
                     [&]() { return stop_ || !queue_.empty(); });
        std::vector<TelemetryMessage> batch;
        if (queue_.empty()) {
            return batch;
        }
        std::size_t count = 0;
        while (!queue_.empty() && count < cfg_.max_batch_size) {
            batch.push_back(queue_.front());
            queue_.pop();
            count++;
        }
        return batch;
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
    }

    std::size_t totalReceived() const { return total_received_; }
    std::size_t totalValid() const { return total_valid_; }
    std::size_t totalInvalid() const { return total_invalid_; }

private:
    BatchConfig cfg_;
    std::queue<TelemetryMessage> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_;
    std::size_t total_received_;
    std::size_t total_valid_;
    std::size_t total_invalid_;
};

class MqttSqliteTelemetrySidecar : public virtual mqtt::callback,
                                  public virtual mqtt::iaction_listener {
public:
    MqttSqliteTelemetrySidecar(const std::string& broker_uri,
                               const std::string& client_id,
                               const std::string& topic_filter,
                               const std::string& db_path,
                               const json& schema,
                               const BatchConfig& batch_cfg,
                               const BackoffConfig& backoff_cfg)
        : broker_uri_(broker_uri),
          client_id_(client_id),
          topic_filter_(topic_filter),
          cli_(broker_uri_, client_id_),
          conn_opts_(),
          validator_(schema),
          store_(db_path, backoff_cfg),
          buffer_(batch_cfg),
          running_(false),
          metrics_thread_running_(false) {

        conn_opts_.set_clean_session(true);
        cli_.set_callback(*this);
    }

    ~MqttSqliteTelemetrySidecar() {
        stop();
    }

    void start() {
        mqtt::token_ptr conntok = cli_.connect(conn_opts_, nullptr, *this);
        conntok->wait(); // synchronous connect wait
        cli_.subscribe(topic_filter_, 1, nullptr, *this);
        running_ = true;
        flusher_thread_ = std::thread(&MqttSqliteTelemetrySidecar::flusherLoop, this);
        metrics_thread_running_ = true;
        metrics_thread_ = std::thread(&MqttSqliteTelemetrySidecar::metricsLoop, this);
    }

    void stop() {
        if (!running_) return;
        running_ = false;
        buffer_.stop();
        if (flusher_thread_.joinable()) {
            flusher_thread_.join();
        }
        metrics_thread_running_ = false;
        if (metrics_thread_.joinable()) {
            metrics_thread_.join();
        }
        try {
            cli_.disconnect()->wait();
        } catch (const mqtt::exception& ex) {
            std::cerr << "MQTT disconnect error: " << ex.what() << std::endl;
        }
    }

    MetricsSnapshot currentMetrics() const {
        MetricsSnapshot m = store_.metrics();
        m.total_received = buffer_.totalReceived();
        m.total_valid = buffer_.totalValid();
        m.total_invalid = buffer_.totalInvalid();
        return m;
    }

    // MQTT callback overrides
    void connection_lost(const std::string& cause) override {
        std::cerr << "Connection lost: " << cause << std::endl;
        // attempt reconnect with simple backoff
        std::size_t delay_ms = 250;
        for (int i = 0; i < 10; ++i) {
            try {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
                cli_.connect(conn_opts_, nullptr, *this)->wait();
                cli_.subscribe(topic_filter_, 1, nullptr, *this);
                std::cerr << "Reconnected successfully" << std::endl;
                return;
            } catch (const mqtt::exception& ex) {
                std::cerr << "Reconnect attempt failed: " << ex.what() << std::endl;
                delay_ms = std::min<std::size_t>(5000, delay_ms * 2);
            }
        }
        std::cerr << "Failed to reconnect after multiple attempts" << std::endl;
    }

    void message_arrived(mqtt::const_message_ptr msg) override {
        TelemetryMessage tm;
        tm.topic = msg->get_topic();
        tm.payload = msg->to_string();
        tm.received_at = std::chrono::system_clock::now();

        bool valid = false;
        try {
            json j = json::parse(tm.payload);
            ValidationResult res = validator_.validate(j);
            if (res.valid) {
                valid = true;
                buffer_.push(tm, true);
            } else {
                buffer_.push(tm, false);
                store_.insertDeadLetter(tm, res.error);
            }
        } catch (const json::parse_error& e) {
            store_.insertDeadLetter(tm, std::string("JSON parse error: ") + e.what());
            buffer_.push(tm, false);
        }
    }

    void delivery_complete(mqtt::delivery_token_ptr) override {
        // no-op; we are only subscribing
    }

    // iaction_listener overrides
    void on_failure(const mqtt::token& tok) override {
        std::cerr << "MQTT operation failed, token: " << tok.get_message_id() << std::endl;
    }

    void on_success(const mqtt::token& tok) override {
        // can log subscription/connection success
        (void)tok;
    }

private:
    std::string broker_uri_;
    std::string client_id_;
    std::string topic_filter_;

    mqtt::async_client cli_;
    mqtt::connect_options conn_opts_;

    JsonSchemaValidator validator_;
    SqliteTelemetryStore store_;
    TelemetryBuffer buffer_;

    std::thread flusher_thread_;
    std::atomic<bool> running_;

    std::thread metrics_thread_;
    std::atomic<bool> metrics_thread_running_;

    void flusherLoop() {
        while (running_) {
            auto batch = buffer_.popBatch();
            if (batch.empty()) {
                continue;
            }
            bool ok = store_.insertBatch(batch);
            if (!ok) {
                std::cerr << "Batch insert failed; messages dropped or retried internally" << std::endl;
            }
        }
    }

    void metricsLoop() {
        using namespace std::chrono_literals;
        while (metrics_thread_running_) {
            std::this_thread::sleep_for(5s);
            MetricsSnapshot m = currentMetrics();
            std::cerr << "[metrics] received=" << m.total_received
                      << " valid=" << m.total_valid
                      << " invalid=" << m.total_invalid
                      << " db_success=" << m.total_db_success
                      << " db_failure=" << m.total_db_failure
                      << " avg_insert_latency_ms=" << m.avg_insert_latency_ms
                      << " avg_backoff_delay_ms=" << m.avg_backoff_delay_ms
                      << std::endl;
        }
    }
};

// Optional gRPC/proto bridge skeleton: for simplicity here we expose a plain TCP socket
// that streams latest metrics in JSON form. This avoids external dependencies but can be
// wrapped by a gRPC microservice externally if desired.

class MetricsTcpBridge {
public:
    MetricsTcpBridge(MqttSqliteTelemetrySidecar& sidecar,
                     unsigned int interval_ms)
        : sidecar_(sidecar),
          interval_ms_(interval_ms),
          running_(false) {}

    void start() {
        running_ = true;
        thread_ = std::thread(&MetricsTcpBridge::loop, this);
    }

    void stop() {
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
    }

private:
    MqttSqliteTelemetrySidecar& sidecar_;
    unsigned int interval_ms_;
    std::atomic<bool> running_;
    std::thread thread_;

    void loop() {
        while (running_) {
            MetricsSnapshot m = sidecar_.currentMetrics();
            json j;
            j["total_received"] = m.total_received;
            j["total_valid"] = m.total_valid;
            j["total_invalid"] = m.total_invalid;
            j["db_success"] = m.total_db_success;
            j["db_failure"] = m.total_db_failure;
            j["avg_insert_latency_ms"] = m.avg_insert_latency_ms;
            j["avg_backoff_delay_ms"] = m.avg_backoff_delay_ms;

            // For now, emit to stderr; in production, this would write to a Unix socket
            // or TCP port for external dashboards/gRPC services to consume.
            std::cerr << "[metrics-json] " << j.dump() << std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
        }
    }
};

} // namespace telemetry
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::telemetry;

    std::string broker_uri = "tcp://localhost:1883";
    std::string client_id = "prometheus_praxis_telemetry_sidecar";
    std::string topic_filter = "cyboquatic/+/telemetry";
    std::string db_path = "telemetry.db";

    if (argc > 1) {
        broker_uri = argv[1];
    }
    if (argc > 2) {
        db_path = argv[2];
    }

    // Example JSON schema used for validation; in a real deployment this would be loaded from config.
    json schema = {
        {"type", "object"},
        {"required", {"device_id", "timestamp", "sensors"}},
        {"properties", {
            {"device_id", {{"type", "string"}}},
            {"timestamp", {{"type", "string"}}},
            {"sensors", {{"type", "object"}}}
        }}
    };

    BatchConfig batch_cfg{64, 500}; // max 64 messages per batch, flush every 500ms
    BackoffConfig backoff_cfg{50, 2000, 2.0, 6}; // 50ms initial, up to 2s, factor 2, 6 retries

    try {
        MqttSqliteTelemetrySidecar sidecar(
            broker_uri,
            client_id,
            topic_filter,
            db_path,
            schema,
            batch_cfg,
            backoff_cfg
        );

        sidecar.start();

        MetricsTcpBridge bridge(sidecar, 2000);
        bridge.start();

        // Simple run loop: in a container this would be replaced by signal handling.
        std::cout << "MQTT->SQLite telemetry sidecar running. Press Ctrl+C to exit." << std::endl;
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }

        bridge.stop();
        sidecar.stop();
    } catch (const mqtt::exception& ex) {
        std::cerr << "MQTT error: " << ex.what() << std::endl;
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Unhandled error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
