// File: cpp/eco_restoration/constexpr_telemetry_validator.hpp
#ifndef PPX_CONSTEXPR_TELEMETRY_VALIDATOR_HPP
#define PPX_CONSTEXPR_TELEMETRY_VALIDATOR_HPP

#include <cmath>
#include <tuple>
#include <utility>

namespace ppx::eco_restoration {

template <typename Object, auto Member>
struct FieldRange {
    double minimum;
    double maximum;

    [[nodiscard]] constexpr bool accepts(const Object& object) const noexcept {
        const double value = object.*Member;
        return std::isfinite(value) && value >= minimum && value <= maximum;
    }
};

template <typename Object, auto Member>
[[nodiscard]] constexpr auto allowed_range(double minimum, double maximum) {
    return FieldRange<Object, Member>{minimum, maximum};
}

template <typename Object, typename... Rules>
class TelemetryValidator {
public:
    constexpr explicit TelemetryValidator(Rules... rules) : rules_(std::move(rules)...) {}

    [[nodiscard]] constexpr bool operator()(const Object& object) const noexcept {
        return std::apply(
            [&object](const auto&... rule) constexpr {
                return (rule.accepts(object) && ...);
            },
            rules_
        );
    }

private:
    std::tuple<Rules...> rules_;
};

template <typename Object, typename... Rules>
[[nodiscard]] constexpr auto make_telemetry_validator(Rules... rules) {
    return TelemetryValidator<Object, Rules...>(std::move(rules)...);
}

/*
struct NumericTelemetry {
    double r_hydraulics, r_energy, r_uncertainty, r_reliability, roh;
};

constexpr auto validate_numeric = make_telemetry_validator<NumericTelemetry>(
    allowed_range<NumericTelemetry, &NumericTelemetry::r_hydraulics>(0.0, 1.0),
    allowed_range<NumericTelemetry, &NumericTelemetry::r_energy>(0.0, 1.0),
    allowed_range<NumericTelemetry, &NumericTelemetry::r_uncertainty>(0.0, 1.0),
    allowed_range<NumericTelemetry, &NumericTelemetry::r_reliability>(0.0, 1.0),
    allowed_range<NumericTelemetry, &NumericTelemetry::roh>(0.0, 1.0)
);
*/

}  // namespace ppx::eco_restoration

#endif
