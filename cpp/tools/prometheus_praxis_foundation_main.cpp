// File: cpp/tools/prometheus_praxis_foundation_main.cpp
#include "../eco_restoration/private_heat_membership_threat_model.hpp"
#include "../eco_restoration/water_biodiversity_and_actuation_authorization.hpp"
#include "../eco_restoration/stochastic_invasive_and_anchor_audit.hpp"
#include <numeric>
#include "../eco_restoration/irrigation_mpc_and_equitable_water.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

struct FoundationReport {
    bool private_heat_accepted{};
    bool threat_fail_closed{};
    bool water_biodiversity_allowed{};
    bool water_biodiversity_invariant_holds{};
    bool authorization_accepted{};
    bool invasive_control_safe{};
    bool irrigation_robustly_feasible{};
    double maximum_risk_of_harm{};
    double knowledge_factor{};
    double eco_impact_value{};
    bool foundation_safe{};
};

namespace prometheus_praxis_foundation_extensions {

inline constexpr std::string_view foundation_extension_version{"1.0.0"};

struct FoundationExtensionTraits {
    std::string_view namespace_name;
    std::string_view version;
    std::size_t registry_size;
    bool append_only;
    bool diagnostics_only;
};

static const std::vector<std::string> extension_registry{
    "foundation_report_json",
    "private_heat_gate",
    "threat_containment_gate",
    "water_biodiversity_gate",
    "authorization_gate",
    "invasive_control_gate",
    "irrigation_gate"
};

inline const FoundationExtensionTraits foundation_extension_traits{
    "prometheus_praxis_foundation_extensions",
    foundation_extension_version,
    extension_registry.size(),
    true,
    true
};

bool valid_extension_name(const std::string& name) {
    if (name.empty()) return false;
    for (const char c : name) {
        const bool lower = c >= 'a' && c <= 'z';
        const bool digit = c >= '0' && c <= '9';
        if (!lower && !digit && c != '_') return false;
    }
    return true;
}

bool has_duplicates(const std::vector<std::string>& names) {
    for (std::size_t i = 0; i < names.size(); ++i) {
        for (std::size_t j = i + 1; j < names.size(); ++j) {
            if (names[i] == names[j]) return true;
        }
    }
    return false;
}

bool extension_registry_self_test() {
    if (foundation_extension_version != "1.0.0" ||
        !foundation_extension_traits.append_only ||
        !foundation_extension_traits.diagnostics_only ||
        foundation_extension_traits.registry_size != extension_registry.size() ||
        extension_registry.empty() || has_duplicates(extension_registry)) {
        return false;
    }
    for (const auto& name : extension_registry) {
        if (!valid_extension_name(name)) return false;
    }
    const std::vector<std::string> duplicate{"eco_gate", "eco_gate"};
    const std::vector<std::string> invalid{"eco-gate"};
    return has_duplicates(duplicate) && !valid_extension_name(invalid.front());
}

void json_string(std::ostringstream& out, std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    out << '"';
    for (const unsigned char c : value) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20U) {
                    out << "\\u00" << hex[(c >> 4U) & 15U] << hex[c & 15U];
                } else {
                    out << static_cast<char>(c);
                }
        }
    }
    out << '"';
}

void json_double(std::ostringstream& out, double value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("FoundationReport has non-finite value");
    }
    out << std::fixed << std::setprecision(6) << value;
}

std::string serialize_foundation_report_json(
    const FoundationReport& report,
    std::string_view schema_version = "foundation_report_v1") {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "{\"schema_version\":";
    json_string(out, schema_version);
    out << ",\"private_heat_accepted\":" << (report.private_heat_accepted ? "true" : "false");
    out << ",\"threat_fail_closed\":" << (report.threat_fail_closed ? "true" : "false");
    out << ",\"water_biodiversity_allowed\":" << (report.water_biodiversity_allowed ? "true" : "false");
    out << ",\"water_biodiversity_invariant_holds\":" << (report.water_biodiversity_invariant_holds ? "true" : "false");
    out << ",\"authorization_accepted\":" << (report.authorization_accepted ? "true" : "false");
    out << ",\"invasive_control_safe\":" << (report.invasive_control_safe ? "true" : "false");
    out << ",\"irrigation_robustly_feasible\":" << (report.irrigation_robustly_feasible ? "true" : "false");
    out << ",\"maximum_risk_of_harm\":"; json_double(out, report.maximum_risk_of_harm);
    out << ",\"knowledge_factor\":"; json_double(out, report.knowledge_factor);
    out << ",\"eco_impact_value\":"; json_double(out, report.eco_impact_value);
    out << ",\"foundation_safe\":" << (report.foundation_safe ? "true" : "false") << '}';
    return out.str();
}

bool foundation_report_json_self_test() {
    const FoundationReport report{true, false, true, true, true, true, true,
                                  0.125, 0.875, 0.625, true};
    const std::string expected =
        "{\"schema_version\":\"foundation_report_v1\",\"private_heat_accepted\":true,"
        "\"threat_fail_closed\":false,\"water_biodiversity_allowed\":true,"
        "\"water_biodiversity_invariant_holds\":true,\"authorization_accepted\":true,"
        "\"invasive_control_safe\":true,\"irrigation_robustly_feasible\":true,"
        "\"maximum_risk_of_harm\":0.125000,\"knowledge_factor\":0.875000,"
        "\"eco_impact_value\":0.625000,\"foundation_safe\":true}";
    if (serialize_foundation_report_json(report) != expected) return false;
    std::ostringstream escaped;
    json_string(escaped, "a\"b\\c\n");
    if (escaped.str() != "\"a\\\"b\\\\c\\n\"") return false;
    return serialize_foundation_report_json(report, "eco\"v1").find("eco\\\"v1") != std::string::npos;
}

FoundationReport foundation_self_check() {
    using namespace eco_restoration;

    const auto heat = build_private_heat_statement(
        {100, 7, 1'000'000, 50'000'000, 5'000'000, 16U * 1024U},
        true, true, 0.95);
    const auto threat = assess_ecological_system_threat({
        {ThreatSurface::SensorSpoofing, 0.08, 0.96, 0.99, 0.02},
        {ThreatSurface::ModelPoisoning, 0.05, 0.97, 0.99, 0.01},
        {ThreatSurface::PolicySubstitution, 0.03, 0.98, 0.99, 0.01},
        {ThreatSurface::DelayedActuation, 0.04, 0.95, 0.99, 0.03}}, 0.10);

    const auto water = evaluate_water_biodiversity({500, 1000, 400}, {700000, 600000});
    const bool water_invariant = required_cross_shard_unsat(water);

    ProofCheckedDispatcher dispatcher("policy_eco_safe_v1");
    const AuthorizationEvidence authorization{
        "irrigation_zone_a", "policy_eco_safe_v1", 1000, 2000, 1, 200000, true};
    const bool authorization_accepted = dispatcher.accept(authorization, 1500);

    const StochasticPopulationModel model{100.0, 1.0, 0.05, 0.10, 0.02, 0.01, 0.5, -0.01};
    const std::vector<InvasiveControlCandidate> candidates{
        {0.3, 10.0, 15.0, 0.20}, {0.5, 20.0, 18.0, 0.25}, {0.8, 30.0, 25.0, 0.15}};
    const auto invasive = select_safe_stochastic_invasive_control(model, candidates);

    const IrrigationDynamics dynamics{15.0, 3.0, 0.1, 10.0, 30.0, 12.0, 25.0, 8.0, 0.1, 0.5};
    const auto irrigation = select_robust_irrigation_schedule(
        {{4.0, 3.0, 4.0}, {5.0, 4.0, 5.0}, {3.0, 2.0, 3.0}},
        {{0.6, {2.0, 1.5, 3.0}}, {0.4, {0.5, 0.0, 1.0}}}, dynamics);

    const auto selected = std::find_if(candidates.begin(), candidates.end(),
        [&invasive](const auto& c) { return c.treatment_intensity == invasive.treatment_intensity; });
    const double invasive_risk = selected == candidates.end() ? 1.0 : selected->risk_of_harm;
    const double authorization_risk =
        static_cast<double>(authorization.risk_of_harm_fixed) / 1'000'000.0;
    const double maximum_risk = std::max({threat.estimated_risk_of_harm,
                                          authorization_risk, invasive_risk});
    const double authorization_quality = authorization_accepted ? 1.0 - authorization_risk : 0.0;
    const double knowledge = std::clamp(
        (heat.knowledge_factor + threat.knowledge_factor + water.knowledge_factor +
         authorization_quality + invasive.knowledge_factor + irrigation.knowledge_factor) / 6.0, 0.0, 1.0);
    const double impact = std::clamp(
        (heat.eco_impact_value + threat.eco_impact_value + water.eco_impact_value +
         authorization_quality + invasive.eco_impact_value + irrigation.eco_impact_value) / 6.0, 0.0, 1.0);
    const bool safe = heat.accepted && !threat.fail_closed && water.allow &&
        water_invariant && authorization_accepted && invasive.safe &&
        irrigation.robustly_feasible && maximum_risk <= 0.30;
    return {heat.accepted, threat.fail_closed, water.allow, water_invariant,
            authorization_accepted, invasive.safe, irrigation.robustly_feasible,
            maximum_risk, knowledge, impact, safe};
}

}  // namespace prometheus_praxis_foundation_extensions

int main(int argc, char** argv) {
    using namespace prometheus_praxis_foundation_extensions;
    if (argc != 2) {
        std::cerr << "usage: prometheus_praxis_foundation_main "
                     "--foundation-self-check|--foundation-extension-self-test\n";
        return 64;
    }
    try {
        const std::string_view command(argv[1]);
        if (command == "--foundation-extension-self-test") {
            const bool passed = extension_registry_self_test() && foundation_report_json_self_test();
            std::cout << "foundation_extensions_self_test=" << (passed ? 1 : 0) << '\n';
            return passed ? 0 : 2;
        }
        if (command == "--foundation-self-check") {
            const FoundationReport report = foundation_self_check();
            std::cout << serialize_foundation_report_json(report) << '\n';
            return report.foundation_safe ? 0 : 2;
        }
        std::cerr << "unsupported command\n";
        return 64;
    } catch (const std::exception& error) {
        std::cerr << "foundation engine error: " << error.what() << '\n';
        return 1;
    }
}

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

bool IsStableKey(std::string_view key) {
    if (key.empty()) return false;
    const char first = key.front();
    if (first < 'a' || first > 'z') return false;
    for (const char character : key) {
        const bool lower = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';
        if (!lower && !digit && character != '_') return false;
    }
    return true;
}

void RequireStableKey(std::string_view key) {
    if (!IsStableKey(key)) {
        throw std::invalid_argument("key must be lower_snake_case");
    }
}

void EmitKeyValue(std::ostream& output,
                  std::string_view key,
                  bool value) {
    RequireStableKey(key);
    output << key << '=' << (value ? "true" : "false") << '\n';
}

void EmitKeyValue(std::ostream& output,
                  std::string_view key,
                  double value) {
    RequireStableKey(key);
    if (!std::isfinite(value)) {
        throw std::invalid_argument("key value must be finite");
    }
    output << key << '='
           << std::fixed
           << std::setprecision(6)
           << value
           << '\n';
}

void EmitKeyValue(std::ostream& output,
                  std::string_view key,
                  std::string_view value) {
    RequireStableKey(key);
    output << key << '=' << value << '\n';
}

void EmitKeyValue(std::ostream& output,
                  std::string_view key,
                  long long value) {
    RequireStableKey(key);
    output << key << '=' << value << '\n';
}

void EmitKeyValue(std::ostream& output,
                  std::string_view key,
                  unsigned long long value) {
    RequireStableKey(key);
    output << key << '=' << value << '\n';
}

bool EmitKeyValueSelfTest() {
    std::ostringstream ordered;
    EmitKeyValue(ordered, "alpha_flag", true);
    EmitKeyValue(ordered, "beta_value", 0.125);
    EmitKeyValue(ordered, "gamma_text", "eco_safe");
    EmitKeyValue(ordered, "delta_count", 7LL);

    const std::string expected =
        "alpha_flag=true\n"
        "beta_value=0.125000\n"
        "gamma_text=eco_safe\n"
        "delta_count=7\n";

    if (ordered.str() != expected) return false;

    try {
        std::ostringstream invalid;
        EmitKeyValue(invalid, "InvalidKey", true);
        return false;
    } catch (const std::invalid_argument&) {
    }

    try {
        std::ostringstream invalid;
        EmitKeyValue(invalid, "invalid-key", true);
        return false;
    } catch (const std::invalid_argument&) {
    }

    try {
        std::ostringstream invalid;
        EmitKeyValue(invalid, "", true);
        return false;
    } catch (const std::invalid_argument&) {
    }

    try {
        std::ostringstream invalid;
        EmitKeyValue(invalid, "nonfinite", std::numeric_limits<double>::infinity());
        return false;
    } catch (const std::invalid_argument&) {
    }

    return true;
}

struct UsageCommand {
    std::string_view command;
    std::string_view summary;
};

bool IsUsageLineWidthValid(std::string_view message,
                           std::size_t maximum_columns = 119U) {
    std::size_t line_width = 0;
    for (const char character : message) {
        if (character == '\n') {
            if (line_width > maximum_columns) return false;
            line_width = 0;
        } else {
            ++line_width;
        }
    }
    return line_width <= maximum_columns;
}

bool IsUsageCommandValid(const UsageCommand& command) {
    if (command.command.empty() || command.summary.empty()) return false;
    if (command.command.rfind("--", 0U) != 0U) return false;
    return command.command.find_first_of("\r\n") == std::string_view::npos &&
           command.summary.find_first_of("\r\n") == std::string_view::npos;
}

std::string BuildUsageMessage(
    std::string_view program_name,
    const std::vector<UsageCommand>& future_commands = {}) {
    if (program_name.empty() ||
        program_name.find_first_of("\r\n") != std::string_view::npos) {
        throw std::invalid_argument("program name must be one line");
    }

    std::vector<UsageCommand> commands{
        {"--foundation-self-check", "run all bounded ecological diagnostics"},
        {"--foundation-extension-self-test", "run foundation extension checks"}
    };

    for (const auto& command : future_commands) {
        if (!IsUsageCommandValid(command)) {
            throw std::invalid_argument("usage command is invalid");
        }
        commands.push_back(command);
    }

    std::ostringstream output;
    output << "usage: " << program_name << " <command>\n";
    output << "commands:\n";

    for (const auto& command : commands) {
        const std::string line =
            "  " + std::string(command.command) + "  " +
            std::string(command.summary);
        if (!IsUsageLineWidthValid(line)) {
            throw std::length_error("usage line exceeds 119 columns");
        }
        output << line << '\n';
    }

    const std::string message = output.str();
    if (!IsUsageLineWidthValid(message)) {
        throw std::length_error("usage message exceeds 119 columns");
    }
    return message;
}

