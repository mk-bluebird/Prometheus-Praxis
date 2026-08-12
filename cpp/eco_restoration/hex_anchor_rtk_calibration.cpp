// File: cpp/eco_restoration/hex_anchor_rtk_calibration.cpp
#include <Eigen/Dense>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace ppx::eco_restoration {

struct GroundControlPoint {
    double predicted_easting_m{};
    double predicted_northing_m{};
    double rtk_easting_m{};
    double rtk_northing_m{};
    double horizontal_sigma_m{0.02};
};

struct HexGridCalibration {
    double scale{};
    double rotation_rad{};
    double easting_offset_m{};
    double northing_offset_m{};
    double residual_rms_m{};

    [[nodiscard]] Eigen::Vector2d apply(const Eigen::Vector2d& predicted_utm) const {
        const double cosine = std::cos(rotation_rad);
        const double sine = std::sin(rotation_rad);
        return {
            scale * (cosine * predicted_utm.x() - sine * predicted_utm.y()) + easting_offset_m,
            scale * (sine * predicted_utm.x() + cosine * predicted_utm.y()) + northing_offset_m
        };
    }
};

class HexAnchorRtkCalibrator {
public:
    [[nodiscard]] HexGridCalibration fit(const std::vector<GroundControlPoint>& points) const {
        if (points.size() < 2) {
            throw std::invalid_argument("at least two spatially distinct RTK control points are required");
        }

        Eigen::MatrixXd design(2 * points.size(), 4);
        Eigen::VectorXd observation(2 * points.size());

        for (std::size_t i = 0; i < points.size(); ++i) {
            const GroundControlPoint& point = points[i];
            if (!std::isfinite(point.predicted_easting_m) ||
                !std::isfinite(point.predicted_northing_m) ||
                !std::isfinite(point.rtk_easting_m) ||
                !std::isfinite(point.rtk_northing_m) ||
                point.horizontal_sigma_m <= 0.0) {
                throw std::invalid_argument("invalid RTK ground-control point");
            }

            const double weight = 1.0 / point.horizontal_sigma_m;
            const double x = point.predicted_easting_m;
            const double y = point.predicted_northing_m;

            design.row(2 * i) << weight * x, -weight * y, weight, 0.0;
            design.row(2 * i + 1) << weight * y, weight * x, 0.0, weight;
            observation(2 * i) = weight * point.rtk_easting_m;
            observation(2 * i + 1) = weight * point.rtk_northing_m;
        }

        const Eigen::ColPivHouseholderQR<Eigen::MatrixXd> qr(design);
        if (qr.rank() < 4) {
            throw std::invalid_argument("ground-control geometry cannot identify scale, rotation, and offsets");
        }

        const Eigen::Vector4d parameters = qr.solve(observation);
        const double a = parameters[0];
        const double b = parameters[1];
        HexGridCalibration result{
            std::hypot(a, b),
            std::atan2(b, a),
            parameters[2],
            parameters[3],
            0.0
        };

        double squared_error = 0.0;
        for (const GroundControlPoint& point : points) {
            const Eigen::Vector2d fitted = result.apply(
                {point.predicted_easting_m, point.predicted_northing_m});
            squared_error += std::pow(fitted.x() - point.rtk_easting_m, 2.0) +
                             std::pow(fitted.y() - point.rtk_northing_m, 2.0);
        }
        result.residual_rms_m = std::sqrt(squared_error / (2.0 * points.size()));
        return result;
    }
};

}  // namespace ppx::eco_restoration
