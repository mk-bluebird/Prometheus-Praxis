// File: cpp/tools/bounded_result_cache.hpp
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct BoundedResultCacheEntry {
    std::string key;
    std::string value;
    std::size_t revision{};
};

class BoundedResultCache {
public:
    explicit BoundedResultCache(std::size_t capacity);

    bool Store(std::string_view key, std::string_view value);

    std::optional<std::string> Lookup(std::string_view key) const;

    void Clear() noexcept;

    std::size_t Size() const noexcept;

    std::size_t Capacity() const noexcept;

    std::vector<BoundedResultCacheEntry> Entries() const;

private:
    std::size_t capacity_{};
    std::size_t next_revision_{1U};
    std::vector<BoundedResultCacheEntry> entries_;
};

bool BoundedResultCacheSelfTest();
