// Repository: mk-bluebird/Prometheus-Praxis
// Filename: cpp/door_interlock/DoorInterlock.cpp
// Destination: cpp/door_interlock/

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <sstream>
#include <stdexcept>
#include <vector>

/**
 * DoorInterlock – Safety evaluator and audit logger for automatic doors.
 *
 * Implements the non‑offsettable invariants required for closing:
 *   - No obstacle detected
 *   - No forced entry/exit signal
 *   - Measured resistance ≤ threshold
 *   - Sensor evidence is sufficient (sensors calibrated and operational)
 *
 * It also enforces identity‑neutral and accessibility‑by‑design constraints:
 *   - No biometric classification used
 *   - Equal safety requirements for all body types
 *   - Protection for augmented limbs and assistive devices
 *
 * All consequential decisions (close, hold, reverse, open) are recorded
 * with KER scoring and a full audit trail.
 */
class DoorInterlock {
public:
    // Motion request types
    enum class Motion {
        OPEN,
        HOLD_OPEN,
        REVERSE,
        CLOSE
    };

    // Outcome of a safety evaluation
    enum class SafetyOutcome {
        SAFE_CLOSE,          // Safe to close
        UNSAFE_CLOSE,        // Unsafe: one or more invariants failed
        RECORDED_NON_CLOSE   // No close requested, just logged
    };

    /**
     * Constructor.
     * @param door_id          Unique identifier for this door
     * @param resistance_floor Maximum allowed resistance (Newtons) for a safe close
     */
    DoorInterlock(const std::string& door_id, double resistance_floor = 50.0)
        : door_id_(door_id),
          resistance_threshold_newtons_(resistance_floor),
          obstacle_detected_(false),
          forced_signal_detected_(false),
          resistance_newtons_(0.0),
          sensor_evidence_sufficient_(true),
          log_file_("door_interlock_audit.csv")
    {
        // Open log file with header if not exists
        std::ifstream check(log_file_);
        if (!check.good()) {
            std::ofstream header(log_file_);
            if (header.is_open()) {
                header << "timestamp,door_id,event_id,requested_motion,obstacle,forced,resistance,threshold,sensor_ok,knowledge_factor,eco_impact,harm_risk,outcome\n";
            }
        }
    }

    /**
     * Main entry point: process a door motion request.
     * This is a non‑actuating evaluator – it only decides whether the requested
     * motion is safe and logs the decision; it does not physically control the door.
     *
     * @param requested_motion  Desired motion (OPEN, HOLD_OPEN, REVERSE, CLOSE)
     * @param obstacle          True if an obstacle is currently detected
     * @param forced_signal     True if a forced‑entry/exit signal is active
     * @param resistance        Measured mechanical resistance (Newtons)
     * @param sensor_ok         True if all sensors are calibrated and operational
     * @param knowledge_factor  Evidence quality score (0‑1)
     * @param eco_impact        Ecological/public‑good benefit score (0‑1)
     * @param harm_risk         Estimated risk of harm (0‑1)
     * @return                 SafetyOutcome indicating if close is safe, or a non‑close event
     */
    SafetyOutcome evaluateAndLog(
        Motion requested_motion,
        bool obstacle,
        bool forced_signal,
        double resistance,
        bool sensor_ok,
        double knowledge_factor,
        double eco_impact,
        double harm_risk
    ) {
        // Update internal state
        obstacle_detected_ = obstacle;
        forced_signal_detected_ = forced_signal;
        resistance_newtons_ = resistance;
        sensor_evidence_sufficient_ = sensor_ok;

        // Build event record
        std::string event_id = generateEventId();
        uint64_t observed_at_ms = currentTimeMillis();
        uint64_t created_at_epoch = currentTimeSeconds();

        // KER values are already provided; we just pass them through.
        // The invariants ensure they are within [0,1] – callers must enforce.

        // Determine outcome based on invariants
        SafetyOutcome outcome = SafetyOutcome::RECORDED_NON_CLOSE;
        bool safe_close = false;

        if (requested_motion == Motion::CLOSE) {
            safe_close = isCloseSafe();
            outcome = safe_close ? SafetyOutcome::SAFE_CLOSE : SafetyOutcome::UNSAFE_CLOSE;
        } else {
            // Non‑close requests are always recorded (but may have safety implications)
            outcome = SafetyOutcome::RECORDED_NON_CLOSE;
        }

        // Log to audit trail
        logEvent(event_id, observed_at_ms, requested_motion, safe_close, outcome,
                 knowledge_factor, eco_impact, harm_risk, created_at_epoch);

        // If close was requested but unsafe, we raise an exception or return a failure.
        // In a real system, this would trigger a hardware interlock or human review.
        if (requested_motion == Motion::CLOSE && !safe_close) {
            std::ostringstream oss;
            oss << "Door close REJECTED: "
                << "obstacle=" << obstacle_detected_
                << ", forced=" << forced_signal_detected_
                << ", resistance=" << resistance_newtons_
                << " > " << resistance_threshold_newtons_
                << ", sensor_ok=" << sensor_evidence_sufficient_;
            throw std::runtime_error(oss.str());
        }

        return outcome;
    }

