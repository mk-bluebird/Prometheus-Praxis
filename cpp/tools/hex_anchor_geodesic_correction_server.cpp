// File: cpp/tools/hex_anchor_geodesic_correction_server.cpp
#include <httplib.h>
#include <proj.h>
#include <sqlite3.h>

#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

class CorrectionService {
public:
    CorrectionService(sqlite3* database, double origin_longitude, double origin_latitude,
                      double base_edge_m)
        : database_(database), edge_m_(base_edge_m), proj_(proj_context_create()) {
        transform_ = proj_create_crs_to_crs(proj_, "EPSG:4326", "EPSG:32612", nullptr);
        if (!transform_ || edge_m_ <= 0.0) throw std::runtime_error("PROJ initialization failed");
        const PJ_COORD origin = proj_trans(transform_, PJ_FWD, proj_coord(origin_longitude, origin_latitude, 0, 0));
        origin_x_ = origin.xy.x;
        origin_y_ = origin.xy.y;
        load_coefficients();
    }

    ~CorrectionService() {
        proj_destroy(transform_);
        proj_context_destroy(proj_);
    }

    std::string corrected(std::uint64_t anchor) const {
        const std::uint32_t level = static_cast<std::uint32_t>((anchor >> 60) & 15U);
        const std::uint32_t row = static_cast<std::uint32_t>((anchor >> 30) & 0x3FFFFFFFU);
        const std::uint32_t column = static_cast<std::uint32_t>(anchor & 0x3FFFFFFFU);
        const double scale = edge_m_ / static_cast<double>(std::uint64_t{1} << level);
        const double x = origin_x_ + std::sqrt(3.0) * scale * (column + 0.5 * row);
        const double y = origin_y_ + 1.5 * scale * row;
        const std::array<double, 6> basis{1.0, x, y, x * x, x * y, y * y};
        double corrected_x = 0.0, corrected_y = 0.0;
        for (std::size_t i = 0; i < basis.size(); ++i) {
            corrected_x += x_coefficients_[i] * basis[i];
            corrected_y += y_coefficients_[i] * basis[i];
        }
        return "{\"anchor\":" + std::to_string(anchor) + ",\"utm_easting_m\":" +
               std::to_string(corrected_x) + ",\"utm_northing_m\":" + std::to_string(corrected_y) + "}";
    }

private:
    void load_coefficients() {
        sqlite3_stmt* raw = nullptr;
        sqlite3_prepare_v2(database_,
            "SELECT term,x_coefficient,y_coefficient FROM hex_anchor_correction WHERE term BETWEEN 0 AND 5;",
            -1, &raw, nullptr);
        std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, sqlite3_finalize);
        while (sqlite3_step(statement.get()) == SQLITE_ROW) {
            const int term = sqlite3_column_int(statement.get(), 0);
            x_coefficients_[term] = sqlite3_column_double(statement.get(), 1);
            y_coefficients_[term] = sqlite3_column_double(statement.get(), 2);
        }
    }

    sqlite3* database_;
    double edge_m_, origin_x_{}, origin_y_{};
    std::array<double, 6> x_coefficients_{}, y_coefficients_{};
    PJ_CONTEXT* proj_;
    PJ* transform_{};
};

}  // namespace

int main(int argc, char** argv) {
    if (argc != 6) return 2;
    sqlite3* raw = nullptr;
    if (sqlite3_open_v2(argv[1], &raw, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) return 1;
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);
    CorrectionService service(database.get(), std::stod(argv[2]), std::stod(argv[3]), std::stod(argv[4]));
    httplib::Server server;
    server.Get(R"(/v1/hex/([0-9]+))", [&service](const httplib::Request& request, httplib::Response& response) {
        try {
            response.set_content(service.corrected(std::stoull(request.matches[1])), "application/json");
        } catch (...) {
            response.status = 400;
            response.set_content("{\"error\":\"invalid anchor\"}", "application/json");
        }
    });
    return server.listen("127.0.0.1", std::stoi(argv[5])) ? 0 : 1;
}
