// File: cpp/eco_restoration/phoenix_hex_grid_and_lane_json.cpp
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string_view>

#include <proj.h>
#include <rapidjson/document.h>

namespace ppx::eco_restoration {

struct AxialHex { std::int64_t q{}, r{}; };
struct Utm12N { double easting_m{}, northing_m{}; };

class CamelbackHexGrid {
public:
    CamelbackHexGrid(double origin_latitude_deg, double origin_longitude_deg, double edge_m)
        : context_(proj_context_create()), edge_m_(edge_m) {
        if (!context_ || edge_m_ <= 0.0) throw std::invalid_argument("invalid grid construction");
        PJ* raw = proj_create_crs_to_crs(context_, "EPSG:4326", "EPSG:32612", nullptr);
        transform_ = proj_normalize_for_visualization(context_, raw);
        proj_destroy(raw);
        if (!transform_) throw std::runtime_error("cannot initialize WGS84 to UTM zone 12N");
        const PJ_COORD projected = proj_trans(
            transform_, PJ_FWD, proj_coord(origin_longitude_deg, origin_latitude_deg, 0.0, 0.0));
        origin_ = {projected.xy.x, projected.xy.y};
    }

    ~CamelbackHexGrid() {
        proj_destroy(transform_);
        proj_context_destroy(context_);
    }

    CamelbackHexGrid(const CamelbackHexGrid&) = delete;
    CamelbackHexGrid& operator=(const CamelbackHexGrid&) = delete;

    [[nodiscard]] Utm12N to_utm(AxialHex cell) const {
        return {
            origin_.easting_m + edge_m_ * std::sqrt(3.0) *
                (static_cast<double>(cell.q) + 0.5 * static_cast<double>(cell.r)),
            origin_.northing_m + edge_m_ * 1.5 * static_cast<double>(cell.r)
        };
    }

    [[nodiscard]] AxialHex from_utm(Utm12N point) const {
        const double local_y = point.northing_m - origin_.northing_m;
        const double r = 2.0 * local_y / (3.0 * edge_m_);
        const double q = (point.easting_m - origin_.easting_m) /
            (std::sqrt(3.0) * edge_m_) - 0.5 * r;
        return {static_cast<std::int64_t>(std::llround(q)),
                static_cast<std::int64_t>(std::llround(r))};
    }

private:
    PJ_CONTEXT* context_{};
    PJ* transform_{};
    Utm12N origin_{};
    double edge_m_{};
};

bool parse_lane_decision_json(std::string_view text) {
    rapidjson::Document document;
    document.Parse(text.data(), text.size());
    if (document.HasParseError() || !document.IsObject()) return false;

    const char* required_text[] = {
        "schema_id", "machine_id", "station_id", "timestamp_utc",
        "lane", "action", "reason_code"
    };
    const char* required_number[] = {
        "hex_anchor", "k_knowledge", "e_eco_impact", "r_risk",
        "roh", "vt_current", "vt_next", "delta_vt"
    };

    for (const char* key : required_text) {
        if (!document.HasMember(key) || !document[key].IsString()) return false;
    }
    for (const char* key : required_number) {
        if (!document.HasMember(key) || !document[key].IsNumber()) return false;
    }
    if (std::string_view(document["schema_id"].GetString()) != "ppx.ai_workload.lane.v1") return false;

    for (const char* key : {"k_knowledge", "e_eco_impact", "r_risk", "roh"}) {
        const double value = document[key].GetDouble();
        if (!std::isfinite(value) || value < 0.0 || value > 1.0) return false;
    }
    for (const char* key : {"r_extra_1", "r_extra_2"}) {
        if (document.HasMember(key) && !document[key].IsNull()) {
            if (!document[key].IsNumber() || document[key].GetDouble() < 0.0 ||
                document[key].GetDouble() > 1.0) return false;
        }
    }
    return true;
}

}  // namespace ppx::eco_restoration