    // ---------- Helper accessors ----------
    bool isObstacleDetected() const { return obstacle_detected_; }
    bool isForcedSignalDetected() const { return forced_signal_detected_; }
    double getResistance() const { return resistance_newtons_; }
    double getResistanceThreshold() const { return resistance_threshold_newtons_; }
    bool isSensorEvidenceSufficient() const { return sensor_evidence_sufficient_; }

private:
    std::string door_id_;
    double resistance_threshold_newtons_;
    bool obstacle_detected_;
    bool forced_signal_detected_;
    double resistance_newtons_;
    bool sensor_evidence_sufficient_;
    std::string log_file_;
    std::mutex log_mutex_;

    /**
     * Safety invariant: a close is permitted only if:
     *   - No obstacle
     *   - No forced signal
     *   - Resistance ≤ threshold
     *   - Sensor evidence is sufficient
     * These are non‑offsettable: all must be true.
     */
    bool isCloseSafe() const {
        return !obstacle_detected_
            && !forced_signal_detected_
            && resistance_newtons_ <= resistance_threshold_newtons_
            && sensor_evidence_sufficient_;
    }

    // ---------- Event ID / Timestamp utilities ----------
    std::string generateEventId() const {
        // Simple UUID‑like generator (not cryptographically strong, but unique enough for demo)
        static uint64_t counter = 0;
        uint64_t seq = ++counter;
        std::ostringstream oss;
        oss << "EVT-" << std::hex << std::setw(16) << std::setfill('0') << seq;
        return oss.str();
    }

    uint64_t currentTimeMillis() const {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        );
        return ms.count();
    }

    uint64_t currentTimeSeconds() const {
        auto now = std::chrono::system_clock::now();
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()
        );
        return sec.count();
    }

    // ---------- Audit logging ----------
    void logEvent(
        const std::string& event_id,
        uint64_t observed_at_ms,
        Motion motion,
        bool safe_close,
        SafetyOutcome outcome,
        double k,
        double e,
        double r,
        uint64_t created_at_epoch
    ) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        std::ofstream log(log_file_, std::ios::app);
        if (!log.is_open()) {
            std::cerr << "Warning: unable to open audit log file: " << log_file_ << std::endl;
            return;
        }

        // Convert motion to string
        std::string motion_str;
        switch (motion) {
            case Motion::OPEN:      motion_str = "OPEN"; break;
            case Motion::HOLD_OPEN: motion_str = "HOLD_OPEN"; break;
            case Motion::REVERSE:   motion_str = "REVERSE"; break;
            case Motion::CLOSE:     motion_str = "CLOSE"; break;
        }

        std::string outcome_str;
        switch (outcome) {
            case SafetyOutcome::SAFE_CLOSE:         outcome_str = "ACCEPTED_CLOSE"; break;
            case SafetyOutcome::UNSAFE_CLOSE:       outcome_str = "REJECTED_CLOSE"; break;
            case SafetyOutcome::RECORDED_NON_CLOSE: outcome_str = "RECORDED_NON_CLOSE"; break;
        }

        // CSV format: timestamp,door_id,event_id,requested_motion,obstacle,forced,resistance,threshold,sensor_ok,knowledge_factor,eco_impact,harm_risk,outcome
        log << observed_at_ms << ","
            << door_id_ << ","
            << event_id << ","
            << motion_str << ","
            << (obstacle_detected_ ? 1 : 0) << ","
            << (forced_signal_detected_ ? 1 : 0) << ","
            << std::fixed << std::setprecision(3) << resistance_newtons_ << ","
            << resistance_threshold_newtons_ << ","
            << (sensor_evidence_sufficient_ ? 1 : 0) << ","
            << k << "," << e << "," << r << ","
            << outcome_str << "\n";

        log.close();
    }
};

// ---------- Demonstration / Test harness ----------
int main() {
    try {
        DoorInterlock door("door-main-entrance", 50.0);

        // Simulate a safe close request
        DoorInterlock::SafetyOutcome result = door.evaluateAndLog(
            DoorInterlock::Motion::CLOSE,
            false,   // no obstacle
            false,   // no forced signal
            10.0,    // resistance (N) within threshold
            true,    // sensors OK
            0.95,    // knowledge factor
            0.80,    // eco impact
            0.10     // harm risk
        );
        std::cout << "Safe close request outcome: " << static_cast<int>(result) << std::endl;

        // Simulate an unsafe close request (obstacle detected)
        try {
            door.evaluateAndLog(
                DoorInterlock::Motion::CLOSE,
                true,    // obstacle detected
                false,
                5.0,
                true,
                0.90,
                0.75,
                0.15
            );
        } catch (const std::runtime_error& e) {
            std::cerr << "Expected error: " << e.what() << std::endl;
        }

        // Simulate a non‑close request (e.g., hold open)
        DoorInterlock::SafetyOutcome holdResult = door.evaluateAndLog(
            DoorInterlock::Motion::HOLD_OPEN,
            false,
            false,
            15.0,
            true,
            0.88,
            0.60,
            0.05
        );
        std::cout << "Hold open request outcome: " << static_cast<int>(holdResult) << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
