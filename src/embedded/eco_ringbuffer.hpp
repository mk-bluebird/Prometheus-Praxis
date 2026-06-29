#pragma once
#include <cstdint>
#include "eco_state.hpp"

struct EcoTelemetrySample {
    uint32_t timestamp_ms;
    EcoState2 state;
    EcoCommand command;
    float V;
};

template<std::size_t N>
class EcoRingBuffer {
    static_assert((N & (N - 1)) == 0, "N must be power of two");
public:
    EcoRingBuffer() : head_(0), tail_(0) {}

    bool push(const EcoTelemetrySample& s) {
        const std::size_t next = (head_ + 1) & (N - 1);
        if (next == tail_) {
            return false;
        }
        buffer_[head_] = s;
        head_ = next;
        return true;
    }

    bool pop(EcoTelemetrySample& out) {
        if (tail_ == head_) {
            return false;
        }
        out = buffer_[tail_];
        tail_ = (tail_ + 1) & (N - 1);
        return true;
    }

    bool empty() const { return head_ == tail_; }

private:
    EcoTelemetrySample buffer_[N];
    std::size_t head_;
    std::size_t tail_;
};
