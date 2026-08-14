// File: cpp/tools/bounded_result_cache.cpp
#include "bounded_result_cache.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

bool IsLowerSnakeCase(const std::string_view key) noexcept {
    if (key.empty() || key.front() == '_' || key.back() == '_') {
        return false;
    }

    bool preceding_underscore = false;
    for (const char character : key) {
        const bool lower_case = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';

        if (character == '_') {
            if (preceding_underscore) {
                return false;
            }
            preceding_underscore = true;
            continue;
        }

        if (!lower_case && !digit) {
            return false;
        }
        preceding_underscore = false;
    }

    return true;
}

bool IsSingleLine(const std::string_view value) noexcept {
    return value.find('\n') == std::string_view::npos &&
           value.find('\r') == std::string_view::npos;
}

std::vector<BoundedResultCacheEntry>::iterator FindEntry(
    std::vector<BoundedResultCacheEntry>& entries,
    const std::string_view key) {
    return std::find_if(
        entries.begin(),
        entries.end(),
        [key](const BoundedResultCacheEntry& entry) {
            return entry.key == key;
        });
}

std::vector<BoundedResultCacheEntry>::const_iterator FindEntry(
    const std::vector<BoundedResultCacheEntry>& entries,
    const std::string_view key) {
    return std::find_if(
        entries.begin(),
        entries.end(),
        [key](const BoundedResultCacheEntry& entry) {
            return entry.key == key;
        });
}

}  // namespace

BoundedResultCache::BoundedResultCache(const std::size_t capacity)
    : capacity_(capacity) {
    entries_.reserve(capacity_);
}

bool BoundedResultCache::Store(const std::string_view key,
                               const std::string_view value) {
    if (capacity_ == 0U ||
        !IsLowerSnakeCase(key) ||
        !IsSingleLine(value) ||
        next_revision_ == 0U) {
        return false;
    }

    const std::size_t revision = next_revision_++;
    const auto existing = FindEntry(entries_, key);

    if (existing != entries_.end()) {
        existing->value.assign(value.data(), value.size());
        existing->revision = revision;
        return true;
    }

    if (entries_.size() == capacity_) {
        entries_.erase(entries_.begin());
    }

    entries_.push_back(
        BoundedResultCacheEntry{
            std::string(key),
            std::string(value),
            revision});
    return true;
}

std::optional<std::string> BoundedResultCache::Lookup(
    const std::string_view key) const {
    const auto existing = FindEntry(entries_, key);

    if (existing == entries_.end()) {
        return std::nullopt;
    }

    return existing->value;
}

void BoundedResultCache::Clear() noexcept {
    entries_.clear();
}

std::size_t BoundedResultCache::Size() const noexcept {
    return entries_.size();
}

std::size_t BoundedResultCache::Capacity() const noexcept {
    return capacity_;
}

std::vector<BoundedResultCacheEntry> BoundedResultCache::Entries() const {
    return entries_;
}

bool BoundedResultCacheSelfTest() {
    BoundedResultCache cache(2U);

    if (cache.Size() != 0U ||
        cache.Capacity() != 2U ||
        cache.Lookup("missing").has_value()) {
        return false;
    }

    if (!cache.Store("first_result", "initial") ||
        !cache.Store("second_result", "second") ||
        cache.Size() != 2U) {
        return false;
    }

    const std::vector<BoundedResultCacheEntry> initial_entries =
        cache.Entries();

    if (initial_entries.size() != 2U ||
        initial_entries[0].key != "first_result" ||
        initial_entries[1].key != "second_result" ||
        initial_entries[0].revision >= initial_entries[1].revision) {
        return false;
    }

    if (!cache.Store("first_result", "updated") ||
        cache.Size() != 2U ||
        !cache.Lookup("first_result").has_value() ||
        *cache.Lookup("first_result") != "updated") {
        return false;
    }

    const std::vector<BoundedResultCacheEntry> updated_entries =
        cache.Entries();

    if (updated_entries[0].revision <= initial_entries[0].revision ||
        updated_entries[1].revision != initial_entries[1].revision) {
        return false;
    }

    if (!cache.Store("third_result", "third") ||
        cache.Size() != 2U ||
        cache.Lookup("first_result").has_value() ||
        !cache.Lookup("second_result").has_value() ||
        !cache.Lookup("third_result").has_value()) {
        return false;
    }

    const std::vector<BoundedResultCacheEntry> evicted_entries =
        cache.Entries();

    if (evicted_entries.size() != 2U ||
        evicted_entries[0].key != "second_result" ||
        evicted_entries[1].key != "third_result") {
        return false;
    }

    if (cache.Store("Invalid-Key", "value") ||
        cache.Store("valid_key", "contains\nnewline")) {
        return false;
    }

    cache.Clear();
    if (cache.Size() != 0U ||
        !cache.Entries().empty()) {
        return false;
    }

    BoundedResultCache zero_capacity(0U);
    return !zero_capacity.Store("result", "value") &&
           !zero_capacity.Lookup("result").has_value();
}
