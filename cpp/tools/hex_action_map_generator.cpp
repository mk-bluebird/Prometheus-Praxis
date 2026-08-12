// File: cpp/tools/hex_action_map_generator.cpp

#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <sqlite3.h>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace eco_restoration {

struct HexGrid {
    double origin_x{};
    double origin_y{};
    double edge_m{};
    std::uint8_t level{};
    std::uint32_t row_offset{};
    std::uint32_t column_offset{};

    std::uint64_t anchor(std::int64_t q, std::int64_t r) const {
        const std::int64_t row = static_cast<std::int64_t>(row_offset) + r;
        const std::int64_t column = static_cast<std::int64_t>(column_offset) + q;
        if (level > 15U || row < 0 || column < 0 || row > 1073741823LL || column > 1073741823LL) {
            throw std::runtime_error("hex coordinate cannot be encoded");
        }
        return (static_cast<std::uint64_t>(level) << 60U) |
               (static_cast<std::uint64_t>(row) << 30U) |
               static_cast<std::uint64_t>(column);
    }

    OGRPolygon polygon(std::int64_t q, std::int64_t r) const {
        const double x = origin_x + std::sqrt(3.0) * edge_m * (static_cast<double>(q) + r * 0.5);
        const double y = origin_y + 1.5 * edge_m * static_cast<double>(r);
        OGRLinearRing ring;

        for (int i = 0; i < 6; ++i) {
            const double angle = 3.14159265358979323846 / 180.0 * (60.0 * i + 30.0);
            ring.addPoint(x + edge_m * std::cos(angle), y + edge_m * std::sin(angle));
        }
        ring.closeRings();

        OGRPolygon result;
        result.addRing(&ring);
        return result;
    }
};

bool permissible_action(const std::string& action) {
    static const std::unordered_set<std::string> allowed{
        "tree_planting", "canal_cleaning", "native_seedling", "mulch_application",
        "infiltration_basin", "habitat_monitoring", "soil_amendment"
    };
    return allowed.contains(action);
}

void persist_actions(
    sqlite3* database,
    const std::unordered_map<std::uint64_t, std::string>& actions) {

    sqlite3_exec(database,
        "CREATE TABLE IF NOT EXISTS hex_restoration_action("
        "hex_anchor INTEGER PRIMARY KEY,action TEXT NOT NULL,"
        "CHECK(action IN ('tree_planting','canal_cleaning','native_seedling','mulch_application',"
        "'infiltration_basin','habitat_monitoring','soil_amendment'))"
        ") STRICT;",
        nullptr, nullptr, nullptr);

    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT INTO hex_restoration_action VALUES(?,?) "
        "ON CONFLICT(hex_anchor) DO UPDATE SET action=excluded.action;",
        -1, &statement, nullptr);

    for (const auto& [anchor, action] : actions) {
        sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(anchor));
        sqlite3_bind_text(statement, 2, action.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(statement) != SQLITE_DONE) {
            sqlite3_finalize(statement);
            throw std::runtime_error("cannot persist hex action");
        }
        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);
    }
    sqlite3_finalize(statement);
}

}  // namespace eco_restoration

int main(int argc, char** argv) {
    using namespace eco_restoration;

    if (argc != 11) {
        throw std::runtime_error(
            "usage: hex_action_map_generator plans.shp action_field origin_x origin_y edge_m "
            "level row_offset column_offset actions.csv actions.sqlite");
    }

    const HexGrid grid{
        std::stod(argv[3]), std::stod(argv[4]), std::stod(argv[5]),
        static_cast<std::uint8_t>(std::stoul(argv[6])),
        static_cast<std::uint32_t>(std::stoul(argv[7])),
        static_cast<std::uint32_t>(std::stoul(argv[8]))
    };
    if (grid.edge_m <= 0.0) throw std::invalid_argument("hex edge must be positive");

    GDALAllRegister();
    GDALDataset* dataset = static_cast<GDALDataset*>(
        GDALOpenEx(argv[1], GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
    if (dataset == nullptr || dataset->GetLayerCount() < 1) {
        if (dataset != nullptr) GDALClose(dataset);
        throw std::runtime_error("cannot open restoration plan datasource");
    }

    OGRLayer* layer = dataset->GetLayer(0);
    const int action_field = layer->GetLayerDefn()->GetFieldIndex(argv[2]);
    if (action_field < 0) {
        GDALClose(dataset);
        throw std::runtime_error("restoration action field is absent");
    }

    OGREnvelope extent;
    if (layer->GetExtent(&extent, TRUE) != OGRERR_NONE) {
        GDALClose(dataset);
        throw std::runtime_error("cannot determine restoration plan extent");
    }

    const std::int64_t range = static_cast<std::int64_t>(
        std::ceil(std::max(extent.MaxX - extent.MinX, extent.MaxY - extent.MinY) / grid.edge_m)) + 4;
    const std::int64_t q_min = static_cast<std::int64_t>(
        std::floor((extent.MinX - grid.origin_x) / (std::sqrt(3.0) * grid.edge_m))) - range;
    const std::int64_t q_max = q_min + 2 * range;
    const std::int64_t r_min = static_cast<std::int64_t>(
        std::floor((extent.MinY - grid.origin_y) / (1.5 * grid.edge_m))) - range;
    const std::int64_t r_max = r_min + 2 * range;

    std::unordered_map<std::uint64_t, std::string> actions;
    layer->ResetReading();

    while (OGRFeature* feature = layer->GetNextFeature()) {
        const std::string action = feature->GetFieldAsString(action_field);
        OGRGeometry* geometry = feature->GetGeometryRef();

        if (!permissible_action(action) || geometry == nullptr || geometry->IsEmpty()) {
            OGRFeature::DestroyFeature(feature);
            GDALClose(dataset);
            throw std::runtime_error("invalid restoration action or geometry");
        }

        for (std::int64_t r = r_min; r <= r_max; ++r) {
            for (std::int64_t q = q_min; q <= q_max; ++q) {
                OGRPolygon hex = grid.polygon(q, r);
                if (!geometry->Intersects(&hex)) continue;

                const std::uint64_t anchor = grid.anchor(q, r);
                const auto existing = actions.find(anchor);
                if (existing != actions.end() && existing->second != action) {
                    OGRFeature::DestroyFeature(feature);
                    GDALClose(dataset);
                    throw std::runtime_error("one hex intersects conflicting restoration actions");
                }
                actions.emplace(anchor, action);
            }
        }
        OGRFeature::DestroyFeature(feature);
    }
    GDALClose(dataset);

    std::ofstream output(argv[9]);
    if (!output) throw std::runtime_error("cannot create action CSV");
    output << "hex_anchor,action\n";
    for (const auto& [anchor, action] : actions) output << anchor << ',' << action << '\n';

    sqlite3* database = nullptr;
    if (sqlite3_open(argv[10], &database) != SQLITE_OK) {
        throw std::runtime_error("cannot open action SQLite database");
    }
    persist_actions(database, actions);
    sqlite3_close(database);
}
