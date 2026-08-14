// File: cpp/tools/foundation_extension_registry.cpp
#include "foundation_extension_registry.hpp"

#include "foundation_report.hpp"

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

bool IsLowerSnakeCase(const std::string_view name) noexcept {
    if (name.empty() || name.front() == '_' || name.back() == '_') {
        return false;
    }

    bool previous_was_underscore = false;
    for (const char character : name) {
        const bool lower_case = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';

        if (character == '_') {
            if (previous_was_underscore) {
                return false;
            }
            previous_was_underscore = true;
            continue;
        }

        if (!lower_case && !digit) {
            return false;
        }
        previous_was_underscore = false;
    }

    return true;
}

bool HasName(const std::vector<CanonicalExtensionDescriptor>& descriptors,
             const std::string_view name) {
    return std::any_of(
        descriptors.begin(),
        descriptors.end(),
        [name](const CanonicalExtensionDescriptor& descriptor) {
            return descriptor.name == name;
        });
}

bool AlwaysPasses() {
    return true;
}

bool AlwaysFails() {
    return false;
}

std::string ExecutionDetail(const CanonicalExtensionDescriptor& descriptor,
                            const bool passed) {
    std::ostringstream output;
    output << (passed ? "passed" : "failed")
           << "; diagnostics_only="
           << (descriptor.diagnostics_only ? "true" : "false")
           << "; purpose=" << descriptor.purpose;
    return output.str();
}

}  // namespace

bool CanonicalExtensionRegistry::Register(
    CanonicalExtensionDescriptor descriptor) {
    if (ValidationErrorFor(descriptor).has_value()) {
        return false;
    }

    descriptors_.push_back(std::move(descriptor));
    return true;
}

const std::vector<CanonicalExtensionDescriptor>&
CanonicalExtensionRegistry::Descriptors() const noexcept {
    return descriptors_;
}

std::optional<std::string> CanonicalExtensionRegistry::ValidationErrorFor(
    const CanonicalExtensionDescriptor& descriptor) const {
    if (!IsLowerSnakeCase(descriptor.name)) {
        return std::string(
            "extension name must be non-empty lower_snake_case");
    }

    if (HasName(descriptors_, descriptor.name)) {
        return std::string("extension name must be unique");
    }

    if (descriptor.self_test == nullptr) {
        return std::string("extension self_test callback must not be null");
    }

    if (descriptor.purpose.empty()) {
        return std::string("extension purpose must not be empty");
    }

    return std::nullopt;
}

std::vector<CanonicalExtensionRunResult> RunCanonicalExtensionSelfTests(
    const CanonicalExtensionRegistry& registry) {
    std::vector<CanonicalExtensionRunResult> results;
    results.reserve(registry.Descriptors().size());

    for (const CanonicalExtensionDescriptor& descriptor :
         registry.Descriptors()) {
        bool passed = false;
        std::string detail;

        try {
            passed = descriptor.self_test();
            detail = ExecutionDetail(descriptor, passed);
        } catch (...) {
            passed = false;
            detail = "failed; self_test raised an exception";
        }

        results.push_back(
            CanonicalExtensionRunResult{descriptor.name, passed, std::move(detail)});
    }

    return results;
}

CanonicalExtensionRegistry BuildKnownExtensionRegistry() {
    CanonicalExtensionRegistry registry;

    const bool registered = registry.Register(
        CanonicalExtensionDescriptor{
            "foundation_report_validator",
            &FoundationReportValidatorSelfTest,
            "Validates ecological foundation reports and compares diagnostics.",
            true});

    if (!registered) {
        return CanonicalExtensionRegistry{};
    }

    return registry;
}

std::string ExplainCanonicalExtensionRun(
    const std::vector<CanonicalExtensionRunResult>& results) {
    std::ostringstream output;

    for (std::size_t index = 0U; index < results.size(); ++index) {
        const CanonicalExtensionRunResult& result = results[index];
        output << result.name << '=' << (result.passed ? "1" : "0")
               << "; " << result.detail;
        if (index + 1U < results.size()) {
            output << '\n';
        }
    }

    return output.str();
}

bool AllCanonicalExtensionSelfTestsPassed(
    const std::vector<CanonicalExtensionRunResult>& results) {
    return std::all_of(
        results.begin(),
        results.end(),
        [](const CanonicalExtensionRunResult& result) {
            return result.passed;
        });
}

bool CanonicalExtensionRegistrySelfTest() {
    CanonicalExtensionRegistry registry;

    if (!registry.Register(
            CanonicalExtensionDescriptor{
                "valid_extension",
                &AlwaysPasses,
                "Runs a deterministic diagnostic self-test.",
                true})) {
        return false;
    }

    if (registry.Register(
            CanonicalExtensionDescriptor{
                "valid_extension",
                &AlwaysPasses,
                "Duplicate names are rejected.",
                true})) {
        return false;
    }

    if (registry.Register(
            CanonicalExtensionDescriptor{
                "Invalid_Name",
                &AlwaysPasses,
                "Uppercase names are rejected.",
                true})) {
        return false;
    }

    if (registry.Register(
            CanonicalExtensionDescriptor{
                "double__underscore",
                &AlwaysPasses,
                "Repeated separators are rejected.",
                true})) {
        return false;
    }

    if (registry.Register(
            CanonicalExtensionDescriptor{
                "missing_callback",
                nullptr,
                "Null callbacks are rejected.",
                true})) {
        return false;
    }

    if (registry.Register(
            CanonicalExtensionDescriptor{
                "empty_purpose",
                &AlwaysPasses,
                "",
                true})) {
        return false;
    }

    if (!registry.Register(
            CanonicalExtensionDescriptor{
                "failing_extension",
                &AlwaysFails,
                "Produces a controlled failing diagnostic result.",
                true})) {
        return false;
    }

    const std::vector<CanonicalExtensionRunResult> results =
        RunCanonicalExtensionSelfTests(registry);

    if (results.size() != 2U ||
        results[0].name != "valid_extension" ||
        !results[0].passed ||
        results[1].name != "failing_extension" ||
        results[1].passed) {
        return false;
    }

    if (AllCanonicalExtensionSelfTestsPassed(results)) {
        return false;
    }

    const std::string explanation = ExplainCanonicalExtensionRun(results);
    const std::string expected =
        "valid_extension=1; passed; diagnostics_only=true; purpose=Runs a "
        "deterministic diagnostic self-test.\n"
        "failing_extension=0; failed; diagnostics_only=true; purpose=Produces "
        "a controlled failing diagnostic result.";

    if (explanation != expected) {
        return false;
    }

    const CanonicalExtensionRegistry known = BuildKnownExtensionRegistry();
    const std::vector<CanonicalExtensionRunResult> known_results =
        RunCanonicalExtensionSelfTests(known);

    return known_results.size() == 1U &&
           known_results.front().name == "foundation_report_validator" &&
           known_results.front().passed &&
           AllCanonicalExtensionSelfTestsPassed(known_results);
}
