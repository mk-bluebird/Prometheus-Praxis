// File: cpp/tools/foundation_extension_registry.cpp
#include "foundation_extension_registry.hpp"

#include "foundation_golden_tests.hpp"
#include "foundation_legacy_contract_inventory.hpp"
#include "foundation_report.hpp"
#include "foundation_report_json.hpp"
#include "foundation_safety_semantics.hpp"

#include <algorithm>
#include <exception>
#include <sstream>
#include <string_view>
#include <utility>

namespace prometheus_praxis::foundation {
namespace {

bool IsLowerSnakeCase(std::string_view name) noexcept {
    if (name.empty() || name.front() == '_' || name.back() == '_') {
        return false;
    }

    bool previous_underscore = false;
    for (const unsigned char character : name) {
        const bool lowercase = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';
        const bool underscore = character == '_';

        if (!lowercase && !digit && !underscore) {
            return false;
        }
        if (underscore && previous_underscore) {
            return false;
        }
        previous_underscore = underscore;
    }
    return true;
}

bool ContainsName(
    const std::vector<CanonicalExtensionDescriptor>& descriptors,
    std::string_view name) {
    return std::any_of(
        descriptors.begin(),
        descriptors.end(),
        [name](const CanonicalExtensionDescriptor& descriptor) {
            return descriptor.name == name;
        });
}

std::string CategoryFor(
    const CanonicalExtensionDescriptor& descriptor) {
    return descriptor.diagnostics_only ? "diagnostics" : "core";
}

CanonicalExtensionRunResult RunDescriptor(
    const CanonicalExtensionDescriptor& descriptor,
    std::size_t execution_order) {
    try {
        const bool passed = descriptor.self_test();
        return CanonicalExtensionRunResult{
            descriptor.name,
            passed,
            passed ? "self_test=passed" : "self_test=failed",
            execution_order,
            CategoryFor(descriptor)};
    } catch (const std::exception& exception) {
        return CanonicalExtensionRunResult{
            descriptor.name,
            false,
            std::string("self_test=exception; message=") + exception.what(),
            execution_order,
            CategoryFor(descriptor)};
    } catch (...) {
        return CanonicalExtensionRunResult{
            descriptor.name,
            false,
            "self_test=exception; message=non_standard_exception",
            execution_order,
            CategoryFor(descriptor)};
    }
}

bool PassingTest() {
    return true;
}

bool FailingTest() {
    return false;
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

std::size_t CanonicalExtensionRegistry::Size() const noexcept {
    return descriptors_.size();
}

std::optional<std::string> CanonicalExtensionRegistry::ValidationErrorFor(
    const CanonicalExtensionDescriptor& descriptor) const {
    if (!IsLowerSnakeCase(descriptor.name)) {
        return "extension name must be non-empty lower_snake_case";
    }
    if (descriptor.self_test == nullptr) {
        return "extension self_test callback must not be null";
    }
    if (descriptor.purpose.empty()) {
        return "extension purpose must not be empty";
    }
    if (ContainsName(descriptors_, descriptor.name)) {
        return "extension name must be unique";
    }
    return std::nullopt;
}

std::vector<CanonicalExtensionRunResult> RunCanonicalExtensionSelfTests(
    const CanonicalExtensionRegistry& registry) {
    std::vector<CanonicalExtensionRunResult> results;
    results.reserve(registry.Size());

    std::size_t execution_order = 0U;
    for (const CanonicalExtensionDescriptor& descriptor :
         registry.Descriptors()) {
        results.push_back(RunDescriptor(descriptor, execution_order));
        ++execution_order;
    }
    return results;
}

CanonicalExtensionRegistry BuildKnownExtensionRegistry() {
    CanonicalExtensionRegistry registry;

    const std::vector<CanonicalExtensionDescriptor> descriptors{
        {
            "foundation_legacy_contract_inventory",
            &legacy_contract::FoundationLegacyContractInventorySelfTest,
            "Validates source-backed legacy command fixtures and contract metadata.",
            true,
        },
        {
            "foundation_golden_tests",
            &golden::FoundationGoldenTestsSelfTest,
            "Validates exact command-line golden expectation comparisons.",
            true,
        },
        {
            "foundation_report_validator",
            &FoundationReportValidatorSelfTest,
            "Validates foundation report safety, metrics, and comparisons.",
            true,
        },
        {
            "foundation_report_json",
            &json::FoundationReportJsonSerializerSelfTest,
            "Validates deterministic foundation report JSON serialization.",
            true,
        },
        {
            "foundation_safety_semantics",
            &safety::FoundationSafetySemanticsSelfTest,
            "Validates legacy containment semantic conversion and audit records.",
            true,
        },
    };

    for (const CanonicalExtensionDescriptor& descriptor : descriptors) {
        if (!registry.Register(descriptor)) {
            return CanonicalExtensionRegistry{};
        }
    }
    return registry;
}

std::string ExplainCanonicalExtensionRun(
    const std::vector<CanonicalExtensionRunResult>& results) {
    std::ostringstream output;
    output << "canonical_extension_run; count=" << results.size();

    for (const CanonicalExtensionRunResult& result : results) {
        output << "; order=" << result.execution_order
               << "; name=" << result.name
               << "; category=" << result.category
               << "; passed=" << (result.passed ? "true" : "false")
               << "; detail=" << result.detail;
    }
    return output.str();
}

bool AllCanonicalExtensionSelfTestsPassed(
    const std::vector<CanonicalExtensionRunResult>& results) {
    return !results.empty() &&
           std::all_of(
               results.begin(),
               results.end(),
               [](const CanonicalExtensionRunResult& result) {
                   return result.passed;
               });
}

std::vector<CanonicalExtensionRunResult> RunCanonicalExtensionSubset(
    const CanonicalExtensionRegistry& registry,
    const std::vector<std::string>& names) {
    std::vector<CanonicalExtensionRunResult> results;
    results.reserve(names.size());

    std::size_t execution_order = 0U;
    for (const CanonicalExtensionDescriptor& descriptor :
         registry.Descriptors()) {
        if (std::find(names.begin(), names.end(), descriptor.name) !=
            names.end()) {
            results.push_back(RunDescriptor(descriptor, execution_order));
            ++execution_order;
        }
    }
    return results;
}

bool CanonicalExtensionRegistrySelfTest() {
    CanonicalExtensionRegistry registry;
    const CanonicalExtensionDescriptor valid{
        "first_extension",
        &PassingTest,
        "Verifies a passing diagnostic callback.",
        true,
    };

    if (!registry.Register(valid) || registry.Size() != 1U ||
        registry.Register(valid)) {
        return false;
    }

    const CanonicalExtensionDescriptor invalid_name{
        "Invalid-Name",
        &PassingTest,
        "Reject invalid descriptor names.",
        true,
    };
    const CanonicalExtensionDescriptor null_callback{
        "null_callback",
        nullptr,
        "Reject null callbacks.",
        true,
    };
    const CanonicalExtensionDescriptor empty_purpose{
        "empty_purpose",
        &PassingTest,
        "",
        true,
    };

    if (!registry.ValidationErrorFor(invalid_name).has_value() ||
        !registry.ValidationErrorFor(null_callback).has_value() ||
        !registry.ValidationErrorFor(empty_purpose).has_value() ||
        registry.Register(invalid_name) ||
        registry.Register(null_callback) ||
        registry.Register(empty_purpose)) {
        return false;
    }

    if (!registry.Register({
            "second_extension",
            &FailingTest,
            "Verifies failed callbacks do not prevent later execution.",
            true,
        }) ||
        !registry.Register({
            "third_extension",
            &PassingTest,
            "Verifies insertion-order execution after failure.",
            false,
        })) {
        return false;
    }

    const std::vector<CanonicalExtensionRunResult> all_results =
        RunCanonicalExtensionSelfTests(registry);
    if (all_results.size() != 3U ||
        !all_results[0].passed ||
        all_results[1].passed ||
        !all_results[2].passed ||
        all_results[0].execution_order != 0U ||
        all_results[1].execution_order != 1U ||
        all_results[2].execution_order != 2U ||
        all_results[2].category != "core" ||
        AllCanonicalExtensionSelfTestsPassed(all_results)) {
        return false;
    }

    const std::vector<CanonicalExtensionRunResult> subset =
        RunCanonicalExtensionSubset(
            registry,
            {"third_extension", "second_extension", "missing_extension"});
    if (subset.size() != 2U ||
        subset[0].name != "second_extension" ||
        subset[1].name != "third_extension" ||
        subset[0].execution_order != 0U ||
        subset[1].execution_order != 1U) {
        return false;
    }

    const std::string explanation = ExplainCanonicalExtensionRun(all_results);
    return explanation ==
           "canonical_extension_run; count=3; order=0; name=first_extension; "
           "category=diagnostics; passed=true; detail=self_test=passed; "
           "order=1; name=second_extension; category=diagnostics; "
           "passed=false; detail=self_test=failed; order=2; "
           "name=third_extension; category=core; passed=true; "
           "detail=self_test=passed";
}

}  // namespace prometheus_praxis::foundation
