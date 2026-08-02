// File: cpp/tools/environmental_sensor_normalizer.cpp
#include <iostream>
#include <vector>
#include <algorithm>

namespace eco {

struct RawSensorSample {
    double value;
    double min_expected;
    double max_expected;
};

class EnvironmentalSensorNormalizer {
public:
    double normalize(const RawSensorSample &sample) const {
        double span = sample.max_expected - sample.min_expected;
        if (span <= 0.0) return 0.0;
        double norm = (sample.value - sample.min_expected) / span;
        return std::clamp(norm, 0.0, 1.0);
    }

    std::vector<double> normalize_series(const std::vector<RawSensorSample> &series) const {
        std::vector<double> out;
        out.reserve(series.size());
        for (const auto &s : series) {
            out.push_back(normalize(s));
        }
        return out;
    }
};

} // namespace eco

int main() {
    eco::EnvironmentalSensorNormalizer norm;
    eco::RawSensorSample temp{30.0, 15.0, 40.0};
    std::cout << "Normalized temperature: " << norm.normalize(temp) << "\n";
    return 0;
}
