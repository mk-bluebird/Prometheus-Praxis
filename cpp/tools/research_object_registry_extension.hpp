// File: cpp/tools/research_object_registry_extension.hpp
#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct ResearchObjectRecord {
    std::size_t object_number{};
    std::string title;
    bool implemented{};
    bool registered{};
};

class ResearchObjectRegistry {
public:
    explicit ResearchObjectRegistry(std::size_t sequence_start = 1U);

    bool Register(std::size_t number, std::string title);

    bool ContainsNumber(std::size_t number) const noexcept;

    bool IsSequential() const noexcept;

    const std::vector<ResearchObjectRecord>& Records() const noexcept;

    std::size_t SequenceStart() const noexcept;

private:
    std::size_t sequence_start_{1U};
    std::vector<ResearchObjectRecord> records_;
};

bool ResearchObjectRegistrySelfTest();
