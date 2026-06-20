// filename: src/cpp/phoenix_unified_health/phoenix_unified_health_reporter.hpp
// repo: mk-bluebird/eco_restoration_shard

#pragma once

#include <string>

namespace phoenix_unified_health {

struct ReporterConfig {
    std::string sqlite_path;
    std::string json_output_path;
    std::string csv_output_path;
};

class PhoenixUnifiedHealthReporter {
public:
    explicit PhoenixUnifiedHealthReporter(const ReporterConfig& config);

    int generate_reports();

private:
    ReporterConfig config_;

    void write_csv_header(std::ostream& out) const;
};

} // namespace phoenix_unified_health
