// File: cpp/eco_restoration/drone_thermal_hex_aggregation.cpp
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

namespace eco_restoration {

class DroneThermalHexAggregation {
public:
    void ingest(const std::string& orthomosaic_path, const std::string& hex_geopackage,
                const std::string& sqlite_path, double kelvin_scale, double kelvin_offset,
                double reference_c, double range_c, double uncertainty_weight,
                std::int64_t observed_unix_s) const {
        if (kelvin_scale <= 0.0 || range_c <= 0.0 || uncertainty_weight < 0.0)
            throw std::invalid_argument("invalid thermal calibration");

        GDALAllRegister();
        std::unique_ptr<GDALDataset, decltype(&GDALClose)> raster(
            GDALDataset::Open(orthomosaic_path.c_str(), GDAL_OF_RASTER), GDALClose);
        std::unique_ptr<GDALDataset, decltype(&GDALClose)> hexes(
            GDALDataset::Open(hex_geopackage.c_str(), GDAL_OF_VECTOR), GDALClose);
        if (!raster || !hexes) throw std::runtime_error("cannot open thermal source or hex geometry");

        OGRLayer* layer = hexes->GetLayerByName("hex_anchor");
        GDALRasterBand* band = raster->GetRasterBand(1);
        if (!layer || !band) throw std::runtime_error("required layer or thermal band absent");

        double transform[6]{};
        if (raster->GetGeoTransform(transform) != CE_None) throw std::runtime_error("missing geotransform");
        double inverse[6]{};
        if (!GDALInvGeoTransform(transform, inverse)) throw std::runtime_error("non-invertible geotransform");

        sqlite3* database = nullptr;
        if (sqlite3_open(sqlite_path.c_str(), &database) != SQLITE_OK) throw std::runtime_error("cannot open SQLite");
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> close_database(database, sqlite3_close);
        sqlite3_exec(database,
            "CREATE TABLE IF NOT EXISTS hex_heat_observation("
            "hex_anchor INTEGER NOT NULL,observed_unix_s INTEGER NOT NULL,"
            "temperature_c REAL NOT NULL,temperature_sd_c REAL NOT NULL,r_heat REAL NOT NULL "
            "CHECK(r_heat BETWEEN 0 AND 1),source TEXT NOT NULL,"
            "PRIMARY KEY(hex_anchor,observed_unix_s,source)) STRICT;", nullptr, nullptr, nullptr);

        layer->ResetReading();
        while (OGRFeature* raw = layer->GetNextFeature()) {
            std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> feature(raw, OGRFeature::DestroyFeature);
            const OGRGeometry* geometry = feature->GetGeometryRef();
            if (!geometry) continue;
            OGREnvelope bounds;
            geometry->getEnvelope(&bounds);
            int x0, y0, x1, y1;
            GDALApplyGeoTransform(inverse, bounds.MinX, bounds.MinY, &x0, &y0);
            GDALApplyGeoTransform(inverse, bounds.MaxX, bounds.MaxY, &x1, &y1);
            x0 = std::clamp(std::min(x0, x1), 0, raster->GetRasterXSize() - 1);
            x1 = std::clamp(std::max(x0, x1), 0, raster->GetRasterXSize() - 1);
            y0 = std::clamp(std::min(y0, y1), 0, raster->GetRasterYSize() - 1);
            y1 = std::clamp(std::max(y0, y1), 0, raster->GetRasterYSize() - 1);

            double sum = 0.0, sum_sq = 0.0;
            int count = 0;
            for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
                double raw_value{};
                band->RasterIO(GF_Read, x, y, 1, 1, &raw_value, 1, 1, GDT_Float64, 0, 0);
                if (band->GetNoDataValue() == raw_value) continue;
                const double celsius = raw_value * kelvin_scale + kelvin_offset - 273.15;
                sum += celsius; sum_sq += celsius * celsius; ++count;
            }
            if (count == 0) continue;
            const double mean = sum / count;
            const double sd = std::sqrt(std::max(0.0, sum_sq / count - mean * mean));
            const double risk = std::clamp((mean - reference_c) / range_c + uncertainty_weight * sd, 0.0, 1.0);

            sqlite3_stmt* statement = nullptr;
            sqlite3_prepare_v2(database,
                "INSERT INTO hex_heat_observation VALUES(?,?,?,?,?, 'drone_thermal') "
                "ON CONFLICT(hex_anchor,observed_unix_s,source) DO UPDATE SET "
                "temperature_c=excluded.temperature_c,temperature_sd_c=excluded.temperature_sd_c,r_heat=excluded.r_heat;",
                -1, &statement, nullptr);
            sqlite3_bind_int64(statement, 1, feature->GetFieldAsInteger64("anchor"));
            sqlite3_bind_int64(statement, 2, observed_unix_s);
            sqlite3_bind_double(statement, 3, mean);
            sqlite3_bind_double(statement, 4, sd);
            sqlite3_bind_double(statement, 5, risk);
            if (sqlite3_step(statement) != SQLITE_DONE) throw std::runtime_error("heat upsert failed");
            sqlite3_finalize(statement);
        }
    }
};

}  // namespace eco_restoration
