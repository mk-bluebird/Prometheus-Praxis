// File: cpp/eco_restoration/hex_anchor_rs_transport.cpp
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace ppx::eco_restoration {

struct HexAnchor {
    std::uint8_t level{};
    std::uint32_t row{};
    std::uint32_t column{};
};

constexpr std::uint64_t encode_hex_anchor(const HexAnchor& anchor) {
    if (anchor.level > 15 || anchor.row >= (1U << anchor.level) ||
        anchor.column >= (1U << anchor.level)) {
        throw std::out_of_range("invalid hierarchical hex anchor coordinate");
    }
    return (static_cast<std::uint64_t>(anchor.level) << 60U) |
           (static_cast<std::uint64_t>(anchor.row) << 30U) |
           static_cast<std::uint64_t>(anchor.column);
}

constexpr HexAnchor decode_hex_anchor(std::uint64_t identifier) {
    return {
        static_cast<std::uint8_t>((identifier >> 60U) & 0x0FU),
        static_cast<std::uint32_t>((identifier >> 30U) & 0x3FFFFFFFU),
        static_cast<std::uint32_t>(identifier & 0x3FFFFFFFU)
    };
}

constexpr HexAnchor parent_hex_anchor(const HexAnchor& anchor) {
    if (anchor.level == 0) throw std::out_of_range("root anchor has no parent");
    return {
        static_cast<std::uint8_t>(anchor.level - 1U),
        anchor.row >> 1U,
        anchor.column >> 1U
    };
}

constexpr HexAnchor child_hex_anchor(const HexAnchor& parent, std::uint8_t row_bit,
                                     std::uint8_t column_bit) {
    if (parent.level >= 15 || row_bit > 1 || column_bit > 1) {
        throw std::out_of_range("invalid child anchor request");
    }
    return {
        static_cast<std::uint8_t>(parent.level + 1U),
        (parent.row << 1U) | row_bit,
        (parent.column << 1U) | column_bit
    };
}

class ReedSolomon16x12 {
public:
    using Symbol = std::uint8_t;
    using Message = std::array<Symbol, 12>;
    using Codeword = std::array<Symbol, 16>;

    ReedSolomon16x12() {
        std::uint16_t value = 1;
        for (int i = 0; i < 255; ++i) {
            exp_[i] = static_cast<Symbol>(value);
            log_[value] = static_cast<Symbol>(i);
            value <<= 1U;
            if ((value & 0x100U) != 0) value ^= 0x11DU;
        }
        for (int i = 255; i < 510; ++i) exp_[i] = exp_[i - 255];

        generator_ = {1, 0, 0, 0, 0};
        std::size_t degree = 0;
        for (int root = 0; root < 4; ++root) {
            std::array<Symbol, 5> next{};
            for (std::size_t i = 0; i <= degree; ++i) {
                next[i] ^= multiply(generator_[i], alpha(root));
                next[i + 1] ^= generator_[i];
            }
            generator_ = next;
            ++degree;
        }
    }

    [[nodiscard]] Codeword encode(const Message& message) const {
        Codeword work{};
        for (std::size_t i = 0; i < message.size(); ++i) work[i + 4] = message[i];

        for (int i = 15; i >= 4; --i) {
            const Symbol factor = work[i];
            for (int j = 0; j <= 4; ++j) {
                work[i - 4 + j] ^= multiply(factor, generator_[j]);
            }
        }

        Codeword encoded{};
        for (std::size_t i = 0; i < 4; ++i) encoded[i] = work[i];
        for (std::size_t i = 0; i < message.size(); ++i) encoded[i + 4] = message[i];
        return encoded;
    }

    [[nodiscard]] std::optional<unsigned> correct(Codeword& word) const {
        if (valid(word)) return 0U;

        for (int p = 0; p < 16; ++p) {
            const Symbol error = divide(syndrome(word, 0), alpha(p));
            word[p] ^= error;
            if (valid(word)) return 1U;
            word[p] ^= error;
        }

        const Symbol s0 = syndrome(word, 0);
        const Symbol s1 = syndrome(word, 1);
        for (int p = 0; p < 16; ++p) {
            for (int q = p + 1; q < 16; ++q) {
                const Symbol a = alpha(p);
                const Symbol b = alpha(q);
                const Symbol denominator =
                    multiply(a, multiply(b, b)) ^ multiply(multiply(a, a), b);
                if (denominator == 0) continue;

                const Symbol error_p = divide(
                    multiply(s0, multiply(b, b)) ^ multiply(s1, b), denominator);
                const Symbol error_q = divide(
                    multiply(a, s1) ^ multiply(multiply(a, a), s0), denominator);

                word[p] ^= error_p;
                word[q] ^= error_q;
                if (valid(word)) return 2U;
                word[p] ^= error_p;
                word[q] ^= error_q;
            }
        }
        return std::nullopt;
    }

private:
    std::array<Symbol, 510> exp_{};
    std::array<Symbol, 256> log_{};
    std::array<Symbol, 5> generator_{};

    [[nodiscard]] Symbol alpha(int exponent) const {
        return exp_[exponent % 255];
    }

    [[nodiscard]] Symbol multiply(Symbol left, Symbol right) const {
        return left == 0 || right == 0 ? 0 : exp_[log_[left] + log_[right]];
    }

    [[nodiscard]] Symbol divide(Symbol numerator, Symbol denominator) const {
        if (denominator == 0) throw std::domain_error("GF(256) division by zero");
        if (numerator == 0) return 0;
        return exp_[log_[numerator] + 255 - log_[denominator]];
    }

    [[nodiscard]] Symbol syndrome(const Codeword& word, int root) const {
        Symbol result = 0;
        for (int i = 0; i < 16; ++i) {
            result ^= multiply(word[i], alpha((root + 1) * i));
        }
        return result;
    }

    [[nodiscard]] bool valid(const Codeword& word) const {
        for (int root = 0; root < 4; ++root) {
            if (syndrome(word, root) != 0) return false;
        }
        return true;
    }
};

}  // namespace ppx::eco_restoration
