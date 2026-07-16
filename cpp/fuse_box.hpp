// fuse_box.hpp
#pragma once
#include <string>
#include <chrono>

struct GovernanceConfig {
    bool nonActuatingOnly;
    std::string deploymentId;
    std::string evidenceHex;
};

class FuseBox {
public:
    explicit FuseBox(const GovernanceConfig& cfg);

    // Called periodically to service watchdog and enforce relay policy.
    void tick();

    // Actuation API that Lua or other languages may call.
    // Returns false and trips relay if non-actuating-only is active.
    bool requestActuation(const std::string& actuatorId,
                          const std::string& command);

private:
    GovernanceConfig cfg_;
    bool relayEnergized_;
    std::chrono::steady_clock::time_point lastWatchdogKick_;

    void setRelay(bool energized);       // hardware-specific GPIO control
    void kickWatchdog();                 // PMIC watchdog tick
    void logAudit(const std::string& actuatorId,
                  const std::string& command,
                  const std::string& outcome);  // writes to SQLite
};
