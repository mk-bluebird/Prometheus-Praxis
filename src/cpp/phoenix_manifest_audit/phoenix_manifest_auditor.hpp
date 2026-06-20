// filename: src/cpp/phoenix_manifest_audit/phoenix_manifest_auditor.hpp
// repo: mk-bluebird/eco_restoration_shard

#pragma once

#include <string>

namespace phoenix_manifest_audit {

struct AuditConfig {
    std::string sqlite_path;
};

class PhoenixManifestAuditor {
public:
    explicit PhoenixManifestAuditor(const AuditConfig& config);

    int run_audit();

private:
    AuditConfig config_;

    void audit_row(
        const std::string& stewarddid,
        const std::string& regioncode,
        const std::string& dayutc,
        double rohmaxday,
        int rohok,
        int kerdeployableday,
        int lyapunovokday);
};

} // namespace phoenix_manifest_audit
