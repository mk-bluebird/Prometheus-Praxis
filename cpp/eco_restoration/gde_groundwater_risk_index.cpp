// File: cpp/eco_restoration/gde_groundwater_risk_index.cpp
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace eco_restoration {

struct GdeRiskParameters {
    double maximum_root_access_depth_m{};
    double maximum_decline_m_year{};
    double restoration_water_demand{};
};

class GdeRiskIndexer {
public:
    void ingest(const std::string& depth_raster_path, const std::string& decline_raster_path,
                const std::string& hex_geopackage, const std::string& sqlite_path,
                const GdeRiskParameters& parameters, std::int64_t observed_unix_s) const {
        if (parameters.maximum_root_access_depth_m <= 0.0 ||
            parameters.maximum_decline_m_year <= 0.0 || parameters.restoration_water_demand < 0.0)
            throw std::invalid_argument("invalid GDE risk parameters");

        GDALAllRegister();
        std::unique_ptr<GDALDataset, decltype(&GDALClose)> depth(
            GDALDataset::Open(depth_raster_path.c_str(), GDAL_OF_RASTER), GDALClose);
        std::unique_ptr<GDALDataset, decltype(&GDALClose)> decline(
            GDALDataset::Open(decline_raster_path.c_str(), GDAL_OF_RASTER), GDALClose);
        std::unique_ptr<GDALDataset, decltype(&GDALClose)> hex_data(
            GDALDataset::Open(hex_geopackage.c_str(), GDAL_OF_VECTOR), GDALClose);
        if (!depth || !decline || !hex_data) throw std::runtime_error("cannot open GDE inputs");
        if (depth->GetRasterXSize() != decline->GetRasterXSize() ||
            depth->GetRasterYSize() != decline->GetRasterYSize())
            throw std::invalid_argument("groundwater rasters must share dimensions");

        OGRLayer* hexes = hex_data->GetLayerByName("hex_anchor");
        if (!hexes) throw std::runtime_error("hex_anchor layer missing");
        double transform[6]{}, inverse[6]{};
        if (depth->GetGeoTransform(transform) != CE_None || !GDALInvGeoTransform(transform, inverse))
            throw std::runtime_error("groundwater raster requires an invertible geotransform");

        sqlite3* raw = nullptr;
        if (sqlite3_open(sqlite_path.c_str(), &raw) != SQLITE_OK) throw std::runtime_error("SQLite open failed");
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);
        sqlite3_exec(database,
            "CREATE TABLE IF NOT EXISTS hex_gde_risk("
            "hex_anchor INTEGER NOT NULL,observed_unix_s INTEGER NOT NULL,"
            "depth_to_groundwater_m REAL NOT NULL,decline_m_year REAL NOT NULL,"
            "r_gde REAL NOT NULL CHECK(r_gde BETWEEN 0 AND 1),"
            "PRIMARY KEY(hex_anchor,observed_unix_s)) STRICT;", nullptr, nullptr, nullptr);

        hexes->ResetReading();
        while (OGRFeature* raw_feature = hexes->GetNextFeature()) {
            std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)>
                feature(raw_feature, OGRFeature::DestroyFeature);
            const OGRGeometry* geometry = feature->GetGeometryRef();
            if (!geometry) continue;

            OGREnvelope envelope;
            geometry->getEnvelope(&envelope);
            double x0{}, y0{}, x1{}, y1{};
            GDALApplyGeoTransform(inverse, envelope.MinX, envelope.MinY, &x0, &y0);
            GDALApplyGeoTransform(inverse, envelope.MaxX, envelope.MaxY, &x1, &y1);
            const int min_x = std::clamp(static_cast<int>(std::floor(std::min(x0, x1))), 0, depth->GetRasterXSize() - 1);
            const int max_x = std::clamp(static_cast<int>(std::ceil(std::max(x0, x1))), 0, depth->GetRasterXSize() - 1);
            const int min_y = std::clamp(static_cast<int>(std::floor(std::min(y0, y1))), 0, depth->GetRasterYSize() - 1);
            const int max_y = std::clamp(static_cast<int>(std::ceil(std::max(y0, y1))), 0, depth->GetRasterYSize() - 1);

            double depth_sum = 0.0, decline_sum = 0.0;
            int samples = 0;
            for (int y = min_y; y <= max_y; ++y) for (int x = min_x; x <= max_x; ++x) {
                double map_x{}, map_y{}, depth_m{}, decline_m_year{};
                GDALApplyGeoTransform(transform, x + 0.5, y + 0.5, &map_x, &map_y);
                OGRPoint point(map_x, map_y);
                if (!geometry->Contains(&point)) continue;
                depth->GetRasterBand(1)->RasterIO(GF_Read, x, y, 1, 1, &depth_m, 1, 1, GDT_Float64, 0, 0);
                decline->GetRasterBand(1)->RasterIO(GF_Read, x, y, 1, 1, &decline_m_year, 1, 1, GDT_Float64, 0, 0);
                if (std::isfinite(depth_m) && std::isfinite(decline_m_year)) {
                    depth_sum += std::max(0.0, depth_m);
                    decline_sum += std::max(0.0, decline_m_year);
                    ++samples;
                }
            }
            if (samples == 0) continue;
            const double mean_depth = depth_sum / samples;
            const double mean_decline = decline_sum / samples;
            const double risk = std::clamp(
                0.50 * mean_depth / parameters.maximum_root_access_depth_m +
                0.40 * mean_decline / parameters.maximum_decline_m_year +
                0.10 * parameters.restoration_water_demand, 0.0, 1.0);

            sqlite3_stmt* statement = nullptr;
            sqlite3_prepare_v2(database,
                "INSERT INTO hex_gde_risk VALUES(?,?,?,?,?) ON CONFLICT(hex_anchor,observed_unix_s) "
                "DO UPDATE SET depth_to_groundwater_m=excluded.depth_to_groundwater_m,"
                "decline_m_year=excluded.decline_m_year,r_gde=excluded.r_gde;",
                -1, &statement, nullptr);
            std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> close_statement(statement, sqlite3_finalize);
            sqlite3_bind_int64(statement, 1, feature->GetFieldAsInteger64("anchor"));
            sqlite3_bind_int64(statement, 2, observed_unix_s);
            sqlite3_bind_double(statement, 3, mean_depth);
            sqlite3_bind_double(statement, 4, mean_decline);
            sqlite3_bind_double(statement, 5, risk);
            if (sqlite3_step(statement) != SQLITE_DONE) throw std::runtime_error("GDE risk upsert failed");
        }
    }
};

}  // namespace eco_restoration
