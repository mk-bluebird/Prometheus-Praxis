// File: cpp/eco_restoration/sentinel2_hex_actual_et.cpp
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

struct SebalParameters {
    double net_radiation_w_m2{};
    double air_temperature_c{};
    double aerodynamic_heat_w_m2_c{};
    double latent_heat_j_kg{2.45e6};
};

class SentinelHexEt {
public:
    void ingest(const std::string& sentinel_raster, const std::string& hex_geopackage,
                const std::string& sqlite_path, const SebalParameters& parameters,
                std::int64_t observed_unix_s) const {
        if (parameters.net_radiation_w_m2 < 0.0 || parameters.aerodynamic_heat_w_m2_c < 0.0 ||
            parameters.latent_heat_j_kg <= 0.0) throw std::invalid_argument("invalid ET parameters");

        GDALAllRegister();
        std::unique_ptr<GDALDataset, decltype(&GDALClose)> raster(
            GDALDataset::Open(sentinel_raster.c_str(), GDAL_OF_RASTER), GDALClose);
        std::unique_ptr<GDALDataset, decltype(&GDALClose)> vectors(
            GDALDataset::Open(hex_geopackage.c_str(), GDAL_OF_VECTOR), GDALClose);
        if (!raster || !vectors || raster->GetRasterCount() < 3) throw std::runtime_error("missing Sentinel bands");

        OGRLayer* hexes = vectors->GetLayerByName("hex_anchor");
        if (!hexes) throw std::runtime_error("hex_anchor layer missing");
        GDALRasterBand* red = raster->GetRasterBand(1);
        GDALRasterBand* nir = raster->GetRasterBand(2);
        GDALRasterBand* surface_temperature = raster->GetRasterBand(3);

        double transform[6]{};
        if (raster->GetGeoTransform(transform) != CE_None) throw std::runtime_error("raster geotransform missing");

        sqlite3* raw = nullptr;
        if (sqlite3_open(sqlite_path.c_str(), &raw) != SQLITE_OK) throw std::runtime_error("SQLite open failed");
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);
        sqlite3_exec(database,
            "CREATE TABLE IF NOT EXISTS hex_actual_et("
            "hex_anchor INTEGER NOT NULL,observed_unix_s INTEGER NOT NULL,"
            "et_mm_day REAL NOT NULL CHECK(et_mm_day>=0),sampled_pixels INTEGER NOT NULL,"
            "PRIMARY KEY(hex_anchor,observed_unix_s)) STRICT;", nullptr, nullptr, nullptr);

        hexes->ResetReading();
        while (OGRFeature* raw_feature = hexes->GetNextFeature()) {
            std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)>
                feature(raw_feature, OGRFeature::DestroyFeature);
            const OGRGeometry* geometry = feature->GetGeometryRef();
            if (!geometry) continue;

            OGREnvelope envelope;
            geometry->getEnvelope(&envelope);
            double inverse[6]{};
            if (!GDALInvGeoTransform(transform, inverse)) throw std::runtime_error("non-invertible geotransform");
            double px0{}, py0{}, px1{}, py1{};
            GDALApplyGeoTransform(inverse, envelope.MinX, envelope.MinY, &px0, &py0);
            GDALApplyGeoTransform(inverse, envelope.MaxX, envelope.MaxY, &px1, &py1);

            const int min_x = std::clamp(static_cast<int>(std::floor(std::min(px0, px1))), 0, raster->GetRasterXSize() - 1);
            const int max_x = std::clamp(static_cast<int>(std::ceil(std::max(px0, px1))), 0, raster->GetRasterXSize() - 1);
            const int min_y = std::clamp(static_cast<int>(std::floor(std::min(py0, py1))), 0, raster->GetRasterYSize() - 1);
            const int max_y = std::clamp(static_cast<int>(std::ceil(std::max(py0, py1))), 0, raster->GetRasterYSize() - 1);

            double et_sum = 0.0;
            int samples = 0;
            for (int y = min_y; y <= max_y; ++y) for (int x = min_x; x <= max_x; ++x) {
                double x_m{}, y_m{};
                GDALApplyGeoTransform(transform, x + 0.5, y + 0.5, &x_m, &y_m);
                OGRPoint point(x_m, y_m);
                if (!geometry->Contains(&point)) continue;

                double red_value{}, nir_value{}, temperature_k{};
                red->RasterIO(GF_Read, x, y, 1, 1, &red_value, 1, 1, GDT_Float64, 0, 0);
                nir->RasterIO(GF_Read, x, y, 1, 1, &nir_value, 1, 1, GDT_Float64, 0, 0);
                surface_temperature->RasterIO(GF_Read, x, y, 1, 1, &temperature_k, 1, 1, GDT_Float64, 0, 0);
                const double ndvi = (nir_value - red_value) / std::max(1e-9, nir_value + red_value);
                const double soil_heat = parameters.net_radiation_w_m2 *
                    std::clamp(0.05 + 0.18 * std::exp(-0.521 * std::max(-0.99, ndvi)), 0.0, 0.5);
                const double sensible_heat = std::max(0.0, parameters.aerodynamic_heat_w_m2_c *
                    (temperature_k - 273.15 - parameters.air_temperature_c));
                const double latent_heat = std::max(0.0, parameters.net_radiation_w_m2 - soil_heat - sensible_heat);
                et_sum += latent_heat * 86400.0 / parameters.latent_heat_j_kg;
                ++samples;
            }
            if (samples == 0) continue;

            sqlite3_stmt* statement = nullptr;
            sqlite3_prepare_v2(database,
                "INSERT INTO hex_actual_et VALUES(?,?,?,?) ON CONFLICT(hex_anchor,observed_unix_s) "
                "DO UPDATE SET et_mm_day=excluded.et_mm_day,sampled_pixels=excluded.sampled_pixels;",
                -1, &statement, nullptr);
            std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> close_statement(statement, sqlite3_finalize);
            sqlite3_bind_int64(statement, 1, feature->GetFieldAsInteger64("anchor"));
            sqlite3_bind_int64(statement, 2, observed_unix_s);
            sqlite3_bind_double(statement, 3, et_sum / samples);
            sqlite3_bind_int(statement, 4, samples);
            if (sqlite3_step(statement) != SQLITE_DONE) throw std::runtime_error("ET upsert failed");
        }
    }
};

}  // namespace eco_restoration
