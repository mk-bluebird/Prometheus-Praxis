// fuse_box.cpp
#include "fuse_box.hpp"
// Include GPIO and SQLite headers appropriate to platform.

FuseBox::FuseBox(const GovernanceConfig& cfg)
    : cfg_(cfg), relayEnergized_(false),
      lastWatchdogKick_(std::chrono::steady_clock::now()) {
    // Ensure relay is de-energized if non-actuating-only.
    if (cfg_.nonActuatingOnly) {
        setRelay(false);
    }
    kickWatchdog();
}

void FuseBox::tick() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - lastWatchdogKick_;
    // Kick watchdog periodically when in safe state.
    if (elapsed > std::chrono::seconds(1)) {
        kickWatchdog();
        lastWatchdogKick_ = now;
    }
}

bool FuseBox::requestActuation(const std::string& actuatorId,
                               const std::string& command) {
    if (cfg_.nonActuatingOnly) {
        // Governance violation: log and trip relay.
        logAudit(actuatorId, command, "DENY_NON_ACTUATING_ONLY");
        setRelay(false); // drop power
        // Optionally: raise signal / terminate process.
        return false;
    }

    // If we reach here, governance allows actuation.
    // But fuse-box still centralizes control: only this method can energize relay.
    setRelay(true);
    logAudit(actuatorId, command, "ALLOW");
    // Actual actuator command is sent via a separate, lower-level driver.
    return true;
}

void FuseBox::setRelay(bool energized) {
    // Platform-specific GPIO control.
    // Example: write to /sys/class/gpio/gpioX/value or use a GPIO library.
    relayEnergized_ = energized;
}

void FuseBox::kickWatchdog() {
    // Toggle a GPIO line or send a PMIC message to show we are alive.
}

void FuseBox::logAudit(const std::string& actuatorId,
                       const std::string& command,
                       const std::string& outcome) {
    // INSERT INTO actuator_audit(deployment_id, actuator_id, command,
    //                            outcome, non_actuating_only, evidence_hex, timestamp)
    // using a prepared statement.
}
