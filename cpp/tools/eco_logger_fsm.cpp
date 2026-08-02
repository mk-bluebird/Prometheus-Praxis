// File: cpp/tools/eco_logger_fsm.cpp
#include <iostream>
#include <string>
#include <unordered_map>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

// Simple eco-impact tracker per entity.
struct EcoImpactRecord {
    double last_impact;      // last recorded eco-impact score (0..1)
    double previous_impact;  // previous score used for change calculation
    std::chrono::system_clock::time_point last_update;
};

// Finite-state machine for eco_logger alerting.
class EcoLoggerFSM {
public:
    enum class LogLevel {
        INFO,
        WARN,
        ERROR,
        CRITICAL
    };

    EcoLoggerFSM()
        : current_level_(LogLevel::INFO)
    {}

    void update_entity(const std::string& entity_id, double new_impact) {
        auto now = std::chrono::system_clock::now();
        EcoImpactRecord& rec = records_[entity_id];

        rec.previous_impact = rec.last_impact;
        rec.last_impact = new_impact;
        rec.last_update = now;

        double change_pct = compute_change_percentage(rec);
        bool within_24h = is_within_24_hours(rec);

        if (within_24h && change_pct <= -20.0) {
            escalate(entity_id, rec.last_impact, change_pct);
        } else {
            log(LogLevel::INFO, entity_id,
                "Eco-impact update; change_pct=" + format_double(change_pct) + "%");
        }
    }

    // For external integration: expose current log level.
    LogLevel current_level() const {
        return current_level_;
    }

private:
    std::unordered_map<std::string, EcoImpactRecord> records_;
    LogLevel current_level_;

    static double compute_change_percentage(const EcoImpactRecord& rec) {
        if (rec.previous_impact <= 0.0) {
            return 0.0;
        }
        double delta = rec.last_impact - rec.previous_impact;
        return (delta / rec.previous_impact) * 100.0;
    }

    static bool is_within_24_hours(const EcoImpactRecord& rec) {
        auto now = std::chrono::system_clock::now();
        auto dt = now - rec.last_update;
        auto hours = std::chrono::duration_cast<std::chrono::hours>(dt).count();
        return hours <= 24;
    }

    void escalate(const std::string& entity_id,
                  double new_impact,
                  double change_pct) {
        // Escalation logic: bump log level one step; max at CRITICAL.
        switch (current_level_) {
            case LogLevel::INFO:
                current_level_ = LogLevel::WARN;
                break;
            case LogLevel::WARN:
                current_level_ = LogLevel::ERROR;
                break;
            case LogLevel::ERROR:
                current_level_ = LogLevel::CRITICAL;
                break;
            case LogLevel::CRITICAL:
                // already at highest level
                break;
        }

        std::string msg = "Eco-impact drop >=20% in 24h for entity "
                          + entity_id +
                          " new_impact=" + format_double(new_impact) +
                          " change_pct=" + format_double(change_pct) + "%";

        log(current_level_, entity_id, msg);
        send_telegram_alert(entity_id, msg);
    }

    static std::string format_double(double value) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3) << value;
        return oss.str();
    }

    static std::string log_level_to_string(LogLevel level) {
        switch (level) {
            case LogLevel::INFO:     return "INFO";
            case LogLevel::WARN:     return "WARN";
            case LogLevel::ERROR:    return "ERROR";
            case LogLevel::CRITICAL: return "CRITICAL";
        }
        return "UNKNOWN";
    }

    void log(LogLevel level,
             const std::string& entity_id,
             const std::string& message) {
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm, &tt);
#else
        localtime_r(&tt, &tm);
#endif
        std::ostringstream ts;
        ts << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");

        std::cout << "[" << ts.str() << "] "
                  << "[" << log_level_to_string(level) << "] "
                  << "[" << entity_id << "] "
                  << message << "\n";
    }

    // Stubbed Telegram alert sender; in real deployment, this would
    // call an HTTP API or a dedicated messaging library.
    void send_telegram_alert(const std::string& entity_id,
                             const std::string& message) {
        std::cout << "TELEGRAM ALERT: entity=" << entity_id
                  << " msg=\"" << message << "\"\n";
    }
};

int main() {
    EcoLoggerFSM logger;

    // Example usage: track two entities' eco-impact over time.
    logger.update_entity("hex-001", 0.80); // initial
    logger.update_entity("hex-001", 0.78); // small drop
    logger.update_entity("hex-001", 0.60); // >20% drop from 0.78

    logger.update_entity("node-washA", 0.92);
    logger.update_entity("node-washA", 0.70); // >20% drop

    return 0;
}
