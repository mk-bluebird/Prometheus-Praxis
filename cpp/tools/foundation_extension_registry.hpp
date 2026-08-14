// File: cpp/tools/foundation_extension_registry.hpp
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace prometheus_praxis::foundation {

struct CanonicalExtensionDescriptor {
    std::string name;
    bool (*self_test)() noexcept{nullptr};
    std::string purpose;
    bool diagnostics_only{true};
};

struct CanonicalExtensionRunResult {
    std::string name;
    bool passed{};
    std::string detail;
    std::size_t execution_order{};
    std::string category;
};

class CanonicalExtensionRegistry {
public:
    bool Register(CanonicalExtensionDescriptor descriptor);

    [[nodiscard]] const std::vector<CanonicalExtensionDescriptor>&
    Descriptors() const noexcept;

    [[nodiscard]] std::size_t Size() const noexcept;

    [[nodiscard]] bool Empty() const noexcept;

    [[nodiscard]] std::optional<std::string> ValidationErrorFor(
        const CanonicalExtensionDescriptor& descriptor) const;

    [[nodiscard]] std::optional<CanonicalExtensionDescriptor> Find(
        std::string_view name) const;

private:
    std::vector<CanonicalExtensionDescriptor> descriptors_;
};

[[nodiscard]] std::vector<CanonicalExtensionRunResult>
RunCanonicalExtensionSelfTests(
    const CanonicalExtensionRegistry& registry);

[[nodiscard]] CanonicalExtensionRegistry BuildKnownExtensionRegistry();

[[nodiscard]] std::string ExplainCanonicalExtensionRun(
    const std::vector<CanonicalExtensionRunResult>& results);

[[nodiscard]] bool AllCanonicalExtensionSelfTestsPassed(
    const std::vector<CanonicalExtensionRunResult>& results);

[[nodiscard]] std::vector<CanonicalExtensionRunResult>
RunCanonicalExtensionSubset(
    const CanonicalExtensionRegistry& registry,
    const std::vector<std::string>& names);

[[nodiscard]] bool CanonicalExtensionRegistrySelfTest() noexcept;

}  // namespace prometheus_praxis::foundation
