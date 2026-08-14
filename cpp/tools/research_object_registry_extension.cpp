// File: cpp/tools/research_object_registry_extension.cpp
#include "research_object_registry_extension.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace {

bool IsIdentifierSafeTitle(const std::string& title) noexcept {
    if (title.empty() || title.front() == '_' || title.back() == '_') {
        return false;
    }

    bool previous_underscore = false;
    for (const char character : title) {
        const bool alphabetic =
            (character >= 'A' && character <= 'Z') ||
            (character >= 'a' && character <= 'z');
        const bool digit = character >= '0' && character <= '9';

        if (character == '_') {
            if (previous_underscore) {
                return false;
            }
            previous_underscore = true;
            continue;
        }

        if (!alphabetic && !digit) {
            return false;
        }
        previous_underscore = false;
    }

    return true;
}

}  // namespace

ResearchObjectRegistry::ResearchObjectRegistry(
    const std::size_t sequence_start)
    : sequence_start_(sequence_start == 0U ? 1U : sequence_start) {}

bool ResearchObjectRegistry::Register(
    const std::size_t number,
    std::string title) {
    if (number == 0U ||
        !IsIdentifierSafeTitle(title) ||
        ContainsNumber(number)) {
        return false;
    }

    records_.push_back(
        ResearchObjectRecord{
            number,
            std::move(title),
            true,
            true});
    return true;
}

bool ResearchObjectRegistry::ContainsNumber(
    const std::size_t number) const noexcept {
    return std::any_of(
        records_.begin(),
        records_.end(),
        [number](const ResearchObjectRecord& record) {
            return record.object_number == number;
        });
}

bool ResearchObjectRegistry::IsSequential() const noexcept {
    if (records_.empty()) {
        return true;
    }

    for (std::size_t index = 0U; index < records_.size(); ++index) {
        if (records_[index].object_number != sequence_start_ + index ||
            !records_[index].registered ||
            records_[index].title.empty()) {
            return false;
        }
    }

    return true;
}

const std::vector<ResearchObjectRecord>&
ResearchObjectRegistry::Records() const noexcept {
    return records_;
}

std::size_t ResearchObjectRegistry::SequenceStart() const noexcept {
    return sequence_start_;
}

bool ResearchObjectRegistrySelfTest() {
    ResearchObjectRegistry primary;

    if (!primary.IsSequential() ||
        !primary.Register(1U, "foundation_report_validator") ||
        !primary.Register(2U, "canonical_extension_registry") ||
        !primary.IsSequential() ||
        primary.Records().size() != 2U ||
        !primary.Records()[0].implemented ||
        !primary.Records()[1].title != "canonical_extension_registry") {
        return false;
    }

    if (primary.Register(2U, "duplicate_number") ||
        primary.Register(0U, "zero_number") ||
        primary.Register(3U, "Invalid Title") ||
        primary.Register(3U, "double__underscore")) {
        return false;
    }

    if (primary.Register(4U, "out_of_order") ||
        primary.IsSequential()) {
        return false;
    }

    ResearchObjectRegistry extension(51U);
    if (!extension.Register(51U, "foundation_report_validator") ||
        !extension.Register(52U, "canonical_extension_registry") ||
        !extension.IsSequential() ||
        extension.SequenceStart() != 51U ||
        !extension.ContainsNumber(51U) ||
        extension.ContainsNumber(53U)) {
        return false;
    }

    return true;
}
