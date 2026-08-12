// File: cpp/eco_restoration/restricted_risk_expression.cpp
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace eco_restoration {

class RestrictedRiskExpression {
public:
    explicit RestrictedRiskExpression(std::string expression) : source_(std::move(expression)) {
        if (source_.empty() || source_.size() > 256) throw std::invalid_argument("invalid expression length");
    }

    double evaluate(const std::unordered_map<std::string, double>& variables) {
        variables_ = &variables;
        position_ = 0;
        const double result = expression();
        skip_space();
        if (position_ != source_.size() || !std::isfinite(result))
            throw std::invalid_argument("invalid risk expression");
        return std::clamp(result, 0.0, 1.0);
    }

private:
    double expression() {
        double value = term();
        while (true) {
            skip_space();
            if (consume('+')) value += term();
            else if (consume('-')) value -= term();
            else return value;
        }
    }

    double term() {
        double value = factor();
        while (true) {
            skip_space();
            if (consume('*')) value *= factor();
            else if (consume('/')) {
                const double divisor = factor();
                if (std::abs(divisor) < 1e-12) throw std::invalid_argument("division by zero");
                value /= divisor;
            } else return value;
        }
    }

    double factor() {
        skip_space();
        if (consume('(')) {
            const double value = expression();
            if (!consume(')')) throw std::invalid_argument("missing closing parenthesis");
            return value;
        }
        if (consume('-')) return -factor();
        if (position_ < source_.size() && (std::isdigit(source_[position_]) || source_[position_] == '.'))
            return number();

        const std::string name = identifier();
        skip_space();
        if (consume('(')) {
            const double first = expression();
            if (name == "abs") {
                if (!consume(')')) throw std::invalid_argument("abs accepts one argument");
                return std::abs(first);
            }
            if (!consume(',')) throw std::invalid_argument("function argument missing");
            const double second = expression();
            if (!consume(')')) throw std::invalid_argument("missing function parenthesis");
            if (name == "min") return std::min(first, second);
            if (name == "max") return std::max(first, second);
            throw std::invalid_argument("function not permitted");
        }
        const auto found = variables_->find(name);
        if (found == variables_->end() || !std::isfinite(found->second))
            throw std::invalid_argument("unknown risk variable");
        return found->second;
    }

    double number() {
        const std::size_t start = position_;
        while (position_ < source_.size() &&
               (std::isdigit(source_[position_]) || source_[position_] == '.')) ++position_;
        return std::stod(source_.substr(start, position_ - start));
    }

    std::string identifier() {
        skip_space();
        const std::size_t start = position_;
        while (position_ < source_.size() &&
               (std::islower(static_cast<unsigned char>(source_[position_])) ||
                std::isdigit(static_cast<unsigned char>(source_[position_])) ||
                source_[position_] == '_')) ++position_;
        if (start == position_) throw std::invalid_argument("identifier expected");
        return source_.substr(start, position_ - start);
    }

    bool consume(char expected) {
        skip_space();
        if (position_ < source_.size() && source_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    void skip_space() {
        while (position_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[position_]))) ++position_;
    }

    std::string source_;
    std::size_t position_{};
    const std::unordered_map<std::string, double>* variables_{};
};

}  // namespace eco_restoration