bool BuildUsageMessageSelfTest() {
    const std::string expected =
        "usage: foundation <command>\n"
        "commands:\n"
        "  --foundation-self-check  run all bounded ecological diagnostics\n"
        "  --foundation-extension-self-test  run foundation extension checks\n";

    if (BuildUsageMessage("foundation") != expected) return false;

    const std::string extended = BuildUsageMessage(
        "foundation",
        {{"--report-json", "emit bounded aggregate report"}});
    if (extended.find("--report-json") == std::string::npos) return false;
    if (!IsUsageLineWidthValid(extended)) return false;

    try {
        static_cast<void>(BuildUsageMessage(""));
        return false;
    } catch (const std::invalid_argument&) {
    }

    try {
        static_cast<void>(BuildUsageMessage(
            "foundation",
            {{"invalid", "invalid command"}}));
        return false;
    } catch (const std::invalid_argument&) {
    }

    try {
        static_cast<void>(BuildUsageMessage(
            "foundation",
            {{"--future",
              "this intentionally oversized summary exceeds the stable usage "
              "line width requirement and must be rejected by the helper"}}));
        return false;
    } catch (const std::length_error&) {
    }

    return true;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

enum class FoundationExitCode : int {
    Success = 0,
    RuntimeFailure = 1,
    SafetyBlocked = 2,
    InvalidUsage = 64
};

constexpr FoundationExitCode FoundationExitCodeFromSafety(bool foundation_safe) {
    return foundation_safe
        ? FoundationExitCode::Success
        : FoundationExitCode::SafetyBlocked;
}

constexpr int ToPlatformExitCode(FoundationExitCode code) {
    return static_cast<int>(code);
}

constexpr bool IsFoundationExitCode(FoundationExitCode code) {
    switch (code) {
        case FoundationExitCode::Success:
        case FoundationExitCode::RuntimeFailure:
        case FoundationExitCode::SafetyBlocked:
        case FoundationExitCode::InvalidUsage:
            return true;
    }
    return false;
}

constexpr bool IsSuccessExitCode(FoundationExitCode code) {
    return code == FoundationExitCode::Success;
}

constexpr bool IsSafetyExitCode(FoundationExitCode code) {
    return code == FoundationExitCode::SafetyBlocked;
}

constexpr bool IsUsageExitCode(FoundationExitCode code) {
    return code == FoundationExitCode::InvalidUsage;
}

constexpr bool IsRuntimeFailureExitCode(FoundationExitCode code) {
    return code == FoundationExitCode::RuntimeFailure;
}

constexpr std::string_view FoundationExitCodeName(FoundationExitCode code) {
    switch (code) {
        case FoundationExitCode::Success:
            return "success";
        case FoundationExitCode::RuntimeFailure:
            return "runtime_failure";
        case FoundationExitCode::SafetyBlocked:
            return "safety_blocked";
        case FoundationExitCode::InvalidUsage:
            return "invalid_usage";
    }
    return "unknown";
}

constexpr std::string_view FoundationExitCodeDescription(FoundationExitCode code) {
    switch (code) {
        case FoundationExitCode::Success:
            return "all required ecological corridors passed";
        case FoundationExitCode::RuntimeFailure:
            return "a validated runtime error prevented assessment";
        case FoundationExitCode::SafetyBlocked:
            return "an ecological safety corridor prevented acceptance";
        case FoundationExitCode::InvalidUsage:
            return "the command line did not match a supported command";
    }
    return "unknown foundation exit state";
}

constexpr FoundationExitCode FoundationExitCodeFromPlatform(int platform_code) {
    switch (platform_code) {
        case 0:
            return FoundationExitCode::Success;
        case 1:
            return FoundationExitCode::RuntimeFailure;
        case 2:
            return FoundationExitCode::SafetyBlocked;
        case 64:
            return FoundationExitCode::InvalidUsage;
        default:
            return FoundationExitCode::RuntimeFailure;
    }
}

constexpr bool FoundationExitCodeStaticSelfTest() {
    return FoundationExitCodeFromSafety(true) == FoundationExitCode::Success &&
           FoundationExitCodeFromSafety(false) == FoundationExitCode::SafetyBlocked &&
           ToPlatformExitCode(FoundationExitCode::Success) == 0 &&
           ToPlatformExitCode(FoundationExitCode::SafetyBlocked) == 2 &&
           ToPlatformExitCode(FoundationExitCode::InvalidUsage) == 64 &&
           ToPlatformExitCode(FoundationExitCode::RuntimeFailure) == 1 &&
           FoundationExitCodeFromPlatform(0) == FoundationExitCode::Success &&
           FoundationExitCodeFromPlatform(2) == FoundationExitCode::SafetyBlocked &&
           FoundationExitCodeFromPlatform(64) == FoundationExitCode::InvalidUsage &&
           FoundationExitCodeFromPlatform(1) == FoundationExitCode::RuntimeFailure &&
           IsSuccessExitCode(FoundationExitCode::Success) &&
           IsSafetyExitCode(FoundationExitCode::SafetyBlocked) &&
           IsUsageExitCode(FoundationExitCode::InvalidUsage) &&
           IsRuntimeFailureExitCode(FoundationExitCode::RuntimeFailure) &&
           FoundationExitCodeName(FoundationExitCode::Success) == "success";
}

static_assert(FoundationExitCodeStaticSelfTest());

bool FoundationExitCodeSelfTest() {
    const std::vector<FoundationExitCode> codes{
        FoundationExitCode::Success,
        FoundationExitCode::RuntimeFailure,
        FoundationExitCode::SafetyBlocked,
        FoundationExitCode::InvalidUsage
    };

    for (const FoundationExitCode code : codes) {
        if (!IsFoundationExitCode(code)) return false;
        if (FoundationExitCodeFromPlatform(ToPlatformExitCode(code)) != code) {
            return false;
        }
        if (FoundationExitCodeName(code).empty() ||
            FoundationExitCodeDescription(code).empty()) {
            return false;
        }
    }

    if (FoundationExitCodeFromSafety(true) != FoundationExitCode::Success) {
        return false;
    }
    if (FoundationExitCodeFromSafety(false) != FoundationExitCode::SafetyBlocked) {
        return false;
    }
    if (FoundationExitCodeFromPlatform(255) != FoundationExitCode::RuntimeFailure) {
        return false;
    }
    return true;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

constexpr std::int32_t foundation_risk_fixed_scale = 1'000'000;
constexpr std::int32_t foundation_risk_fixed_limit = 300'000;

double RiskOfHarmFromFixed(std::int32_t fixed_risk) {
    if (fixed_risk < 0 || fixed_risk > foundation_risk_fixed_scale) {
        throw std::invalid_argument("fixed risk of harm must lie in [0,1000000]");
    }
    return static_cast<double>(fixed_risk) /
           static_cast<double>(foundation_risk_fixed_scale);
}

std::int32_t RiskOfHarmToFixed(double risk_of_harm) {
    if (!std::isfinite(risk_of_harm) ||
        risk_of_harm < 0.0 || risk_of_harm > 1.0) {
        throw std::invalid_argument("risk of harm must lie in [0,1]");
    }

    const double scaled =
        risk_of_harm * static_cast<double>(foundation_risk_fixed_scale);
    const long long rounded = std::llround(scaled);

    if (rounded < 0 ||
        rounded > static_cast<long long>(foundation_risk_fixed_scale)) {
        throw std::invalid_argument("risk conversion exceeded fixed-point domain");
    }
    return static_cast<std::int32_t>(rounded);
}

bool IsSafeRiskOfHarm(double risk_of_harm) {
    if (!std::isfinite(risk_of_harm) ||
        risk_of_harm < 0.0 || risk_of_harm > 1.0) {
        return false;
    }
    return risk_of_harm <= 0.30;
}

bool IsSafeRiskOfHarmFixed(std::int32_t fixed_risk) {
    if (fixed_risk < 0 || fixed_risk > foundation_risk_fixed_scale) {
        return false;
    }
    return fixed_risk <= foundation_risk_fixed_limit;
}

double SelectedInvasiveRiskOfHarm(
    const std::vector<eco_restoration::InvasiveControlCandidate>& candidates,
    const eco_restoration::StochasticControlDecision& selected) {
    if (!selected.safe || !std::isfinite(selected.treatment_intensity)) {
        throw std::invalid_argument("selected invasive decision is not safe");
    }

    for (const auto& candidate : candidates) {
        if (candidate.treatment_intensity == selected.treatment_intensity) {
            if (!std::isfinite(candidate.risk_of_harm) ||
                candidate.risk_of_harm < 0.0 ||
                candidate.risk_of_harm > 1.0) {
                throw std::invalid_argument("selected invasive risk is invalid");
            }
            return candidate.risk_of_harm;
        }
    }
    throw std::invalid_argument("selected invasive candidate is not present");
}

double MaximumRiskOfHarm(
    const eco_restoration::ThreatAssessment& threat,
    const eco_restoration::AuthorizationEvidence& authorization,
    double selected_invasive_risk_of_harm) {
    if (!std::isfinite(threat.estimated_risk_of_harm) ||
        threat.estimated_risk_of_harm < 0.0 ||
        threat.estimated_risk_of_harm > 1.0) {
        throw std::invalid_argument("threat assessment risk is invalid");
    }

    const double authorization_risk =
        RiskOfHarmFromFixed(authorization.risk_of_harm_fixed);

    if (!std::isfinite(selected_invasive_risk_of_harm) ||
        selected_invasive_risk_of_harm < 0.0 ||
        selected_invasive_risk_of_harm > 1.0) {
        throw std::invalid_argument("selected invasive risk is invalid");
    }

    return std::max({
        threat.estimated_risk_of_harm,
        authorization_risk,
        selected_invasive_risk_of_harm
    });
}

bool MaximumRiskOfHarmSelfTest() {
    const eco_restoration::ThreatAssessment threat{
        0.14,
        0.18,
        false,
        false,
        0.91,
        0.82
    };

    const eco_restoration::AuthorizationEvidence authorization{
        "assessment_only",
        "policy_eco_safe_v1",
        1000,
        2000,
        1,
        250'000,
        true
    };

    const double invasive_risk = 0.22;
    const double maximum = MaximumRiskOfHarm(
        threat, authorization, invasive_risk);

    if (std::abs(maximum - 0.25) > 1e-12) return false;
    if (!IsSafeRiskOfHarm(0.30)) return false;
    if (IsSafeRiskOfHarm(0.300001)) return false;
    if (!IsSafeRiskOfHarmFixed(300'000)) return false;
    if (IsSafeRiskOfHarmFixed(300'001)) return false;
    if (RiskOfHarmToFixed(0.25) != 250'000) return false;
    if (std::abs(RiskOfHarmFromFixed(125'000) - 0.125) > 1e-12) {
        return false;
    }

    try {
        static_cast<void>(RiskOfHarmFromFixed(-1));
        return false;
    } catch (const std::invalid_argument&) {
    }

    try {
        static_cast<void>(RiskOfHarmToFixed(1.01));
        return false;
    } catch (const std::invalid_argument&) {
    }

    try {
        static_cast<void>(MaximumRiskOfHarm(threat, authorization, -0.01));
        return false;
    } catch (const std::invalid_argument&) {
    }

    return true;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

struct FoundationSafetyVerdict {
    bool foundation_safe{};
    std::vector<std::string> failure_reasons;
};

void AddFailureReason(FoundationSafetyVerdict& verdict,
                      bool condition,
                      std::string_view reason) {
    if (!condition) {
        verdict.failure_reasons.emplace_back(reason);
    }
}

FoundationSafetyVerdict EvaluateFoundationSafety(
    const FoundationReport& report) {
    FoundationSafetyVerdict verdict;

    AddFailureReason(
        verdict,
        report.private_heat_accepted,
        "private heat membership or heat-threshold proof was not accepted");

    AddFailureReason(
        verdict,
        !report.threat_fail_closed,
        "threat containment required a fail-closed state");

    AddFailureReason(
        verdict,
        report.water_biodiversity_allowed,
        "water allocation or biodiversity corridor did not allow the decision");

    AddFailureReason(
        verdict,
        report.water_biodiversity_invariant_holds,
        "water-compliant biodiversity-violation authorization invariant failed");

    AddFailureReason(
        verdict,
        report.authorization_accepted,
        "proof-checked authorization was not accepted");

    AddFailureReason(
        verdict,
        report.invasive_control_safe,
        "selected invasive-control candidate was not safe");

    AddFailureReason(
        verdict,
        report.irrigation_robustly_feasible,
        "robust irrigation schedule was not feasible");

    AddFailureReason(
        verdict,
        std::isfinite(report.maximum_risk_of_harm),
        "maximum risk of harm was not finite");

    if (std::isfinite(report.maximum_risk_of_harm)) {
        AddFailureReason(
            verdict,
            report.maximum_risk_of_harm >= 0.0,
            "maximum risk of harm was below zero");

        AddFailureReason(
            verdict,
            report.maximum_risk_of_harm <= 1.0,
            "maximum risk of harm exceeded the unit interval");

        AddFailureReason(
            verdict,
            report.maximum_risk_of_harm <= 0.30,
            "maximum risk of harm exceeded the 0.30 safety corridor");
    }

    verdict.foundation_safe = verdict.failure_reasons.empty();
    return verdict;
}

bool FoundationSafetyVerdictMatchesReport(
    const FoundationSafetyVerdict& verdict,
    const FoundationReport& report) {
    return verdict.foundation_safe == report.foundation_safe;
}

bool FoundationSafetyVerdictHasReason(
    const FoundationSafetyVerdict& verdict,
    std::string_view expected_reason) {
    return std::find(
        verdict.failure_reasons.begin(),
        verdict.failure_reasons.end(),
        expected_reason) != verdict.failure_reasons.end();
}

std::string SerializeFoundationSafetyReasons(
    const FoundationSafetyVerdict& verdict) {
    std::ostringstream output;
    for (std::size_t index = 0; index < verdict.failure_reasons.size(); ++index) {
        if (index != 0U) output << ';';
        output << verdict.failure_reasons[index];
    }
    return output.str();
}

bool FoundationSafetyVerdictSelfTest() {
    const FoundationReport safe_report{
        true,
        false,
        true,
        true,
        true,
        true,
        true,
        0.30,
        0.90,
        0.80,
        true
    };

    const FoundationSafetyVerdict safe_verdict =
        EvaluateFoundationSafety(safe_report);

    if (!safe_verdict.foundation_safe ||
        !safe_verdict.failure_reasons.empty() ||
        !FoundationSafetyVerdictMatchesReport(safe_verdict, safe_report)) {
        return false;
    }

    FoundationReport unsafe_report = safe_report;
    unsafe_report.private_heat_accepted = false;
    unsafe_report.threat_fail_closed = true;
    unsafe_report.water_biodiversity_allowed = false;
    unsafe_report.water_biodiversity_invariant_holds = false;
    unsafe_report.authorization_accepted = false;
    unsafe_report.invasive_control_safe = false;
    unsafe_report.irrigation_robustly_feasible = false;
    unsafe_report.maximum_risk_of_harm = 0.31;
    unsafe_report.foundation_safe = false;

    const FoundationSafetyVerdict unsafe_verdict =
        EvaluateFoundationSafety(unsafe_report);

    if (unsafe_verdict.foundation_safe ||
        unsafe_verdict.failure_reasons.size() != 8U ||
        !FoundationSafetyVerdictMatchesReport(unsafe_verdict, unsafe_report)) {
        return false;
    }

    if (!FoundationSafetyVerdictHasReason(
            unsafe_verdict,
            "private heat membership or heat-threshold proof was not accepted") ||
        !FoundationSafetyVerdictHasReason(
            unsafe_verdict,
            "maximum risk of harm exceeded the 0.30 safety corridor")) {
        return false;
    }

    const std::string reasons = SerializeFoundationSafetyReasons(unsafe_verdict);
    if (reasons.empty() ||
        reasons.find("proof-checked authorization") == std::string::npos) {
        return false;
    }

    FoundationReport invalid_risk_report = safe_report;
    invalid_risk_report.maximum_risk_of_harm =
        std::numeric_limits<double>::infinity();

    const FoundationSafetyVerdict invalid_risk_verdict =
        EvaluateFoundationSafety(invalid_risk_report);

    return !invalid_risk_verdict.foundation_safe &&
           FoundationSafetyVerdictHasReason(
               invalid_risk_verdict,
               "maximum risk of harm was not finite");
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
#include <optional>

namespace prometheus_praxis_foundation_extensions {

struct PrivateHeatProofPlanOverrides {
    std::optional<std::size_t> corridor_cell_count;
    std::optional<std::uint8_t> h3_resolution;
    std::optional<std::int64_t> fixed_point_scale;
    std::optional<std::int64_t> heat_critical_fixed;
    std::optional<std::int64_t> uncertainty_margin_fixed;
    std::optional<std::size_t> proof_target_bytes;
};

constexpr std::size_t default_corridor_cell_count = 100U;
constexpr std::uint8_t default_h3_resolution = 7U;
constexpr std::int64_t default_fixed_point_scale = 1'000'000;
constexpr std::int64_t default_heat_critical_fixed = 50'000'000;
constexpr std::int64_t default_uncertainty_margin_fixed = 5'000'000;
constexpr std::size_t default_proof_target_bytes = 16U * 1024U;

void ValidatePrivateHeatProofPlan(
    const eco_restoration::PrivateHeatProofPlan& plan) {
    if (plan.corridor_cell_count == 0U) {
        throw std::invalid_argument("corridor cell count must be positive");
    }

    if (plan.h3_resolution > 15U) {
        throw std::invalid_argument("H3 resolution must lie in [0,15]");
    }

    if (plan.fixed_point_scale <= 0) {
        throw std::invalid_argument("fixed-point scale must be positive");
    }

    if (plan.heat_critical_fixed <= 0) {
        throw std::invalid_argument("heat-critical threshold must be positive");
    }

    if (plan.uncertainty_margin_fixed < 0) {
        throw std::invalid_argument("heat uncertainty margin must be nonnegative");
    }

    if (plan.uncertainty_margin_fixed > plan.heat_critical_fixed) {
        throw std::invalid_argument(
            "heat uncertainty margin must not exceed the critical threshold");
    }

    if (plan.proof_target_bytes == 0U) {
        throw std::invalid_argument("proof target size must be positive");
    }
}

eco_restoration::PrivateHeatProofPlan MakePrivateHeatProofPlan(
    const PrivateHeatProofPlanOverrides& overrides = {}) {
    eco_restoration::PrivateHeatProofPlan plan{
        default_corridor_cell_count,
        default_h3_resolution,
        default_fixed_point_scale,
        default_heat_critical_fixed,
        default_uncertainty_margin_fixed,
        default_proof_target_bytes
    };

    if (overrides.corridor_cell_count.has_value()) {
        plan.corridor_cell_count = *overrides.corridor_cell_count;
    }

    if (overrides.h3_resolution.has_value()) {
        plan.h3_resolution = *overrides.h3_resolution;
    }

    if (overrides.fixed_point_scale.has_value()) {
        plan.fixed_point_scale = *overrides.fixed_point_scale;
    }

    if (overrides.heat_critical_fixed.has_value()) {
        plan.heat_critical_fixed = *overrides.heat_critical_fixed;
    }

    if (overrides.uncertainty_margin_fixed.has_value()) {
        plan.uncertainty_margin_fixed =
            *overrides.uncertainty_margin_fixed;
    }

    if (overrides.proof_target_bytes.has_value()) {
        plan.proof_target_bytes = *overrides.proof_target_bytes;
    }

    ValidatePrivateHeatProofPlan(plan);
    return plan;
}

bool PrivateHeatProofPlanEqual(
    const eco_restoration::PrivateHeatProofPlan& left,
    const eco_restoration::PrivateHeatProofPlan& right) {
    return left.corridor_cell_count == right.corridor_cell_count &&
           left.h3_resolution == right.h3_resolution &&
           left.fixed_point_scale == right.fixed_point_scale &&
           left.heat_critical_fixed == right.heat_critical_fixed &&
           left.uncertainty_margin_fixed == right.uncertainty_margin_fixed &&
           left.proof_target_bytes == right.proof_target_bytes;
}

bool PrivateHeatProofPlanSelfTest() {
    const auto defaults = MakePrivateHeatProofPlan();

    const eco_restoration::PrivateHeatProofPlan expected_defaults{
        default_corridor_cell_count,
        default_h3_resolution,
        default_fixed_point_scale,
        default_heat_critical_fixed,
        default_uncertainty_margin_fixed,
        default_proof_target_bytes
    };

    if (!PrivateHeatProofPlanEqual(defaults, expected_defaults)) {
        return false;
    }

    const PrivateHeatProofPlanOverrides overrides{
        384U,
        static_cast<std::uint8_t>(11U),
        100U,
        5400,
        180,
        32U * 1024U
    };

    const auto customized = MakePrivateHeatProofPlan(overrides);

    if (customized.corridor_cell_count != 384U ||
        customized.h3_resolution != 11U ||
        customized.fixed_point_scale != 100 ||
        customized.heat_critical_fixed != 5400 ||
        customized.uncertainty_margin_fixed != 180 ||
        customized.proof_target_bytes != 32U * 1024U) {
        return false;
    }

    try {
        static_cast<void>(MakePrivateHeatProofPlan(
            {.corridor_cell_count = 0U}));
        return false;
    } catch (const std::invalid_argument&) {
    }

    try {
        static_cast<void>(MakePrivateHeatProofPlan(
            {.h3_resolution = static_cast<std::uint8_t>(16U)}));
        return false;
    } catch (const std::invalid_argument&) {
    }

    try {
        static_cast<void>(MakePrivateHeatProofPlan(
            {.fixed_point_scale = 0}));
        return false;
    } catch (const std::invalid_argument&) {
    }

    try {
        static_cast<void>(MakePrivateHeatProofPlan(
            {.uncertainty_margin_fixed = default_heat_critical_fixed + 1}));
        return false;
    } catch (const std::invalid_argument&) {
    }

    return true;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

struct PrivateHeatStatementPublicView {
    std::string corridor_table_identifier;
    std::string proof_system_identifier;
    bool accepted{};
    std::size_t membership_lookup_rows{};
    std::size_t heat_range_lookup_rows{};
    double knowledge_factor{};
    double eco_impact_value{};
};

void ValidateUnitInterval(double value, std::string_view field_name) {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
        throw std::invalid_argument(
            std::string(field_name) + " must be finite and lie in [0,1]");
    }
}

PrivateHeatStatementPublicView MakePrivateHeatStatementPublicView(
    const eco_restoration::PrivateHeatStatement& statement) {
    if (statement.corridor_table_identifier.empty()) {
        throw std::invalid_argument("corridor table identifier is required");
    }

    if (statement.proof_system_identifier.empty()) {
        throw std::invalid_argument("proof system identifier is required");
    }

    if (statement.membership_lookup_rows == 0U) {
        throw std::invalid_argument("membership lookup rows must be positive");
    }

    if (statement.heat_range_lookup_rows == 0U) {
        throw std::invalid_argument("heat range lookup rows must be positive");
    }

    ValidateUnitInterval(statement.knowledge_factor, "knowledge factor");
    ValidateUnitInterval(statement.eco_impact_value, "eco-impact value");

    return {
        statement.corridor_table_identifier,
        statement.proof_system_identifier,
        statement.accepted,
        statement.membership_lookup_rows,
        statement.heat_range_lookup_rows,
        statement.knowledge_factor,
        statement.eco_impact_value
    };
}

bool IsPrivateHeatStatementPublicViewValid(
    const PrivateHeatStatementPublicView& view) {
    if (view.corridor_table_identifier.empty() ||
        view.proof_system_identifier.empty() ||
        view.membership_lookup_rows == 0U ||
        view.heat_range_lookup_rows == 0U) {
        return false;
    }

    return std::isfinite(view.knowledge_factor) &&
           std::isfinite(view.eco_impact_value) &&
           view.knowledge_factor >= 0.0 &&
           view.knowledge_factor <= 1.0 &&
           view.eco_impact_value >= 0.0 &&
           view.eco_impact_value <= 1.0;
}

std::string SerializePrivateHeatStatementPublicView(
    const PrivateHeatStatementPublicView& view) {
    if (!IsPrivateHeatStatementPublicViewValid(view)) {
        throw std::invalid_argument("private heat public view is invalid");
    }

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << '{';

    output << "\"corridor_table_identifier\":";
    json_string(output, view.corridor_table_identifier);

    output << ",\"proof_system_identifier\":";
    json_string(output, view.proof_system_identifier);

    output << ",\"accepted\":"
           << (view.accepted ? "true" : "false");

    output << ",\"membership_lookup_rows\":"
           << view.membership_lookup_rows;

    output << ",\"heat_range_lookup_rows\":"
           << view.heat_range_lookup_rows;

    output << ",\"knowledge_factor\":";
    json_double(output, view.knowledge_factor);

    output << ",\"eco_impact_value\":";
    json_double(output, view.eco_impact_value);

    output << '}';
    return output.str();
}

bool PrivateHeatStatementPublicViewSelfTest() {
    const eco_restoration::PrivateHeatStatement statement{
        "corridor_h3_table",
        "external_private_proof",
        true,
        384U,
        65'536U,
        0.91,
        0.82
    };

    const PrivateHeatStatementPublicView view =
        MakePrivateHeatStatementPublicView(statement);

    if (!IsPrivateHeatStatementPublicViewValid(view) ||
        view.corridor_table_identifier != "corridor_h3_table" ||
        view.proof_system_identifier != "external_private_proof" ||
        !view.accepted ||
        view.membership_lookup_rows != 384U ||
        view.heat_range_lookup_rows != 65'536U ||
        std::abs(view.knowledge_factor - 0.91) > 1e-12 ||
        std::abs(view.eco_impact_value - 0.82) > 1e-12) {
        return false;
    }

    const std::string expected =
        "{\"corridor_table_identifier\":\"corridor_h3_table\","
        "\"proof_system_identifier\":\"external_private_proof\","
        "\"accepted\":true,"
        "\"membership_lookup_rows\":384,"
        "\"heat_range_lookup_rows\":65536,"
        "\"knowledge_factor\":0.910000,"
        "\"eco_impact_value\":0.820000}";

    if (SerializePrivateHeatStatementPublicView(view) != expected) {
        return false;
    }

    try {
        eco_restoration::PrivateHeatStatement invalid = statement;
        invalid.corridor_table_identifier.clear();
        static_cast<void>(MakePrivateHeatStatementPublicView(invalid));
        return false;
    } catch (const std::invalid_argument&) {
    }

    try {
        eco_restoration::PrivateHeatStatement invalid = statement;
        invalid.knowledge_factor = 1.01;
        static_cast<void>(MakePrivateHeatStatementPublicView(invalid));
        return false;
    } catch (const std::invalid_argument&) {
    }

    return true;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

constexpr double default_sensor_spoofing_anomaly = 0.08;
constexpr double default_model_poisoning_anomaly = 0.05;
constexpr double default_policy_substitution_anomaly = 0.03;
constexpr double default_delayed_actuation_anomaly = 0.04;

bool IsUnitIntervalScore(double value) {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

bool IsThreatObservationValid(
    const eco_restoration::ThreatObservation& observation) {
    return IsUnitIntervalScore(observation.anomaly_score) &&
           IsUnitIntervalScore(observation.provenance_coverage) &&
           IsUnitIntervalScore(observation.policy_match_score) &&
           IsUnitIntervalScore(observation.actuation_delay_ratio);
}

std::size_t ThreatSurfaceIndex(eco_restoration::ThreatSurface surface) {
    switch (surface) {
        case eco_restoration::ThreatSurface::SensorSpoofing:
            return 0U;
        case eco_restoration::ThreatSurface::ModelPoisoning:
            return 1U;
        case eco_restoration::ThreatSurface::PolicySubstitution:
            return 2U;
        case eco_restoration::ThreatSurface::DelayedActuation:
            return 3U;
    }
    throw std::invalid_argument("unrecognized threat surface");
}

bool IsThreatObservationSetValid(
    const std::vector<eco_restoration::ThreatObservation>& observations) {
    if (observations.size() != 4U) return false;

    std::vector<bool> surfaces_present(4U, false);
    for (const auto& observation : observations) {
        if (!IsThreatObservationValid(observation)) return false;

        const std::size_t index = ThreatSurfaceIndex(observation.surface);
        if (surfaces_present[index]) return false;
        surfaces_present[index] = true;
    }

    return std::all_of(
        surfaces_present.begin(),
        surfaces_present.end(),
        [](bool present) { return present; });
}

std::vector<eco_restoration::ThreatObservation> MakeThreatObservationSet() {
    const std::vector<eco_restoration::ThreatObservation> observations{
        {
            eco_restoration::ThreatSurface::SensorSpoofing,
            default_sensor_spoofing_anomaly,
            0.96,
            0.99,
            0.02
        },
        {
            eco_restoration::ThreatSurface::ModelPoisoning,
            default_model_poisoning_anomaly,
            0.97,
            0.99,
            0.01
        },
        {
            eco_restoration::ThreatSurface::PolicySubstitution,
            default_policy_substitution_anomaly,
            0.98,
            0.99,
            0.01
        },
        {
            eco_restoration::ThreatSurface::DelayedActuation,
            default_delayed_actuation_anomaly,
            0.95,
            0.99,
            0.03
        }
    };

    if (!IsThreatObservationSetValid(observations)) {
        throw std::runtime_error("default threat observation fixture is invalid");
    }
    return observations;
}

bool ThreatObservationSetIsSafe(
    const std::vector<eco_restoration::ThreatObservation>& observations) {
    if (!IsThreatObservationSetValid(observations)) return false;

    for (const auto& observation : observations) {
        if (observation.anomaly_score > 0.10 ||
            observation.provenance_coverage < 0.90 ||
            observation.policy_match_score < 0.98 ||
            observation.actuation_delay_ratio > 0.10) {
            return false;
        }
    }
    return true;
}

bool ThreatObservationFixtureSelfTest() {
    const auto observations = MakeThreatObservationSet();

    if (!IsThreatObservationSetValid(observations) ||
        !ThreatObservationSetIsSafe(observations) ||
        observations.size() != 4U) {
        return false;
    }

    if (observations[0].surface !=
            eco_restoration::ThreatSurface::SensorSpoofing ||
        observations[1].surface !=
            eco_restoration::ThreatSurface::ModelPoisoning ||
        observations[2].surface !=
            eco_restoration::ThreatSurface::PolicySubstitution ||
        observations[3].surface !=
            eco_restoration::ThreatSurface::DelayedActuation) {
        return false;
    }

    auto duplicate_surface = observations;
    duplicate_surface[3].surface =
        eco_restoration::ThreatSurface::SensorSpoofing;

    if (IsThreatObservationSetValid(duplicate_surface)) {
        return false;
    }

    auto invalid_score = observations;
    invalid_score[0].anomaly_score = 1.01;

    if (IsThreatObservationSetValid(invalid_score)) {
        return false;
    }

    auto unsafe_fixture = observations;
    unsafe_fixture[1].provenance_coverage = 0.89;

    if (ThreatObservationSetIsSafe(unsafe_fixture)) {
        return false;
    }

    return true;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

bool IsThreatAssessmentValid(
    const eco_restoration::ThreatAssessment& assessment) {
    return IsUnitIntervalScore(assessment.detectability) &&
           IsUnitIntervalScore(assessment.estimated_risk_of_harm) &&
           IsUnitIntervalScore(assessment.knowledge_factor) &&
           IsUnitIntervalScore(assessment.eco_impact_value);
}

std::string ThreatAssessmentClassification(
    const eco_restoration::ThreatAssessment& assessment) {
    if (!IsThreatAssessmentValid(assessment)) {
        return "invalid_assessment";
    }

    if (assessment.fail_closed) {
        return "fail_closed";
    }

    if (assessment.estimated_risk_of_harm > 0.30) {
        return "risk_blocked";
    }

    if (assessment.detectability >= 0.50) {
        return "elevated_detection_signal";
    }

    return "accepted_for_diagnostic_use";
}

std::string ThreatAssessmentRiskBand(
    double risk_of_harm) {
    if (!IsUnitIntervalScore(risk_of_harm)) {
        throw std::invalid_argument("risk of harm must lie in [0,1]");
    }

    if (risk_of_harm <= 0.10) return "low";
    if (risk_of_harm <= 0.20) return "guarded";
    if (risk_of_harm <= 0.30) return "corridor_limit";
    return "unsafe";
}

std::string ThreatAssessmentDetectabilityBand(
    double detectability) {
    if (!IsUnitIntervalScore(detectability)) {
        throw std::invalid_argument("detectability must lie in [0,1]");
    }

    if (detectability < 0.10) return "minimal";
    if (detectability < 0.25) return "observable";
    if (detectability < 0.50) return "elevated";
    return "high";
}

std::string ExplainThreatAssessment(
    const eco_restoration::ThreatAssessment& assessment) {
    if (!IsThreatAssessmentValid(assessment)) {
        throw std::invalid_argument("threat assessment contains invalid scores");
    }

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(6);

    output << "threat_assessment\n";
    output << "classification="
           << ThreatAssessmentClassification(assessment) << '\n';
    output << "detectability="
           << assessment.detectability << '\n';
    output << "detectability_band="
           << ThreatAssessmentDetectabilityBand(assessment.detectability) << '\n';
    output << "estimated_risk_of_harm="
           << assessment.estimated_risk_of_harm << '\n';
    output << "risk_band="
           << ThreatAssessmentRiskBand(assessment.estimated_risk_of_harm) << '\n';
    output << "risk_within_0_30_corridor="
           << (assessment.estimated_risk_of_harm <= 0.30 ? "true" : "false")
           << '\n';
    output << "unsafe_condition="
           << (assessment.unsafe_condition ? "true" : "false") << '\n';
    output << "fail_closed="
           << (assessment.fail_closed ? "true" : "false") << '\n';
    output << "knowledge_factor="
           << assessment.knowledge_factor << '\n';
    output << "eco_impact_value="
           << assessment.eco_impact_value << '\n';

    if (assessment.fail_closed) {
        output << "recommendation=hold_diagnostic_acceptance\n";
    } else if (assessment.estimated_risk_of_harm > 0.30) {
        output << "recommendation=block_due_to_risk\n";
    } else {
        output << "recommendation=retain_for_bounded_review\n";
    }

    return output.str();
}

bool ThreatAssessmentExplanationHasStableLines(
    std::string_view explanation) {
    const std::vector<std::string_view> required_prefixes{
        "threat_assessment",
        "classification=",
        "detectability=",
        "detectability_band=",
        "estimated_risk_of_harm=",
        "risk_band=",
        "risk_within_0_30_corridor=",
        "unsafe_condition=",
        "fail_closed=",
        "knowledge_factor=",
        "eco_impact_value=",
        "recommendation="
    };

    std::size_t previous_position = 0U;
    for (const auto prefix : required_prefixes) {
        const std::size_t position = explanation.find(prefix);
        if (position == std::string_view::npos ||
            position < previous_position) {
            return false;
        }
        previous_position = position;
    }
    return true;
}

bool ExplainThreatAssessmentSelfTest() {
    const eco_restoration::ThreatAssessment safe_assessment{
        0.140000,
        0.180000,
        false,
        false,
        0.910000,
        0.820000
    };

    const std::string safe_explanation =
        ExplainThreatAssessment(safe_assessment);

    if (!ThreatAssessmentExplanationHasStableLines(safe_explanation) ||
        safe_explanation.find("classification=accepted_for_diagnostic_use") ==
            std::string::npos ||
        safe_explanation.find("risk_band=guarded") ==
            std::string::npos ||
        safe_explanation.find("fail_closed=false") ==
            std::string::npos ||
        safe_explanation.find("recommendation=retain_for_bounded_review") ==
            std::string::npos) {
        return false;
    }

    const eco_restoration::ThreatAssessment blocked_assessment{
        0.650000,
        0.410000,
        true,
        true,
        0.300000,
        0.000000
    };

    const std::string blocked_explanation =
        ExplainThreatAssessment(blocked_assessment);

    if (blocked_explanation.find("classification=fail_closed") ==
            std::string::npos ||
        blocked_explanation.find("risk_band=unsafe") ==
            std::string::npos ||
        blocked_explanation.find("fail_closed=true") ==
            std::string::npos ||
        blocked_explanation.find("recommendation=hold_diagnostic_acceptance") ==
            std::string::npos) {
        return false;
    }

    try {
        eco_restoration::ThreatAssessment invalid = safe_assessment;
        invalid.detectability = -0.01;
        static_cast<void>(ExplainThreatAssessment(invalid));
        return false;
    } catch (const std::invalid_argument&) {
    }

    try {
        static_cast<void>(ThreatAssessmentRiskBand(1.01));
        return false;
    } catch (const std::invalid_argument&) {
    }

    return true;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

constexpr std::int64_t default_water_permitted_ml = 1'000'000;
constexpr std::int64_t default_water_allocated_ml = 500'000;
constexpr std::int64_t default_ecological_reserve_ml = 400'000;

bool IsWaterAllocationStructurallyValid(
    const eco_restoration::WaterAllocation& allocation) {
    return allocation.allocated_ml >= 0 &&
           allocation.permitted_ml >= 0 &&
           allocation.ecological_reserve_ml >= 0;
}

std::int64_t WaterAllocationRemainingMl(
    const eco_restoration::WaterAllocation& allocation) {
    if (!IsWaterAllocationStructurallyValid(allocation)) {
        throw std::invalid_argument("water allocation values must be nonnegative");
    }

    if (allocation.allocated_ml > allocation.permitted_ml) {
        return 0;
    }

    return allocation.permitted_ml - allocation.allocated_ml;
}

bool WaterAllocationPreservesReserve(
    const eco_restoration::WaterAllocation& allocation) {
    if (!IsWaterAllocationStructurallyValid(allocation)) return false;
    if (allocation.allocated_ml > allocation.permitted_ml) return false;

    return WaterAllocationRemainingMl(allocation) >=
           allocation.ecological_reserve_ml;
}

eco_restoration::WaterAllocation MakeWaterAllocation() {
    const eco_restoration::WaterAllocation allocation{
        default_water_allocated_ml,
        default_water_permitted_ml,
        default_ecological_reserve_ml
    };

    if (!WaterAllocationPreservesReserve(allocation)) {
        throw std::runtime_error("default water allocation violates ecological reserve");
    }

    return allocation;
}

eco_restoration::WaterAllocation MakeWaterAllocation(
    std::int64_t allocated_ml,
    std::int64_t permitted_ml,
    std::int64_t ecological_reserve_ml) {
    const eco_restoration::WaterAllocation allocation{
        allocated_ml,
        permitted_ml,
        ecological_reserve_ml
    };

    if (!IsWaterAllocationStructurallyValid(allocation)) {
        throw std::invalid_argument("water allocation values must be nonnegative");
    }

    return allocation;
}

eco_restoration::WaterAllocation MakeWaterAllocationViolatingReserve() {
    const eco_restoration::WaterAllocation allocation{
        750'000,
        default_water_permitted_ml,
        300'000
    };

    if (WaterAllocationPreservesReserve(allocation)) {
        throw std::runtime_error(
            "negative-testing water allocation must violate ecological reserve");
    }

    return allocation;
}

std::string ExplainWaterAllocation(
    const eco_restoration::WaterAllocation& allocation) {
    if (!IsWaterAllocationStructurallyValid(allocation)) {
        throw std::invalid_argument("water allocation values must be nonnegative");
    }

    std::ostringstream output;
    output.imbue(std::locale::classic());

    output << "water_allocation\n";
    output << "allocated_ml=" << allocation.allocated_ml << '\n';
    output << "permitted_ml=" << allocation.permitted_ml << '\n';
    output << "ecological_reserve_ml="
           << allocation.ecological_reserve_ml << '\n';
    output << "remaining_ml="
           << WaterAllocationRemainingMl(allocation) << '\n';
    output << "preserves_reserve="
           << (WaterAllocationPreservesReserve(allocation) ? "true" : "false")
           << '\n';

    return output.str();
}

bool WaterAllocationFixtureSelfTest() {
    const eco_restoration::WaterAllocation valid =
        MakeWaterAllocation();

    if (!IsWaterAllocationStructurallyValid(valid) ||
        !WaterAllocationPreservesReserve(valid) ||
        valid.allocated_ml != default_water_allocated_ml ||
        valid.permitted_ml != default_water_permitted_ml ||
        valid.ecological_reserve_ml != default_ecological_reserve_ml ||
        WaterAllocationRemainingMl(valid) != 500'000) {
        return false;
    }

    const eco_restoration::WaterAllocation violating =
        MakeWaterAllocationViolatingReserve();

    if (!IsWaterAllocationStructurallyValid(violating) ||
        WaterAllocationPreservesReserve(violating) ||
        WaterAllocationRemainingMl(violating) != 250'000) {
        return false;
    }

    const std::string valid_explanation =
        ExplainWaterAllocation(valid);

    if (valid_explanation.find("preserves_reserve=true") ==
            std::string::npos ||
        valid_explanation.find("remaining_ml=500000") ==
            std::string::npos) {
        return false;
    }

    const std::string violating_explanation =
        ExplainWaterAllocation(violating);

    if (violating_explanation.find("preserves_reserve=false") ==
            std::string::npos) {
        return false;
    }

    try {
        static_cast<void>(MakeWaterAllocation(-1, 100, 10));
        return false;
    } catch (const std::invalid_argument&) {
    }

    try {
        static_cast<void>(WaterAllocationRemainingMl({-1, 100, 10}));
        return false;
    } catch (const std::invalid_argument&) {
    }

    return true;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

constexpr std::int32_t default_biodiversity_quality_fixed = 700'000;
constexpr std::int32_t default_biodiversity_required_minimum_fixed = 600'000;

bool IsBiodiversityIndexValid(
    const eco_restoration::BiodiversityIndex& index) {
    return index.quality_fixed >= 0 &&
           index.quality_fixed <= eco_restoration::unit_interval_scale &&
           index.required_minimum_fixed >= 0 &&
           index.required_minimum_fixed <= eco_restoration::unit_interval_scale;
}

bool BiodiversityRequirementSatisfied(
    const eco_restoration::BiodiversityIndex& index) {
    if (!IsBiodiversityIndexValid(index)) return false;
    return index.quality_fixed >= index.required_minimum_fixed;
}

double BiodiversityQualityNormalized(
    const eco_restoration::BiodiversityIndex& index) {
    if (!IsBiodiversityIndexValid(index)) {
        throw std::invalid_argument(
            "biodiversity index must lie in the fixed-point unit interval");
    }

    return static_cast<double>(index.quality_fixed) /
           static_cast<double>(eco_restoration::unit_interval_scale);
}

double BiodiversityMinimumNormalized(
    const eco_restoration::BiodiversityIndex& index) {
    if (!IsBiodiversityIndexValid(index)) {
        throw std::invalid_argument(
            "biodiversity minimum must lie in the fixed-point unit interval");
    }

    return static_cast<double>(index.required_minimum_fixed) /
           static_cast<double>(eco_restoration::unit_interval_scale);
}

eco_restoration::BiodiversityIndex MakeBiodiversityIndex() {
    const eco_restoration::BiodiversityIndex index{
        default_biodiversity_quality_fixed,
        default_biodiversity_required_minimum_fixed
    };

    if (!IsBiodiversityIndexValid(index) ||
        !BiodiversityRequirementSatisfied(index)) {
        throw std::runtime_error(
            "default biodiversity index must satisfy the required minimum");
    }

    return index;
}

eco_restoration::BiodiversityIndex MakeBiodiversityIndex(
    std::int32_t quality_fixed,
    std::int32_t required_minimum_fixed) {
    const eco_restoration::BiodiversityIndex index{
        quality_fixed,
        required_minimum_fixed
    };

    if (!IsBiodiversityIndexValid(index)) {
        throw std::invalid_argument(
            "biodiversity quality and minimum must lie in [0,unit_interval_scale]");
    }

    return index;
}

std::string ExplainBiodiversityIndex(
    const eco_restoration::BiodiversityIndex& index) {
    if (!IsBiodiversityIndexValid(index)) {
        throw std::invalid_argument("biodiversity index is invalid");
    }

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(6);
    output << "biodiversity_index\n";
    output << "quality_fixed=" << index.quality_fixed << '\n';
    output << "required_minimum_fixed="
           << index.required_minimum_fixed << '\n';
    output << "quality_normalized="
           << BiodiversityQualityNormalized(index) << '\n';
    output << "required_minimum_normalized="
           << BiodiversityMinimumNormalized(index) << '\n';
    output << "requirement_satisfied="
           << (BiodiversityRequirementSatisfied(index) ? "true" : "false")
           << '\n';
    return output.str();
}

bool BiodiversityIndexFixtureSelfTest() {
    const auto defaults = MakeBiodiversityIndex();

    if (!IsBiodiversityIndexValid(defaults) ||
        !BiodiversityRequirementSatisfied(defaults) ||
        defaults.quality_fixed != default_biodiversity_quality_fixed ||
        defaults.required_minimum_fixed !=
            default_biodiversity_required_minimum_fixed) {
        return false;
    }

    if (std::abs(BiodiversityQualityNormalized(defaults) - 0.70) > 1e-12 ||
        std::abs(BiodiversityMinimumNormalized(defaults) - 0.60) > 1e-12) {
        return false;
    }

    const auto below_requirement =
        MakeBiodiversityIndex(500'000, 600'000);

    if (!IsBiodiversityIndexValid(below_requirement) ||
        BiodiversityRequirementSatisfied(below_requirement)) {
        return false;
    }

    const std::string explanation =
        ExplainBiodiversityIndex(defaults);

    if (explanation.find("quality_fixed=700000") ==
            std::string::npos ||
        explanation.find("requirement_satisfied=true") ==
            std::string::npos) {
        return false;
    }

    try {
        static_cast<void>(MakeBiodiversityIndex(-1, 0));
        return false;
    } catch (const std::invalid_argument&) {
    }

    try {
        static_cast<void>(MakeBiodiversityIndex(
            eco_restoration::unit_interval_scale + 1,
            eco_restoration::unit_interval_scale));
        return false;
    } catch (const std::invalid_argument&) {
    }

    try {
        static_cast<void>(MakeBiodiversityIndex(
            eco_restoration::unit_interval_scale,
            eco_restoration::unit_interval_scale + 1));
        return false;
    } catch (const std::invalid_argument&) {
    }

    return true;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

struct WaterBiodiversityInvariantResult {
    eco_restoration::CrossShardDecision decision;
    bool invariant_holds{};
    bool accepted{};
    std::vector<std::string> failure_reasons;
};

void AddWaterBiodiversityFailure(
    WaterBiodiversityInvariantResult& result,
    bool condition,
    std::string_view reason) {
    if (!condition) {
        result.failure_reasons.emplace_back(reason);
    }
}

WaterBiodiversityInvariantResult CheckWaterBiodiversityInvariant(
    const eco_restoration::WaterAllocation& water,
    const eco_restoration::BiodiversityIndex& biodiversity) {
    if (!IsWaterAllocationStructurallyValid(water)) {
        throw std::invalid_argument("water allocation is structurally invalid");
    }

    if (!IsBiodiversityIndexValid(biodiversity)) {
        throw std::invalid_argument("biodiversity index is structurally invalid");
    }

    WaterBiodiversityInvariantResult result;
    result.decision = eco_restoration::evaluate_water_biodiversity(
        water,
        biodiversity);
    result.invariant_holds =
        eco_restoration::required_cross_shard_unsat(result.decision);

    AddWaterBiodiversityFailure(
        result,
        result.decision.water_compliant,
        "water allocation does not preserve permitted and ecological reserve limits");

    AddWaterBiodiversityFailure(
        result,
        result.decision.biodiversity_compliant,
        "biodiversity quality is below the required minimum");

    AddWaterBiodiversityFailure(
        result,
        !result.decision.biodiversity_violation ||
            !result.decision.allow,
        "biodiversity violation was paired with an allow decision");

    AddWaterBiodiversityFailure(
        result,
        result.invariant_holds,
        "cross-shard unsatisfiability invariant did not hold");

    AddWaterBiodiversityFailure(
        result,
        result.decision.allow,
        "cross-shard decision did not allow the assessment");

    result.accepted = result.failure_reasons.empty();
    return result;
}

bool IsWaterBiodiversityInvariantResultValid(
    const WaterBiodiversityInvariantResult& result) {
    if (result.accepted != result.failure_reasons.empty()) {
        return false;
    }

    if (!result.invariant_holds && result.accepted) {
        return false;
    }

    if (!result.decision.allow && result.accepted) {
        return false;
    }

    return IsUnitIntervalScore(result.decision.knowledge_factor) &&
           IsUnitIntervalScore(result.decision.eco_impact_value);
}

std::string ExplainWaterBiodiversityInvariant(
    const WaterBiodiversityInvariantResult& result) {
    if (!IsWaterBiodiversityInvariantResultValid(result)) {
        throw std::invalid_argument("water biodiversity invariant result is invalid");
    }

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(6);
    output << "water_biodiversity_invariant\n";
    output << "water_compliant="
           << (result.decision.water_compliant ? "true" : "false") << '\n';
    output << "biodiversity_compliant="
           << (result.decision.biodiversity_compliant ? "true" : "false") << '\n';
    output << "biodiversity_violation="
           << (result.decision.biodiversity_violation ? "true" : "false") << '\n';
    output << "allow="
           << (result.decision.allow ? "true" : "false") << '\n';
    output << "invariant_holds="
           << (result.invariant_holds ? "true" : "false") << '\n';
    output << "accepted="
           << (result.accepted ? "true" : "false") << '\n';
    output << "failure_reason_count="
           << result.failure_reasons.size() << '\n';

    for (std::size_t index = 0; index < result.failure_reasons.size(); ++index) {
        output << "failure_reason_" << index << '='
               << result.failure_reasons[index] << '\n';
    }

    return output.str();
}

bool WaterBiodiversityInvariantSelfTest() {
    const auto valid_water = MakeWaterAllocation();
    const auto valid_biodiversity = MakeBiodiversityIndex();

    const auto accepted = CheckWaterBiodiversityInvariant(
        valid_water,
        valid_biodiversity);

    if (!accepted.accepted ||
        !accepted.invariant_holds ||
        !accepted.decision.allow ||
        !IsWaterBiodiversityInvariantResultValid(accepted)) {
        return false;
    }

    const auto reserve_violation = CheckWaterBiodiversityInvariant(
        MakeWaterAllocationViolatingReserve(),
        valid_biodiversity);

    if (reserve_violation.accepted ||
        reserve_violation.decision.water_compliant ||
        reserve_violation.failure_reasons.empty()) {
        return false;
    }

    const auto biodiversity_violation = CheckWaterBiodiversityInvariant(
        valid_water,
        MakeBiodiversityIndex(500'000, 600'000));

    if (biodiversity_violation.accepted ||
        biodiversity_violation.decision.biodiversity_compliant ||
        biodiversity_violation.decision.allow ||
        !biodiversity_violation.invariant_holds) {
        return false;
    }

    const std::string explanation =
        ExplainWaterBiodiversityInvariant(biodiversity_violation);

    if (explanation.find("biodiversity_compliant=false") ==
            std::string::npos ||
        explanation.find("accepted=false") ==
            std::string::npos ||
        explanation.find("failure_reason_count=") ==
            std::string::npos) {
        return false;
    }

    return true;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

constexpr std::string_view default_foundation_policy_identifier =
    "policy_eco_safe_v1";
constexpr std::string_view default_foundation_action_identifier =
    "diagnostic_review";
constexpr std::uint64_t default_authorization_window_seconds = 300U;
constexpr std::uint64_t default_authorization_issue_lead_seconds = 60U;

eco_restoration::ProofCheckedDispatcher MakeProofCheckedDispatcher(
    std::string_view policy_identifier =
        default_foundation_policy_identifier) {
    if (policy_identifier.empty()) {
        throw std::invalid_argument("proof-checked dispatcher policy is required");
    }

    return eco_restoration::ProofCheckedDispatcher(
        std::string(policy_identifier));
}

eco_restoration::AuthorizationEvidence MakeValidAuthorizationEvidence(
    std::string_view action_identifier,
    std::string_view policy_identifier,
    std::uint64_t now_s,
    std::uint64_t sequence = 1U,
    std::int32_t risk_of_harm_fixed = 200'000) {
    if (action_identifier.empty()) {
        throw std::invalid_argument("authorization action identifier is required");
    }

    if (policy_identifier.empty()) {
        throw std::invalid_argument("authorization policy identifier is required");
    }

    if (sequence == 0U) {
        throw std::invalid_argument("authorization sequence must be positive");
    }

    if (risk_of_harm_fixed < 0 ||
        risk_of_harm_fixed > eco_restoration::risk_of_harm_limit_fixed) {
        throw std::invalid_argument(
            "authorization risk must lie within the safety corridor");
    }

    if (now_s < default_authorization_issue_lead_seconds ||
        now_s > std::numeric_limits<std::uint64_t>::max() -
                    default_authorization_window_seconds) {
        throw std::invalid_argument(
            "authorization time cannot form a valid bounded expiry window");
    }

    return {
        std::string(action_identifier),
        std::string(policy_identifier),
        now_s - default_authorization_issue_lead_seconds,
        now_s + default_authorization_window_seconds,
        sequence,
        risk_of_harm_fixed,
        true
    };
}

eco_restoration::AuthorizationEvidence MakeValidAuthorizationEvidence(
    std::uint64_t now_s,
    std::uint64_t sequence = 1U) {
    return MakeValidAuthorizationEvidence(
        default_foundation_action_identifier,
        default_foundation_policy_identifier,
        now_s,
        sequence);
}

bool AuthorizationEvidenceWindowContains(
    const eco_restoration::AuthorizationEvidence& evidence,
    std::uint64_t now_s) {
    return evidence.issue_time_s <= now_s &&
           now_s <= evidence.expiry_time_s &&
           evidence.issue_time_s <= evidence.expiry_time_s;
}

bool AuthorizationEvidenceHasSafeRisk(
    const eco_restoration::AuthorizationEvidence& evidence) {
    return evidence.risk_of_harm_fixed >= 0 &&
           evidence.risk_of_harm_fixed <=
               eco_restoration::risk_of_harm_limit_fixed;
}

bool AuthorizationEvidenceHasRequiredFields(
    const eco_restoration::AuthorizationEvidence& evidence) {
    return !evidence.action_identifier.empty() &&
           !evidence.policy_identifier.empty() &&
           evidence.sequence > 0U;
}

bool ProofCheckedDispatcherAdapterSelfTest() {
    constexpr std::uint64_t now_s = 10'000U;
    auto dispatcher = MakeProofCheckedDispatcher();
    const auto first = MakeValidAuthorizationEvidence(now_s, 1U);

    if (!AuthorizationEvidenceHasRequiredFields(first) ||
        !AuthorizationEvidenceHasSafeRisk(first) ||
        !AuthorizationEvidenceWindowContains(first, now_s) ||
        !first.externally_verified ||
        !dispatcher.accept(first, now_s) ||
        !dispatcher.has_approved_actuation()) {
        return false;
    }

    const auto& approved = dispatcher.latest_approved_actuation();
    if (approved.action_identifier !=
            default_foundation_action_identifier ||
        approved.policy_identifier !=
            default_foundation_policy_identifier ||
        approved.sequence != 1U ||
        approved.risk_of_harm_fixed != 200'000) {
        return false;
    }

    const auto duplicate_sequence =
        MakeValidAuthorizationEvidence(now_s, 1U);

    if (dispatcher.accept(duplicate_sequence, now_s)) {
        return false;
    }

    const auto next_sequence =
        MakeValidAuthorizationEvidence(now_s, 2U);

    if (!dispatcher.accept(next_sequence, now_s)) {
        return false;
    }

    auto expired = MakeValidAuthorizationEvidence(now_s, 3U);
    expired.expiry_time_s = now_s - 1U;

    if (AuthorizationEvidenceWindowContains(expired, now_s) ||
        dispatcher.accept(expired, now_s)) {
        return false;
    }

    auto future = MakeValidAuthorizationEvidence(now_s, 3U);
    future.issue_time_s = now_s + 1U;

    if (AuthorizationEvidenceWindowContains(future, now_s) ||
        dispatcher.accept(future, now_s)) {
        return false;
    }

    try {
        static_cast<void>(MakeProofCheckedDispatcher(""));
        return false;
    } catch (const std::invalid_argument&) {
    }

    try {
        static_cast<void>(MakeValidAuthorizationEvidence(
            "diagnostic_review",
            "policy_eco_safe_v1",
            now_s,
            0U));
        return false;
    } catch (const std::invalid_argument&) {
    }

    return true;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

struct AuthorizationEvidenceValidation {
    bool valid{};
    std::vector<std::string> failure_reasons;
};

void AddAuthorizationEvidenceFailure(
    AuthorizationEvidenceValidation& validation,
    bool condition,
    std::string_view reason) {
    if (!condition) {
        validation.failure_reasons.emplace_back(reason);
    }
}

AuthorizationEvidenceValidation ValidateAuthorizationEvidenceFixedPoint(
    const eco_restoration::AuthorizationEvidence& evidence,
    std::string_view expected_policy_identifier,
    std::uint64_t now_s,
    std::uint64_t last_accepted_sequence) {
    if (expected_policy_identifier.empty()) {
        throw std::invalid_argument("expected authorization policy is required");
    }

    AuthorizationEvidenceValidation validation;

    AddAuthorizationEvidenceFailure(
        validation,
        !evidence.action_identifier.empty(),
        "authorization action identifier is empty");

    AddAuthorizationEvidenceFailure(
        validation,
        !evidence.policy_identifier.empty(),
        "authorization policy identifier is empty");

    AddAuthorizationEvidenceFailure(
        validation,
        evidence.policy_identifier == expected_policy_identifier,
        "authorization policy identifier does not match expected policy");

    AddAuthorizationEvidenceFailure(
        validation,
        evidence.externally_verified,
        "authorization evidence was not externally verified");

    AddAuthorizationEvidenceFailure(
        validation,
        evidence.sequence > 0U,
        "authorization sequence must be positive");

    AddAuthorizationEvidenceFailure(
        validation,
        evidence.sequence > last_accepted_sequence,
        "authorization sequence is not strictly monotonic");

    AddAuthorizationEvidenceFailure(
        validation,
        evidence.issue_time_s <= evidence.expiry_time_s,
        "authorization issue time exceeds expiry time");

    AddAuthorizationEvidenceFailure(
        validation,
        evidence.issue_time_s <= now_s,
        "authorization evidence is not active yet");

    AddAuthorizationEvidenceFailure(
        validation,
        now_s <= evidence.expiry_time_s,
        "authorization evidence has expired");

    AddAuthorizationEvidenceFailure(
        validation,
        evidence.risk_of_harm_fixed >= 0,
        "authorization fixed-point risk is below zero");

    AddAuthorizationEvidenceFailure(
        validation,
        evidence.risk_of_harm_fixed <=
            eco_restoration::unit_interval_scale,
        "authorization fixed-point risk exceeds the unit interval");

    AddAuthorizationEvidenceFailure(
        validation,
        evidence.risk_of_harm_fixed <=
            eco_restoration::risk_of_harm_limit_fixed,
        "authorization fixed-point risk exceeds the 0.30 safety corridor");

    validation.valid = validation.failure_reasons.empty();
    return validation;
}

bool AuthorizationEvidenceValidationHasReason(
    const AuthorizationEvidenceValidation& validation,
    std::string_view reason) {
    return std::find(
        validation.failure_reasons.begin(),
        validation.failure_reasons.end(),
        reason) != validation.failure_reasons.end();
}

std::string ExplainAuthorizationEvidenceValidation(
    const AuthorizationEvidenceValidation& validation) {
    std::ostringstream output;
    output << "authorization_evidence_validation\n";
    output << "valid=" << (validation.valid ? "true" : "false") << '\n';
    output << "failure_reason_count="
           << validation.failure_reasons.size() << '\n';

    for (std::size_t index = 0;
         index < validation.failure_reasons.size();
         ++index) {
        output << "failure_reason_" << index << '='
               << validation.failure_reasons[index] << '\n';
    }

    return output.str();
}

bool AuthorizationEvidenceFixedPointValidationSelfTest() {
    constexpr std::uint64_t now_s = 50'000U;
    const auto valid = MakeValidAuthorizationEvidence(
        "diagnostic_review",
        "policy_eco_safe_v1",
        now_s,
        4U,
        200'000);

    const auto accepted = ValidateAuthorizationEvidenceFixedPoint(
        valid,
        "policy_eco_safe_v1",
        now_s,
        3U);

    if (!accepted.valid ||
        !accepted.failure_reasons.empty()) {
        return false;
    }

    auto policy_mismatch = valid;
    policy_mismatch.policy_identifier = "different_policy";

    const auto mismatched = ValidateAuthorizationEvidenceFixedPoint(
        policy_mismatch,
        "policy_eco_safe_v1",
        now_s,
        3U);

    if (mismatched.valid ||
        !AuthorizationEvidenceValidationHasReason(
            mismatched,
            "authorization policy identifier does not match expected policy")) {
        return false;
    }

    auto stale_sequence = valid;
    stale_sequence.sequence = 3U;

    const auto stale = ValidateAuthorizationEvidenceFixedPoint(
        stale_sequence,
        "policy_eco_safe_v1",
        now_s,
        3U);

    if (stale.valid ||
        !AuthorizationEvidenceValidationHasReason(
            stale,
            "authorization sequence is not strictly monotonic")) {
        return false;
    }

    auto expired = valid;
    expired.expiry_time_s = now_s - 1U;

    const auto expired_validation = ValidateAuthorizationEvidenceFixedPoint(
        expired,
        "policy_eco_safe_v1",
        now_s,
        3U);

    if (expired_validation.valid ||
        !AuthorizationEvidenceValidationHasReason(
            expired_validation,
            "authorization evidence has expired")) {
        return false;
    }

    auto unsafe_risk = valid;
    unsafe_risk.risk_of_harm_fixed = 300'001;

    const auto unsafe_validation = ValidateAuthorizationEvidenceFixedPoint(
        unsafe_risk,
        "policy_eco_safe_v1",
        now_s,
        3U);

    if (unsafe_validation.valid ||
        !AuthorizationEvidenceValidationHasReason(
            unsafe_validation,
            "authorization fixed-point risk exceeds the 0.30 safety corridor")) {
        return false;
    }

    const std::string explanation =
        ExplainAuthorizationEvidenceValidation(unsafe_validation);

    return explanation.find("valid=false") != std::string::npos &&
           explanation.find("failure_reason_count=") != std::string::npos;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

constexpr double default_population_abundance = 100.0;
constexpr double default_population_time_step = 1.0;
constexpr double default_population_growth_rate = 0.05;
constexpr double default_population_treatment_effect = 0.10;
constexpr double default_population_diffusion_scale = 0.02;
constexpr double default_population_running_cost = 0.01;
constexpr double default_population_value_gradient = 0.50;
constexpr double default_population_value_curvature = -0.01;

bool IsFiniteNonnegative(double value) {
    return std::isfinite(value) && value >= 0.0;
}

bool IsStochasticPopulationModelValid(
    const eco_restoration::StochasticPopulationModel& model) {
    return IsFiniteNonnegative(model.current_abundance) &&
           std::isfinite(model.time_step) &&
           model.time_step > 0.0 &&
           IsFiniteNonnegative(model.drift_growth_rate) &&
           IsFiniteNonnegative(model.treatment_effect) &&
           IsFiniteNonnegative(model.diffusion_scale) &&
           IsFiniteNonnegative(model.running_abundance_cost) &&
           std::isfinite(model.value_gradient) &&
           std::isfinite(model.value_curvature);
}

void ValidateStochasticPopulationModel(
    const eco_restoration::StochasticPopulationModel& model) {
    if (!std::isfinite(model.current_abundance) ||
        model.current_abundance < 0.0) {
        throw std::invalid_argument(
            "stochastic population abundance must be finite and nonnegative");
    }

    if (!std::isfinite(model.time_step) ||
        model.time_step <= 0.0) {
        throw std::invalid_argument(
            "stochastic population time step must be finite and positive");
    }

    if (!std::isfinite(model.drift_growth_rate) ||
        model.drift_growth_rate < 0.0) {
        throw std::invalid_argument(
            "stochastic population drift growth rate must be finite and nonnegative");
    }

    if (!std::isfinite(model.treatment_effect) ||
        model.treatment_effect < 0.0) {
        throw std::invalid_argument(
            "stochastic population treatment effect must be finite and nonnegative");
    }

    if (!std::isfinite(model.diffusion_scale) ||
        model.diffusion_scale < 0.0) {
        throw std::invalid_argument(
            "stochastic population diffusion scale must be finite and nonnegative");
    }

    if (!std::isfinite(model.running_abundance_cost) ||
        model.running_abundance_cost < 0.0) {
        throw std::invalid_argument(
            "stochastic population running cost must be finite and nonnegative");
    }

    if (!std::isfinite(model.value_gradient) ||
        !std::isfinite(model.value_curvature)) {
        throw std::invalid_argument(
            "stochastic population value derivatives must be finite");
    }
}

eco_restoration::StochasticPopulationModel MakeStochasticPopulationModel() {
    const eco_restoration::StochasticPopulationModel model{
        default_population_abundance,
        default_population_time_step,
        default_population_growth_rate,
        default_population_treatment_effect,
        default_population_diffusion_scale,
        default_population_running_cost,
        default_population_value_gradient,
        default_population_value_curvature
    };

    ValidateStochasticPopulationModel(model);
    return model;
}

eco_restoration::StochasticPopulationModel MakeStochasticPopulationModel(
    double abundance,
    double time_step,
    double drift_growth_rate,
    double treatment_effect,
    double diffusion_scale,
    double running_abundance_cost,
    double value_gradient,
    double value_curvature) {
    const eco_restoration::StochasticPopulationModel model{
        abundance,
        time_step,
        drift_growth_rate,
        treatment_effect,
        diffusion_scale,
        running_abundance_cost,
        value_gradient,
        value_curvature
    };

    ValidateStochasticPopulationModel(model);
    return model;
}

double ExpectedUncontrolledPopulationChange(
    const eco_restoration::StochasticPopulationModel& model) {
    ValidateStochasticPopulationModel(model);
    return model.time_step *
           model.drift_growth_rate *
           model.current_abundance;
}

double PopulationDiffusionVariance(
    const eco_restoration::StochasticPopulationModel& model) {
    ValidateStochasticPopulationModel(model);
    return model.diffusion_scale *
           model.diffusion_scale *
           model.current_abundance *
           model.time_step;
}

std::string ExplainStochasticPopulationModel(
    const eco_restoration::StochasticPopulationModel& model) {
    ValidateStochasticPopulationModel(model);

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(6);
    output << "stochastic_population_model\n";
    output << "current_abundance=" << model.current_abundance << '\n';
    output << "time_step=" << model.time_step << '\n';
    output << "drift_growth_rate=" << model.drift_growth_rate << '\n';
    output << "treatment_effect=" << model.treatment_effect << '\n';
    output << "diffusion_scale=" << model.diffusion_scale << '\n';
    output << "running_abundance_cost="
           << model.running_abundance_cost << '\n';
    output << "expected_uncontrolled_change="
           << ExpectedUncontrolledPopulationChange(model) << '\n';
    output << "diffusion_variance="
           << PopulationDiffusionVariance(model) << '\n';
    return output.str();
}

bool StochasticPopulationModelFixtureSelfTest() {
    const auto defaults = MakeStochasticPopulationModel();

    if (!IsStochasticPopulationModelValid(defaults) ||
        std::abs(defaults.current_abundance -
                 default_population_abundance) > 1e-12 ||
        std::abs(ExpectedUncontrolledPopulationChange(defaults) -
                 5.0) > 1e-12 ||
        std::abs(PopulationDiffusionVariance(defaults) -
                 0.04) > 1e-12) {
        return false;
    }

    const std::string explanation =
        ExplainStochasticPopulationModel(defaults);

    if (explanation.find("current_abundance=100.000000") ==
            std::string::npos ||
        explanation.find("diffusion_variance=0.040000") ==
            std::string::npos) {
        return false;
    }

    try {
        static_cast<void>(MakeStochasticPopulationModel(
            -1.0, 1.0, 0.05, 0.10, 0.02, 0.01, 0.50, -0.01));
        return false;
    } catch (const std::invalid_argument&) {
    }

    try {
        static_cast<void>(MakeStochasticPopulationModel(
            100.0, 0.0, 0.05, 0.10, 0.02, 0.01, 0.50, -0.01));
        return false;
    } catch (const std::invalid_argument&) {
    }

    try {
        static_cast<void>(MakeStochasticPopulationModel(
            100.0, 1.0, 0.05, 0.10, -0.02, 0.01, 0.50, -0.01));
        return false;
    } catch (const std::invalid_argument&) {
    }

    return true;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

bool IsInvasiveControlCandidateValid(
    const eco_restoration::InvasiveControlCandidate& candidate) {
    return IsFiniteNonnegative(candidate.treatment_intensity) &&
           IsFiniteNonnegative(candidate.treatment_cost) &&
           IsFiniteNonnegative(candidate.expected_benefit) &&
           std::isfinite(candidate.risk_of_harm) &&
           candidate.risk_of_harm >= 0.0 &&
           candidate.risk_of_harm <= 1.0;
}

void ValidateInvasiveControlCandidate(
    const eco_restoration::InvasiveControlCandidate& candidate) {
    if (!IsInvasiveControlCandidateValid(candidate)) {
        throw std::invalid_argument(
            "invasive-control candidate contains an invalid bounded value");
    }
}

double ExpectedPopulationAfterTreatment(
    const eco_restoration::StochasticPopulationModel& model,
    const eco_restoration::InvasiveControlCandidate& candidate) {
    ValidateStochasticPopulationModel(model);
    ValidateInvasiveControlCandidate(candidate);

    const double drift =
        model.drift_growth_rate * model.current_abundance -
        model.treatment_effect * candidate.treatment_intensity *
            model.current_abundance;

    return model.current_abundance + model.time_step * drift;
}

bool IsSafeInvasiveControlCandidate(
    const eco_restoration::StochasticPopulationModel& model,
    const eco_restoration::InvasiveControlCandidate& candidate) {
    if (!IsInvasiveControlCandidateValid(candidate)) return false;

    return candidate.expected_benefit >= candidate.treatment_cost &&
           candidate.risk_of_harm <= 0.30 &&
           ExpectedPopulationAfterTreatment(model, candidate) >= 0.0;
}

std::vector<eco_restoration::InvasiveControlCandidate>
MakeInvasiveControlCandidates() {
    const std::vector<eco_restoration::InvasiveControlCandidate> candidates{
        {0.30, 10.0, 15.0, 0.20},
        {0.50, 20.0, 24.0, 0.25},
        {0.80, 12.0, 10.0, 0.34}
    };

    if (candidates.size() < 3U) {
        throw std::runtime_error(
            "invasive-control fixture requires at least three candidates");
    }

    for (const auto& candidate : candidates) {
        ValidateInvasiveControlCandidate(candidate);
    }

    return candidates;
}

std::size_t CountSafeInvasiveControlCandidates(
    const eco_restoration::StochasticPopulationModel& model,
    const std::vector<eco_restoration::InvasiveControlCandidate>& candidates) {
    ValidateStochasticPopulationModel(model);

    std::size_t count = 0U;
    for (const auto& candidate : candidates) {
        if (IsSafeInvasiveControlCandidate(model, candidate)) {
            ++count;
        }
    }
    return count;
}

std::string ExplainInvasiveControlCandidates(
    const eco_restoration::StochasticPopulationModel& model,
    const std::vector<eco_restoration::InvasiveControlCandidate>& candidates) {
    ValidateStochasticPopulationModel(model);

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(6);
    output << "invasive_control_candidates\n";
    output << "candidate_count=" << candidates.size() << '\n';

    for (std::size_t index = 0U; index < candidates.size(); ++index) {
        const auto& candidate = candidates[index];
        ValidateInvasiveControlCandidate(candidate);

        output << "candidate_" << index << "_intensity="
               << candidate.treatment_intensity << '\n';
        output << "candidate_" << index << "_cost="
               << candidate.treatment_cost << '\n';
        output << "candidate_" << index << "_benefit="
               << candidate.expected_benefit << '\n';
        output << "candidate_" << index << "_risk_of_harm="
               << candidate.risk_of_harm << '\n';
        output << "candidate_" << index << "_expected_population="
               << ExpectedPopulationAfterTreatment(model, candidate) << '\n';
        output << "candidate_" << index << "_safe="
               << (IsSafeInvasiveControlCandidate(model, candidate)
                   ? "true" : "false") << '\n';
    }

    output << "safe_candidate_count="
           << CountSafeInvasiveControlCandidates(model, candidates) << '\n';
    return output.str();
}

bool InvasiveControlCandidateFixtureSelfTest() {
    const auto model = MakeStochasticPopulationModel();
    const auto candidates = MakeInvasiveControlCandidates();

    if (candidates.size() != 3U ||
        CountSafeInvasiveControlCandidates(model, candidates) != 2U ||
        !IsSafeInvasiveControlCandidate(model, candidates[0]) ||
        !IsSafeInvasiveControlCandidate(model, candidates[1]) ||
        IsSafeInvasiveControlCandidate(model, candidates[2])) {
        return false;
    }

    if (candidates[2].expected_benefit >= candidates[2].treatment_cost ||
        candidates[2].risk_of_harm <= 0.30) {
        return false;
    }

    const auto decision =
        eco_restoration::select_safe_stochastic_invasive_control(
            model,
            candidates);

    if (!decision.safe ||
        decision.treatment_intensity != candidates[0].treatment_intensity) {
        return false;
    }

    const std::string explanation =
        ExplainInvasiveControlCandidates(model, candidates);

    if (explanation.find("candidate_0_safe=true") ==
            std::string::npos ||
        explanation.find("candidate_2_safe=false") ==
            std::string::npos ||
        explanation.find("safe_candidate_count=2") ==
            std::string::npos) {
        return false;
    }

    try {
        ValidateInvasiveControlCandidate({-0.1, 1.0, 2.0, 0.2});
        return false;
    } catch (const std::invalid_argument&) {
    }

    return true;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

struct SafeInvasiveControlSelectionResult {
    eco_restoration::StochasticControlDecision decision;
    bool selected{};
    bool used_fallback{};
    std::vector<std::string> failure_reasons;
};

void AddInvasiveControlSelectionFailure(
    SafeInvasiveControlSelectionResult& result,
    bool condition,
    std::string_view reason) {
    if (!condition) {
        result.failure_reasons.emplace_back(reason);
    }
}

eco_restoration::StochasticControlDecision MakeInvasiveControlFallback(
    const eco_restoration::StochasticPopulationModel& model) {
    ValidateStochasticPopulationModel(model);
    return {
        0.0,
        std::numeric_limits<double>::infinity(),
        model.current_abundance,
        PopulationDiffusionVariance(model),
        false,
        0.0,
        0.0
    };
}

SafeInvasiveControlSelectionResult SelectSafeInvasiveControlWrapper(
    const eco_restoration::StochasticPopulationModel& model,
    const std::vector<eco_restoration::InvasiveControlCandidate>& candidates) {
    SafeInvasiveControlSelectionResult result;
    result.decision = MakeInvasiveControlFallback(model);
    result.used_fallback = true;

    AddInvasiveControlSelectionFailure(
        result,
        !candidates.empty(),
        "no invasive-control candidates were supplied");

    if (candidates.empty()) {
        return result;
    }

    std::size_t structurally_valid_count = 0U;
    std::size_t safe_count = 0U;

    for (const auto& candidate : candidates) {
        if (!IsInvasiveControlCandidateValid(candidate)) {
            continue;
        }

        ++structurally_valid_count;
        if (IsSafeInvasiveControlCandidate(model, candidate)) {
            ++safe_count;
        }
    }

    AddInvasiveControlSelectionFailure(
        result,
        structurally_valid_count == candidates.size(),
        "one or more invasive-control candidates had invalid values");

    AddInvasiveControlSelectionFailure(
        result,
        safe_count > 0U,
        "no invasive-control candidate satisfied benefit, risk, and state corridors");

    if (!result.failure_reasons.empty()) {
        return result;
    }

    result.decision =
        eco_restoration::select_safe_stochastic_invasive_control(
            model,
            candidates);

    AddInvasiveControlSelectionFailure(
        result,
        result.decision.safe,
        "HJB selection did not return a safe invasive-control decision");

    if (!result.failure_reasons.empty()) {
        result.decision = MakeInvasiveControlFallback(model);
        return result;
    }

    const double selected_risk =
        SelectedInvasiveRiskOfHarm(candidates, result.decision);

    AddInvasiveControlSelectionFailure(
        result,
        selected_risk <= 0.30,
        "selected invasive-control candidate exceeded the 0.30 risk corridor");

    if (!result.failure_reasons.empty()) {
        result.decision = MakeInvasiveControlFallback(model);
        return result;
    }

    result.selected = true;
    result.used_fallback = false;
    return result;
}

bool IsSafeInvasiveControlSelectionResultValid(
    const SafeInvasiveControlSelectionResult& result) {
    if (result.selected) {
        return result.decision.safe &&
               !result.used_fallback &&
               result.failure_reasons.empty();
    }

    return !result.decision.safe &&
           result.used_fallback &&
           !result.failure_reasons.empty();
}

std::string ExplainSafeInvasiveControlSelection(
    const SafeInvasiveControlSelectionResult& result) {
    if (!IsSafeInvasiveControlSelectionResultValid(result)) {
        throw std::invalid_argument("safe invasive-control selection result is invalid");
    }

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(6);
    output << "safe_invasive_control_selection\n";
    output << "selected=" << (result.selected ? "true" : "false") << '\n';
    output << "used_fallback="
           << (result.used_fallback ? "true" : "false") << '\n';
    output << "treatment_intensity="
           << result.decision.treatment_intensity << '\n';
    output << "hjb_value="
           << result.decision.hamilton_jacobi_bellman_value << '\n';
    output << "expected_next_abundance="
           << result.decision.expected_next_abundance << '\n';
    output << "decision_safe="
           << (result.decision.safe ? "true" : "false") << '\n';
    output << "failure_reason_count="
           << result.failure_reasons.size() << '\n';

    for (std::size_t index = 0U;
         index < result.failure_reasons.size();
         ++index) {
        output << "failure_reason_" << index << '='
               << result.failure_reasons[index] << '\n';
    }

    return output.str();
}

bool SafeInvasiveControlSelectionWrapperSelfTest() {
    const auto model = MakeStochasticPopulationModel();
    const auto candidates = MakeInvasiveControlCandidates();

    const auto selected = SelectSafeInvasiveControlWrapper(
        model,
        candidates);

    if (!selected.selected ||
        selected.used_fallback ||
        !selected.decision.safe ||
        !selected.failure_reasons.empty() ||
        !IsSafeInvasiveControlSelectionResultValid(selected)) {
        return false;
    }

    const std::vector<eco_restoration::InvasiveControlCandidate> unsafe_only{
        {0.20, 12.0, 10.0, 0.20},
        {0.30, 10.0, 12.0, 0.31},
        {0.40, 15.0, 13.0, 0.35}
    };

    const auto fallback = SelectSafeInvasiveControlWrapper(
        model,
        unsafe_only);

    if (fallback.selected ||
        !fallback.used_fallback ||
        fallback.decision.safe ||
        fallback.failure_reasons.empty() ||
        !IsSafeInvasiveControlSelectionResultValid(fallback)) {
        return false;
    }

    const std::string explanation =
        ExplainSafeInvasiveControlSelection(fallback);

    if (explanation.find("selected=false") == std::string::npos ||
        explanation.find("used_fallback=true") == std::string::npos ||
        explanation.find("failure_reason_count=1") == std::string::npos) {
        return false;
    }

    const auto empty = SelectSafeInvasiveControlWrapper(model, {});
    return !empty.selected &&
           empty.used_fallback &&
           !empty.failure_reasons.empty();
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

struct AnchorAuditAppendResult {
    eco_restoration::AnchorAuditDecision decision;
    bool input_accepted{};
    bool caught_validation_error{};
    std::vector<std::string> failure_reasons;
};

void AddAnchorAuditFailure(
    AnchorAuditAppendResult& result,
    bool condition,
    std::string_view reason) {
    if (!condition) {
        result.failure_reasons.emplace_back(reason);
    }
}

AnchorAuditAppendResult AppendAnchorAuditRecordSafely(
    eco_restoration::HexAnchorAuditStore& store,
    const eco_restoration::AnchorAuditRecord& record) {
    AnchorAuditAppendResult result;

    try {
        result.decision = store.append(record);
        result.input_accepted = result.decision.accepted;

        AddAnchorAuditFailure(
            result,
            result.decision.invariant_holds,
            "anchor audit safety invariant did not hold");

        AddAnchorAuditFailure(
            result,
            result.decision.accepted,
            "anchor audit record was not accepted");

        return result;
    } catch (const std::invalid_argument& error) {
        result.caught_validation_error = true;
        result.failure_reasons.emplace_back(error.what());
        return result;
    } catch (const std::exception& error) {
        result.caught_validation_error = true;
        result.failure_reasons.emplace_back(error.what());
        return result;
    }
}

bool IsAnchorAuditAppendResultValid(
    const AnchorAuditAppendResult& result) {
    if (result.caught_validation_error) {
        return !result.input_accepted &&
               !result.failure_reasons.empty();
    }

    if (result.input_accepted) {
        return result.decision.accepted &&
               result.decision.invariant_holds &&
               result.failure_reasons.empty();
    }

    return !result.decision.accepted &&
           !result.failure_reasons.empty();
}

std::string ExplainAnchorAuditAppendResult(
    const AnchorAuditAppendResult& result) {
    if (!IsAnchorAuditAppendResultValid(result)) {
        throw std::invalid_argument("anchor audit append result is invalid");
    }

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(6);
    output << "anchor_audit_append\n";
    output << "input_accepted="
           << (result.input_accepted ? "true" : "false") << '\n';
    output << "caught_validation_error="
           << (result.caught_validation_error ? "true" : "false") << '\n';
    output << "invariant_holds="
           << (result.decision.invariant_holds ? "true" : "false") << '\n';
    output << "knowledge_factor="
           << result.decision.knowledge_factor << '\n';
    output << "eco_impact_value="
           << result.decision.eco_impact_value << '\n';
    output << "failure_reason_count="
           << result.failure_reasons.size() << '\n';

    for (std::size_t index = 0U;
         index < result.failure_reasons.size();
         ++index) {
        output << "failure_reason_" << index << '='
               << result.failure_reasons[index] << '\n';
    }

    return output.str();
}

eco_restoration::AnchorAuditRecord MakeAnchorAuditRecord(
    std::uint64_t sequence,
    std::string previous_reference,
    std::string anchor_reference,
    std::int32_t risk_of_harm_fixed,
    bool allow) {
    return {
        sequence,
        std::move(previous_reference),
        std::move(anchor_reference),
        0x8928308280fffffULL,
        4580,
        risk_of_harm_fixed,
        true,
        allow
    };
}

bool AnchorAuditStoreIntegrationSelfTest() {
    eco_restoration::HexAnchorAuditStore store;

    const auto first = AppendAnchorAuditRecordSafely(
        store,
        MakeAnchorAuditRecord(
            1U,
            "",
            "phoenix_anchor_001",
            180'000,
            true));

    if (!first.input_accepted ||
        first.caught_validation_error ||
        !first.decision.invariant_holds ||
        !IsAnchorAuditAppendResultValid(first) ||
        store.records().size() != 1U) {
        return false;
    }

    const auto second = AppendAnchorAuditRecordSafely(
        store,
        MakeAnchorAuditRecord(
            2U,
            "phoenix_anchor_001",
            "phoenix_anchor_002",
            220'000,
            true));

    if (!second.input_accepted ||
        second.caught_validation_error ||
        store.records().size() != 2U) {
        return false;
    }

    const auto invalid_sequence = AppendAnchorAuditRecordSafely(
        store,
        MakeAnchorAuditRecord(
            4U,
            "phoenix_anchor_002",
            "phoenix_anchor_004",
            180'000,
            true));

    if (invalid_sequence.input_accepted ||
        !invalid_sequence.caught_validation_error ||
        invalid_sequence.failure_reasons.empty() ||
        store.records().size() != 2U) {
        return false;
    }

    const auto invalid_predecessor = AppendAnchorAuditRecordSafely(
        store,
        MakeAnchorAuditRecord(
            3U,
            "incorrect_predecessor",
            "phoenix_anchor_003",
            180'000,
            true));

    if (invalid_predecessor.input_accepted ||
        !invalid_predecessor.caught_validation_error ||
        store.records().size() != 2U) {
        return false;
    }

    const std::string explanation =
        ExplainAnchorAuditAppendResult(invalid_predecessor);

    return explanation.find("input_accepted=false") != std::string::npos &&
           explanation.find("caught_validation_error=true") !=
               std::string::npos;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

constexpr std::uint64_t default_anchor_sequence = 1U;
constexpr std::uint64_t default_anchor_cell_id = 617700169958293503ULL;
constexpr std::int32_t default_anchor_heat_index_fixed = 4580;
constexpr std::int32_t default_anchor_safe_risk_fixed = 180'000;
constexpr std::int32_t default_anchor_unsafe_risk_fixed = 300'001;

bool IsAnchorAuditRecordStructurallyValid(
    const eco_restoration::AnchorAuditRecord& record) {
    return record.sequence > 0U &&
           !record.anchor_reference.empty() &&
           record.risk_of_harm_fixed >= 0 &&
           record.risk_of_harm_fixed <=
               eco_restoration::unit_interval_scale;
}

bool AnchorAuditRecordHasValidPredecessorShape(
    const eco_restoration::AnchorAuditRecord& record) {
    if (record.sequence == 1U) {
        return record.previous_reference.empty();
    }
    return !record.previous_reference.empty();
}

bool AnchorAuditRecordIsSafe(
    const eco_restoration::AnchorAuditRecord& record) {
    return IsAnchorAuditRecordStructurallyValid(record) &&
           AnchorAuditRecordHasValidPredecessorShape(record) &&
           record.externally_verified_commitment &&
           record.risk_of_harm_fixed <=
               eco_restoration::risk_of_harm_limit_fixed;
}

eco_restoration::AnchorAuditRecord MakeValidAnchorAuditRecord(
    std::uint64_t sequence = default_anchor_sequence,
    std::string previous_reference = {},
    std::string anchor_reference = "phoenix_anchor_001") {
    if (sequence == 0U) {
        throw std::invalid_argument("anchor sequence must be positive");
    }

    if (sequence == 1U) {
        previous_reference.clear();
    } else if (previous_reference.empty()) {
        throw std::invalid_argument(
            "noninitial anchor records require a predecessor reference");
    }

    eco_restoration::AnchorAuditRecord record{
        sequence,
        std::move(previous_reference),
        std::move(anchor_reference),
        default_anchor_cell_id,
        default_anchor_heat_index_fixed,
        default_anchor_safe_risk_fixed,
        true,
        true
    };

    if (!AnchorAuditRecordIsSafe(record)) {
        throw std::runtime_error("valid anchor fixture did not satisfy safety invariants");
    }

    return record;
}

eco_restoration::AnchorAuditRecord MakeUnsafeAnchorAuditRecord(
    std::uint64_t sequence = default_anchor_sequence,
    std::string previous_reference = {},
    std::string anchor_reference = "phoenix_anchor_unsafe_001") {
    if (sequence == 0U) {
        throw std::invalid_argument("anchor sequence must be positive");
    }

    if (sequence == 1U) {
        previous_reference.clear();
    } else if (previous_reference.empty()) {
        throw std::invalid_argument(
            "noninitial anchor records require a predecessor reference");
    }

    eco_restoration::AnchorAuditRecord record{
        sequence,
        std::move(previous_reference),
        std::move(anchor_reference),
        default_anchor_cell_id,
        default_anchor_heat_index_fixed,
        default_anchor_unsafe_risk_fixed,
        true,
        true
    };

    if (!IsAnchorAuditRecordStructurallyValid(record) ||
        record.risk_of_harm_fixed <=
            eco_restoration::risk_of_harm_limit_fixed ||
        !record.allow) {
        throw std::runtime_error(
            "unsafe anchor fixture must expose a risk-and-allow invariant violation");
    }

    return record;
}

std::string ExplainAnchorAuditRecord(
    const eco_restoration::AnchorAuditRecord& record) {
    if (!IsAnchorAuditRecordStructurallyValid(record)) {
        throw std::invalid_argument("anchor audit record is structurally invalid");
    }

    std::ostringstream output;
    output << "anchor_audit_record\n";
    output << "sequence=" << record.sequence << '\n';
    output << "has_predecessor="
           << (!record.previous_reference.empty() ? "true" : "false") << '\n';
    output << "anchor_reference=" << record.anchor_reference << '\n';
    output << "h3_cell_id=" << record.h3_cell_id << '\n';
    output << "heat_index_fixed=" << record.heat_index_fixed << '\n';
    output << "risk_of_harm_fixed=" << record.risk_of_harm_fixed << '\n';
    output << "risk_of_harm="
           << RiskOfHarmFromFixed(record.risk_of_harm_fixed) << '\n';
    output << "externally_verified="
           << (record.externally_verified_commitment ? "true" : "false") << '\n';
    output << "allow=" << (record.allow ? "true" : "false") << '\n';
    output << "safe=" << (AnchorAuditRecordIsSafe(record) ? "true" : "false") << '\n';
    return output.str();
}

bool AnchorAuditRecordFixtureSelfTest() {
    const auto valid = MakeValidAnchorAuditRecord();

    if (!IsAnchorAuditRecordStructurallyValid(valid) ||
        !AnchorAuditRecordHasValidPredecessorShape(valid) ||
        !AnchorAuditRecordIsSafe(valid) ||
        valid.risk_of_harm_fixed != default_anchor_safe_risk_fixed) {
        return false;
    }

    const auto unsafe = MakeUnsafeAnchorAuditRecord();

    if (!IsAnchorAuditRecordStructurallyValid(unsafe) ||
        AnchorAuditRecordIsSafe(unsafe) ||
        unsafe.risk_of_harm_fixed != default_anchor_unsafe_risk_fixed ||
        !unsafe.allow) {
        return false;
    }

    const auto chained = MakeValidAnchorAuditRecord(
        2U,
        "phoenix_anchor_001",
        "phoenix_anchor_002");

    if (!AnchorAuditRecordHasValidPredecessorShape(chained) ||
        !AnchorAuditRecordIsSafe(chained)) {
        return false;
    }

    const std::string explanation = ExplainAnchorAuditRecord(unsafe);

    if (explanation.find("risk_of_harm_fixed=300001") ==
            std::string::npos ||
        explanation.find("safe=false") == std::string::npos) {
        return false;
    }

    try {
        static_cast<void>(MakeValidAnchorAuditRecord(
            2U,
            "",
            "phoenix_anchor_002"));
        return false;
    } catch (const std::invalid_argument&) {
    }

    return true;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

constexpr double default_initial_moisture_mm = 15.0;
constexpr double default_evapotranspiration_mm = 3.0;
constexpr double default_drainage_fraction = 0.10;
constexpr double default_moisture_minimum_mm = 10.0;
constexpr double default_moisture_maximum_mm = 30.0;
constexpr double default_terminal_minimum_mm = 12.0;
constexpr double default_terminal_maximum_mm = 25.0;
constexpr double default_irrigation_maximum_mm = 8.0;
constexpr double default_irrigation_cost = 0.10;
constexpr double default_stress_cost = 0.50;

bool IsIrrigationDynamicsFinite(
    const eco_restoration::IrrigationDynamics& dynamics) {
    return std::isfinite(dynamics.initial_moisture_mm) &&
           std::isfinite(dynamics.evapotranspiration_mm_per_step) &&
           std::isfinite(dynamics.drainage_fraction) &&
           std::isfinite(dynamics.moisture_min_mm) &&
           std::isfinite(dynamics.moisture_max_mm) &&
           std::isfinite(dynamics.terminal_min_mm) &&
           std::isfinite(dynamics.terminal_max_mm) &&
           std::isfinite(dynamics.irrigation_max_mm_per_step) &&
           std::isfinite(dynamics.irrigation_cost) &&
           std::isfinite(dynamics.stress_cost);
}

void ValidateIrrigationDynamics(
    const eco_restoration::IrrigationDynamics& dynamics) {
    if (!IsIrrigationDynamicsFinite(dynamics)) {
        throw std::invalid_argument("irrigation dynamics values must be finite");
    }

    if (dynamics.evapotranspiration_mm_per_step < 0.0 ||
        dynamics.drainage_fraction < 0.0 ||
        dynamics.drainage_fraction > 1.0 ||
        dynamics.irrigation_max_mm_per_step < 0.0 ||
        dynamics.irrigation_cost < 0.0 ||
        dynamics.stress_cost < 0.0) {
        throw std::invalid_argument(
            "irrigation losses, limits, and costs must be nonnegative");
    }

    if (dynamics.moisture_min_mm > dynamics.moisture_max_mm) {
        throw std::invalid_argument(
            "irrigation moisture minimum must not exceed maximum");
    }

    if (dynamics.initial_moisture_mm < dynamics.moisture_min_mm ||
        dynamics.initial_moisture_mm > dynamics.moisture_max_mm) {
        throw std::invalid_argument(
            "initial moisture must lie within moisture bounds");
    }

    if (dynamics.terminal_min_mm > dynamics.terminal_max_mm) {
        throw std::invalid_argument(
            "terminal moisture minimum must not exceed maximum");
    }

    if (dynamics.terminal_min_mm < dynamics.moisture_min_mm ||
        dynamics.terminal_max_mm > dynamics.moisture_max_mm) {
        throw std::invalid_argument(
            "terminal moisture bounds must lie within moisture bounds");
    }
}

bool IsIrrigationDynamicsValid(
    const eco_restoration::IrrigationDynamics& dynamics) {
    try {
        ValidateIrrigationDynamics(dynamics);
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

eco_restoration::IrrigationDynamics MakeIrrigationDynamics() {
    const eco_restoration::IrrigationDynamics dynamics{
        default_initial_moisture_mm,
        default_evapotranspiration_mm,
        default_drainage_fraction,
        default_moisture_minimum_mm,
        default_moisture_maximum_mm,
        default_terminal_minimum_mm,
        default_terminal_maximum_mm,
        default_irrigation_maximum_mm,
        default_irrigation_cost,
        default_stress_cost
    };

    ValidateIrrigationDynamics(dynamics);
    return dynamics;
}

std::string ExplainIrrigationDynamics(
    const eco_restoration::IrrigationDynamics& dynamics) {
    ValidateIrrigationDynamics(dynamics);

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(6);
    output << "irrigation_dynamics\n";
    output << "initial_moisture_mm=" << dynamics.initial_moisture_mm << '\n';
    output << "evapotranspiration_mm_per_step="
           << dynamics.evapotranspiration_mm_per_step << '\n';
    output << "drainage_fraction="
           << dynamics.drainage_fraction << '\n';
    output << "moisture_min_mm=" << dynamics.moisture_min_mm << '\n';
    output << "moisture_max_mm=" << dynamics.moisture_max_mm << '\n';
    output << "terminal_min_mm=" << dynamics.terminal_min_mm << '\n';
    output << "terminal_max_mm=" << dynamics.terminal_max_mm << '\n';
    output << "irrigation_max_mm_per_step="
           << dynamics.irrigation_max_mm_per_step << '\n';
    output << "irrigation_cost=" << dynamics.irrigation_cost << '\n';
    output << "stress_cost=" << dynamics.stress_cost << '\n';
    return output.str();
}

bool IrrigationDynamicsFixtureSelfTest() {
    const auto defaults = MakeIrrigationDynamics();

    if (!IsIrrigationDynamicsValid(defaults) ||
        defaults.initial_moisture_mm != default_initial_moisture_mm ||
        defaults.moisture_min_mm != default_moisture_minimum_mm ||
        defaults.moisture_max_mm != default_moisture_maximum_mm ||
        defaults.terminal_min_mm != default_terminal_minimum_mm ||
        defaults.terminal_max_mm != default_terminal_maximum_mm ||
        defaults.irrigation_max_mm_per_step != default_irrigation_maximum_mm) {
        return false;
    }

    const std::string explanation = ExplainIrrigationDynamics(defaults);

    if (explanation.find("moisture_min_mm=10.000000") ==
            std::string::npos ||
        explanation.find("terminal_max_mm=25.000000") ==
            std::string::npos) {
        return false;
    }

    auto invalid_moisture = defaults;
    invalid_moisture.moisture_min_mm = 31.0;

    if (IsIrrigationDynamicsValid(invalid_moisture)) {
        return false;
    }

    auto invalid_terminal = defaults;
    invalid_terminal.terminal_min_mm = 26.0;
    invalid_terminal.terminal_max_mm = 25.0;

    if (IsIrrigationDynamicsValid(invalid_terminal)) {
        return false;
    }

    auto invalid_limit = defaults;
    invalid_limit.irrigation_max_mm_per_step = -0.01;

    if (IsIrrigationDynamicsValid(invalid_limit)) {
        return false;
    }

    return true;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

constexpr std::size_t default_irrigation_horizon_steps = 3U;
constexpr double rainfall_probability_tolerance = 1e-12;

bool IsRainfallScenarioValid(
    const eco_restoration::RainfallScenario& scenario,
    std::size_t expected_horizon) {
    if (!std::isfinite(scenario.probability) ||
        scenario.probability < 0.0 ||
        scenario.probability > 1.0 ||
        scenario.rainfall_mm.size() != expected_horizon) {
        return false;
    }

    for (const double rainfall : scenario.rainfall_mm) {
        if (!std::isfinite(rainfall) || rainfall < 0.0) {
            return false;
        }
    }

    return true;
}

void ValidateRainfallScenarios(
    const std::vector<eco_restoration::RainfallScenario>& scenarios,
    std::size_t expected_horizon) {
    if (expected_horizon == 0U) {
        throw std::invalid_argument("rainfall scenario horizon must be positive");
    }

    if (scenarios.size() < 2U) {
        throw std::invalid_argument("at least two rainfall scenarios are required");
    }

    double probability_sum = 0.0;
    for (const auto& scenario : scenarios) {
        if (!IsRainfallScenarioValid(scenario, expected_horizon)) {
            throw std::invalid_argument(
                "rainfall scenario probability or horizon values are invalid");
        }
        probability_sum += scenario.probability;
    }

    if (std::abs(probability_sum - 1.0) > rainfall_probability_tolerance) {
        throw std::invalid_argument(
            "rainfall scenario probabilities must sum to one");
    }
}

bool RainfallScenariosAreValid(
    const std::vector<eco_restoration::RainfallScenario>& scenarios,
    std::size_t expected_horizon) {
    try {
        ValidateRainfallScenarios(scenarios, expected_horizon);
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

std::vector<eco_restoration::RainfallScenario> MakeRainfallScenarios(
    std::size_t horizon_steps = default_irrigation_horizon_steps) {
    if (horizon_steps != default_irrigation_horizon_steps) {
        throw std::invalid_argument(
            "default rainfall fixture supports the three-step irrigation horizon");
    }

    const std::vector<eco_restoration::RainfallScenario> scenarios{
        {
            0.60,
            {2.0, 1.5, 3.0}
        },
        {
            0.40,
            {0.5, 0.0, 1.0}
        }
    };

    ValidateRainfallScenarios(scenarios, horizon_steps);
    return scenarios;
}

double TotalRainfallMm(
    const eco_restoration::RainfallScenario& scenario) {
    if (!std::isfinite(scenario.probability) ||
        scenario.probability < 0.0 ||
        scenario.probability > 1.0) {
        throw std::invalid_argument("rainfall scenario probability is invalid");
    }

    double total = 0.0;
    for (const double rainfall : scenario.rainfall_mm) {
        if (!std::isfinite(rainfall) || rainfall < 0.0) {
            throw std::invalid_argument("rainfall values must be finite and nonnegative");
        }
        total += rainfall;
    }
    return total;
}

double ExpectedRainfallMm(
    const std::vector<eco_restoration::RainfallScenario>& scenarios,
    std::size_t expected_horizon) {
    ValidateRainfallScenarios(scenarios, expected_horizon);

    double expected_total = 0.0;
    for (const auto& scenario : scenarios) {
        expected_total += scenario.probability * TotalRainfallMm(scenario);
    }
    return expected_total;
}

std::string ExplainRainfallScenarios(
    const std::vector<eco_restoration::RainfallScenario>& scenarios,
    std::size_t expected_horizon) {
    ValidateRainfallScenarios(scenarios, expected_horizon);

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(6);
    output << "rainfall_scenarios\n";
    output << "scenario_count=" << scenarios.size() << '\n';
    output << "horizon_steps=" << expected_horizon << '\n';

    for (std::size_t index = 0U; index < scenarios.size(); ++index) {
        const auto& scenario = scenarios[index];
        output << "scenario_" << index << "_probability="
               << scenario.probability << '\n';
        output << "scenario_" << index << "_total_rainfall_mm="
               << TotalRainfallMm(scenario) << '\n';
    }

    output << "expected_rainfall_mm="
           << ExpectedRainfallMm(scenarios, expected_horizon) << '\n';
    return output.str();
}

bool RainfallScenarioFixtureSelfTest() {
    const auto scenarios = MakeRainfallScenarios();

    if (!RainfallScenariosAreValid(
            scenarios,
            default_irrigation_horizon_steps) ||
        scenarios.size() != 2U ||
        std::abs(scenarios[0].probability + scenarios[1].probability - 1.0) >
            rainfall_probability_tolerance ||
        std::abs(TotalRainfallMm(scenarios[0]) - 6.5) > 1e-12 ||
        std::abs(TotalRainfallMm(scenarios[1]) - 1.5) > 1e-12 ||
        std::abs(ExpectedRainfallMm(
                     scenarios,
                     default_irrigation_horizon_steps) - 4.5) > 1e-12) {
        return false;
    }

    const std::string explanation = ExplainRainfallScenarios(
        scenarios,
        default_irrigation_horizon_steps);

    if (explanation.find("scenario_count=2") == std::string::npos ||
        explanation.find("horizon_steps=3") == std::string::npos ||
        explanation.find("expected_rainfall_mm=4.500000") ==
            std::string::npos) {
        return false;
    }

    auto invalid_sum = scenarios;
    invalid_sum[1].probability = 0.30;

    if (RainfallScenariosAreValid(
            invalid_sum,
            default_irrigation_horizon_steps)) {
        return false;
    }

    auto invalid_length = scenarios;
    invalid_length[0].rainfall_mm.pop_back();

    if (RainfallScenariosAreValid(
            invalid_length,
            default_irrigation_horizon_steps)) {
        return false;
    }

    return true;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

using IrrigationSchedule = std::vector<double>;
using IrrigationScheduleSet = std::vector<IrrigationSchedule>;

bool IsIrrigationScheduleValid(
    const IrrigationSchedule& schedule,
    std::size_t expected_horizon,
    const eco_restoration::IrrigationDynamics& dynamics) {
    if (schedule.size() != expected_horizon) return false;

    for (const double irrigation : schedule) {
        if (!std::isfinite(irrigation) ||
            irrigation < 0.0 ||
            irrigation > dynamics.irrigation_max_mm_per_step) {
            return false;
        }
    }

    return true;
}

void ValidateIrrigationCandidateSchedules(
    const IrrigationScheduleSet& schedules,
    std::size_t expected_horizon,
    const eco_restoration::IrrigationDynamics& dynamics) {
    ValidateIrrigationDynamics(dynamics);

    if (expected_horizon == 0U) {
        throw std::invalid_argument("irrigation schedule horizon must be positive");
    }

    if (schedules.size() < 3U) {
        throw std::invalid_argument(
            "at least three irrigation candidate schedules are required");
    }

    for (const auto& schedule : schedules) {
        if (!IsIrrigationScheduleValid(
                schedule,
                expected_horizon,
                dynamics)) {
            throw std::invalid_argument(
                "irrigation candidate has invalid horizon or water limit");
        }
    }
}

double TotalIrrigationMm(
    const IrrigationSchedule& schedule) {
    double total = 0.0;
    for (const double irrigation : schedule) {
        if (!std::isfinite(irrigation) || irrigation < 0.0) {
            throw std::invalid_argument(
                "irrigation schedule values must be finite and nonnegative");
        }
        total += irrigation;
    }
    return total;
}

bool HasConservativeIrrigationSchedule(
    const IrrigationScheduleSet& schedules) {
    if (schedules.empty()) return false;

    double minimum_total = std::numeric_limits<double>::infinity();
    double maximum_total = 0.0;

    for (const auto& schedule : schedules) {
        const double total = TotalIrrigationMm(schedule);
        minimum_total = std::min(minimum_total, total);
        maximum_total = std::max(maximum_total, total);
    }

    return minimum_total < maximum_total;
}

IrrigationScheduleSet MakeIrrigationCandidateSchedules(
    const eco_restoration::IrrigationDynamics& dynamics,
    std::size_t horizon_steps = default_irrigation_horizon_steps) {
    ValidateIrrigationDynamics(dynamics);

    if (horizon_steps != default_irrigation_horizon_steps) {
        throw std::invalid_argument(
            "default irrigation fixture supports the three-step horizon");
    }

    const IrrigationScheduleSet schedules{
        {4.0, 3.0, 4.0},
        {5.0, 4.0, 5.0},
        {3.0, 2.0, 3.0}
    };

    ValidateIrrigationCandidateSchedules(
        schedules,
        horizon_steps,
        dynamics);

    if (!HasConservativeIrrigationSchedule(schedules)) {
        throw std::runtime_error(
            "irrigation fixture must contain a conservative schedule");
    }

    return schedules;
}

std::string ExplainIrrigationCandidateSchedules(
    const IrrigationScheduleSet& schedules,
    std::size_t expected_horizon,
    const eco_restoration::IrrigationDynamics& dynamics) {
    ValidateIrrigationCandidateSchedules(
        schedules,
        expected_horizon,
        dynamics);

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(6);
    output << "irrigation_candidate_schedules\n";
    output << "schedule_count=" << schedules.size() << '\n';
    output << "horizon_steps=" << expected_horizon << '\n';

    for (std::size_t index = 0U; index < schedules.size(); ++index) {
        output << "schedule_" << index << "_total_irrigation_mm="
               << TotalIrrigationMm(schedules[index]) << '\n';
    }

    output << "has_conservative_schedule="
           << (HasConservativeIrrigationSchedule(schedules)
               ? "true" : "false")
           << '\n';
    return output.str();
}

bool IrrigationCandidateScheduleFixtureSelfTest() {
    const auto dynamics = MakeIrrigationDynamics();
    const auto schedules = MakeIrrigationCandidateSchedules(dynamics);
    const auto rainfall = MakeRainfallScenarios();

    if (schedules.size() != 3U ||
        !HasConservativeIrrigationSchedule(schedules) ||
        std::abs(TotalIrrigationMm(schedules[0]) - 11.0) > 1e-12 ||
        std::abs(TotalIrrigationMm(schedules[1]) - 14.0) > 1e-12 ||
        std::abs(TotalIrrigationMm(schedules[2]) - 8.0) > 1e-12) {
        return false;
    }

    const auto result =
        eco_restoration::select_robust_irrigation_schedule(
            schedules,
            rainfall,
            dynamics);

    if (!result.robustly_feasible) {
        return false;
    }

    const std::string explanation =
        ExplainIrrigationCandidateSchedules(
            schedules,
            default_irrigation_horizon_steps,
            dynamics);

    if (explanation.find("schedule_count=3") == std::string::npos ||
        explanation.find("schedule_2_total_irrigation_mm=8.000000") ==
            std::string::npos ||
        explanation.find("has_conservative_schedule=true") ==
            std::string::npos) {
        return false;
    }

    auto invalid_limit = schedules;
    invalid_limit[0][0] =
        dynamics.irrigation_max_mm_per_step + 0.01;

    try {
        ValidateIrrigationCandidateSchedules(
            invalid_limit,
            default_irrigation_horizon_steps,
            dynamics);
        return false;
    } catch (const std::invalid_argument&) {
    }

    auto invalid_horizon = schedules;
    invalid_horizon[1].pop_back();

    try {
        ValidateIrrigationCandidateSchedules(
            invalid_horizon,
            default_irrigation_horizon_steps,
            dynamics);
        return false;
    } catch (const std::invalid_argument&) {
    }

    return true;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

bool IsRobustIrrigationResultValid(
    const eco_restoration::IrrigationMpcResult& result) {
    if (!std::isfinite(result.expected_cost) ||
        !std::isfinite(result.worst_case_terminal_moisture_mm) ||
        !IsUnitIntervalScore(result.knowledge_factor) ||
        !IsUnitIntervalScore(result.eco_impact_value)) {
        return false;
    }

    if (result.robustly_feasible && result.schedule_mm.empty()) {
        return false;
    }

    for (const double irrigation : result.schedule_mm) {
        if (!std::isfinite(irrigation) || irrigation < 0.0) {
            return false;
        }
    }

    return true;
}

double MaximumScheduleIrrigationMm(
    const eco_restoration::IrrigationMpcResult& result) {
    if (!IsRobustIrrigationResultValid(result) ||
        result.schedule_mm.empty()) {
        throw std::invalid_argument(
            "irrigation result must contain a valid selected schedule");
    }

    return *std::max_element(
        result.schedule_mm.begin(),
        result.schedule_mm.end());
}

std::string IrrigationResultStatus(
    const eco_restoration::IrrigationMpcResult& result) {
    if (!IsRobustIrrigationResultValid(result)) {
        return "invalid";
    }

    if (result.robustly_feasible) {
        return "robustly_feasible";
    }

    if (result.schedule_mm.empty()) {
        return "no_feasible_schedule";
    }

    return "candidate_not_robust";
}

std::string ExplainIrrigationResult(
    const eco_restoration::IrrigationMpcResult& result) {
    if (!IsRobustIrrigationResultValid(result)) {
        throw std::invalid_argument("irrigation result is invalid");
    }

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(6);
    output << "robust_irrigation_result\n";
    output << "status=" << IrrigationResultStatus(result) << '\n';
    output << "robustly_feasible="
           << (result.robustly_feasible ? "true" : "false") << '\n';
    output << "selected_schedule_steps="
           << result.schedule_mm.size() << '\n';

    for (std::size_t index = 0U;
         index < result.schedule_mm.size();
         ++index) {
        output << "selected_schedule_step_" << index << "_mm="
               << result.schedule_mm[index] << '\n';
    }

    if (!result.schedule_mm.empty()) {
        output << "selected_schedule_total_mm="
               << TotalIrrigationMm(result.schedule_mm) << '\n';
        output << "selected_schedule_peak_mm="
               << MaximumScheduleIrrigationMm(result) << '\n';
    }

    output << "expected_cost=" << result.expected_cost << '\n';
    output << "worst_terminal_moisture_mm="
           << result.worst_case_terminal_moisture_mm << '\n';
    output << "knowledge_factor="
           << result.knowledge_factor << '\n';
    output << "eco_impact_value="
           << result.eco_impact_value << '\n';

    return output.str();
}

bool IrrigationResultMatchesHorizon(
    const eco_restoration::IrrigationMpcResult& result,
    std::size_t expected_horizon) {
    return IsRobustIrrigationResultValid(result) &&
           result.schedule_mm.size() == expected_horizon;
}

bool IrrigationResultExplainerSelfTest() {
    const auto dynamics = MakeIrrigationDynamics();
    const auto rainfall = MakeRainfallScenarios();
    const auto candidates = MakeIrrigationCandidateSchedules(dynamics);

    const auto result =
        eco_restoration::select_robust_irrigation_schedule(
            candidates,
            rainfall,
            dynamics);

    if (!result.robustly_feasible ||
        !IrrigationResultMatchesHorizon(
            result,
            default_irrigation_horizon_steps) ||
        std::abs(TotalIrrigationMm(result.schedule_mm) - 8.0) > 1e-12 ||
        std::abs(MaximumScheduleIrrigationMm(result) - 3.0) > 1e-12) {
        return false;
    }

    const std::string explanation =
        ExplainIrrigationResult(result);

    if (explanation.find("status=robustly_feasible") ==
            std::string::npos ||
        explanation.find("robustly_feasible=true") ==
            std::string::npos ||
        explanation.find("selected_schedule_steps=3") ==
            std::string::npos ||
        explanation.find("selected_schedule_total_mm=8.000000") ==
            std::string::npos ||
        explanation.find("worst_terminal_moisture_mm=") ==
            std::string::npos) {
        return false;
    }

    eco_restoration::IrrigationMpcResult invalid = result;
    invalid.expected_cost =
        std::numeric_limits<double>::infinity();

    if (IsRobustIrrigationResultValid(invalid)) {
        return false;
    }

    try {
        static_cast<void>(ExplainIrrigationResult(invalid));
        return false;
    } catch (const std::invalid_argument&) {
    }

    return true;
}

}  // namespace prometheus_praxis_foundation_extensions

// File: cpp/tools/prometheus_praxis_foundation_main.cpp
namespace prometheus_praxis_foundation_extensions {

constexpr double default_available_water_mm = 90.0;
constexpr double default_equity_tolerance = 0.01;

double AgriculturalWaterUtility(double allocation_mm) {
    return allocation_mm < 0.0 ? -std::numeric_limits<double>::infinity()
                               : std::sqrt(allocation_mm);
}

double MunicipalWaterUtility(double allocation_mm) {
    return allocation_mm < 0.0 ? -std::numeric_limits<double>::infinity()
                               : std::sqrt(allocation_mm);
}

double EcologicalWaterUtility(double allocation_mm) {
    return allocation_mm < 0.0 ? -std::numeric_limits<double>::infinity()
                               : std::sqrt(allocation_mm);
}

std::vector<eco_restoration::WaterStakeholder>
MakeEquitableWaterStakeholders() {
    const std::vector<eco_restoration::WaterStakeholder> stakeholders{
        {10.0, 40.0, AgriculturalWaterUtility},
        {10.0, 40.0, MunicipalWaterUtility},
        {10.0, 40.0, EcologicalWaterUtility}
    };

    for (const auto& stakeholder : stakeholders) {
        if (!stakeholder.utility ||
            stakeholder.minimum_allocation_mm < 0.0 ||
            stakeholder.maximum_allocation_mm <
                stakeholder.minimum_allocation_mm) {
            throw std::runtime_error("equitable water stakeholder fixture is invalid");
        }
    }

    return stakeholders;
}

std::vector<double> MakeEquitableWaterAllocation() {
    return {30.0, 30.0, 30.0};
}

struct EquitableWaterAllocationEvaluation {
    eco_restoration::EquitableAllocationResult result;
    bool evaluated{};
    bool inequality_within_tolerance{};
    std::vector<std::string> failure_reasons;
};

double UtilityInequalityGap(
    const std::vector<eco_restoration::WaterStakeholder>& stakeholders,
    const std::vector<double>& allocation_mm) {
    if (stakeholders.size() != allocation_mm.size() ||
        stakeholders.empty()) {
        throw std::invalid_argument(
            "stakeholder and allocation vectors must have equal nonzero length");
    }

    double minimum_utility = std::numeric_limits<double>::infinity();
    double maximum_utility = -std::numeric_limits<double>::infinity();

    for (std::size_t index = 0U; index < stakeholders.size(); ++index) {
        const double utility = stakeholders[index].utility(allocation_mm[index]);
        if (!std::isfinite(utility)) {
            throw std::invalid_argument("stakeholder utility must be finite");
        }
        minimum_utility = std::min(minimum_utility, utility);
        maximum_utility = std::max(maximum_utility, utility);
    }

    return maximum_utility - minimum_utility;
}

EquitableWaterAllocationEvaluation EvaluateEquitableWaterAllocation(
    const std::vector<double>& allocation_mm = MakeEquitableWaterAllocation(),
    double available_water_mm = default_available_water_mm,
    double equity_tolerance = default_equity_tolerance) {
    EquitableWaterAllocationEvaluation evaluation;
    const auto stakeholders = MakeEquitableWaterStakeholders();

    try {
        evaluation.result = eco_restoration::evaluate_water_allocation(
            allocation_mm,
            stakeholders,
            available_water_mm,
            equity_tolerance);
        evaluation.evaluated = true;

        const double gap = UtilityInequalityGap(
            stakeholders,
            allocation_mm);

        evaluation.inequality_within_tolerance =
            gap <= equity_tolerance;

        if (!evaluation.result.equitable) {
            evaluation.failure_reasons.emplace_back(
                "water allocation did not satisfy the equity constraint");
        }

        if (!evaluation.inequality_within_tolerance) {
            evaluation.failure_reasons.emplace_back(
                "utility inequality gap exceeded the configured tolerance");
        }

        if (evaluation.result.utility_gap > equity_tolerance) {
            evaluation.failure_reasons.emplace_back(
                "reported utility gap exceeded the configured tolerance");
        }
    } catch (const std::exception& error) {
        evaluation.failure_reasons.emplace_back(error.what());
    }

    return evaluation;
}

bool IsEquitableWaterAllocationEvaluationValid(
    const EquitableWaterAllocationEvaluation& evaluation) {
    if (!evaluation.evaluated) {
        return !evaluation.failure_reasons.empty();
    }

    if (evaluation.result.equitable) {
        return evaluation.inequality_within_tolerance &&
               evaluation.failure_reasons.empty();
    }

    return !evaluation.failure_reasons.empty();
}

std::string ExplainEquitableWaterAllocation(
    const EquitableWaterAllocationEvaluation& evaluation) {
    if (!IsEquitableWaterAllocationEvaluationValid(evaluation)) {
        throw std::invalid_argument(
            "equitable water allocation evaluation is invalid");
    }

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(6);
    output << "equitable_water_allocation\n";
    output << "evaluated="
           << (evaluation.evaluated ? "true" : "false") << '\n';
    output << "equitable="
           << (evaluation.result.equitable ? "true" : "false") << '\n';
    output << "inequality_within_tolerance="
           << (evaluation.inequality_within_tolerance ? "true" : "false")
           << '\n';
    output << "total_utility="
           << evaluation.result.total_utility << '\n';
    output << "utility_gap="
           << evaluation.result.utility_gap << '\n';
    output << "failure_reason_count="
           << evaluation.failure_reasons.size() << '\n';

    for (std::size_t index = 0U;
         index < evaluation.failure_reasons.size();
         ++index) {
        output << "failure_reason_" << index << '='
               << evaluation.failure_reasons[index] << '\n';
    }

    return output.str();
}

bool EquitableWaterAllocationEvaluatorSelfTest() {
    const auto accepted = EvaluateEquitableWaterAllocation();

    if (!accepted.evaluated ||
        !accepted.result.equitable ||
        !accepted.inequality_within_tolerance ||
        !accepted.failure_reasons.empty() ||
        !IsEquitableWaterAllocationEvaluationValid(accepted) ||
        std::abs(accepted.result.utility_gap) > 1e-12) {
        return false;
    }

    const auto unequal = EvaluateEquitableWaterAllocation(
        {40.0, 30.0, 20.0});

    if (!unequal.evaluated ||
        unequal.result.equitable ||
        unequal.inequality_within_tolerance ||
        unequal.failure_reasons.empty()) {
        return false;
    }

    const std::string explanation =
        ExplainEquitableWaterAllocation(unequal);

    if (explanation.find("equitable=false") == std::string::npos ||
        explanation.find("inequality_within_tolerance=false") ==
            std::string::npos ||
        explanation.find("utility_gap=") == std::string::npos) {
        return false;
    }

    const auto invalid = EvaluateEquitableWaterAllocation(
        {30.0, 30.0},
        default_available_water_mm,
        default_equity_tolerance);

    return !invalid.evaluated &&
           !invalid.failure_reasons.empty() &&
           IsEquitableWaterAllocationEvaluationValid(invalid);
}

}  // namespace prometheus_praxis_foundation_extensions
