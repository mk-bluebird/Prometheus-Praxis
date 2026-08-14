// File: cpp/tools/foundation_extension_registry.hpp
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct CanonicalExtensionDescriptor {
    std::string name;
    bool (*self_test)();
    std::string_view purpose;
    bool diagnostics_only;
};

struct CanonicalExtensionRunResult {
    std::string name;
    bool passed;
    std::string detail;
};

class CanonicalExtensionRegistry {
public:
    bool Register(CanonicalExtensionDescriptor descriptor);

    const std::vector<CanonicalExtensionDescriptor>& Descriptors() const noexcept;

    std::optional<std::string> ValidationErrorFor(
        const CanonicalExtensionDescriptor& descriptor) const;

private:
    std::vector<CanonicalExtensionDescriptor> descriptors_;
};

std::vector<CanonicalExtensionRunResult> RunCanonicalExtensionSelfTests(
    const CanonicalExtensionRegistry& registry);

CanonicalExtensionRegistry BuildKnownExtensionRegistry();

std::string ExplainCanonicalExtensionRun(
    const std::vector<CanonicalExtensionRunResult>& results);

bool AllCanonicalExtensionSelfTestsPassed(
    const std::vector<CanonicalExtensionRunResult>& results);

bool CanonicalExtensionRegistrySelfTest();
