// File: cpp/eco_restoration/hex_renewable_resource_map.cpp
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

struct RenewableClimate {
    double annual_horizontal_irradiance_kwh_m2{};
    double mean_wind_speed_m_s{};
    double air_density_kg_m3{1.225};
    double small_wind_power_coefficient{0.25};
};

void build_hex_renewable_resource_map(const std::string& dem_path, const std::string& hex_geopackage,
                                      const std::string& sqlite_path, const RenewableClimate& climate,
                                      std::int64_t observed_unix_s) {
    if (climate.annual_horizontal_irradiance_kwh_m2 < 0.0 || climate.mean_wind_speed_m_s < 0.0 ||
        climate.air_density_kg_m3 <= 0.0 || climate.small_wind_power_coefficient < 0.0)
        throw std::invalid_argument("invalid renewable climate inputs");

    GDALAllRegister();
    std::unique_ptr<GDALDataset, decltype(&GDALClose)> dem(
        GDALDataset::Open(dem_path.c_str(), GDAL_OF_RASTER), GDALClose);
    std::unique_ptr<GDALDataset, decltype(&GDALClose)> vectors(
        GDALDataset::Open(hex_geopackage.c_str(), GDAL_OF_VECTOR), GDALClose);
    if (!dem || !vectors) throw std::runtime_error("cannot open DEM or hex layer");
    OGRLayer* hexes = vectors->GetLayerByName("hex_anchor");
    if (!hexes) throw std::runtime_error("hex_anchor layer missing");

    double transform[6]{}, inverse[6]{};
    if (dem->GetGeoTransform(transform) != CE_None || !GDALInvGeoTransform(transform, inverse))
        throw std::runtime_error("DEM requires an invertible geotransform");

    sqlite3* raw = nullptr;
    if (sqlite3_open(sqlite_path.c_str(), &raw) != SQLITE_OK) throw std::runtime_error("SQLite open failed");
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);
    sqlite3_exec(database,
        "CREATE TABLE IF NOT EXISTS hex_renewable_resource("
        "hex_anchor INTEGER NOT NULL,observed_unix_s INTEGER NOT NULL,"
        "solar_kwh_m2_year REAL NOT NULL,wind_w_m2 REAL NOT NULL,"
        "mean_slope_degrees REAL NOT NULL,PRIMARY KEY(hex_anchor,observed_unix_s)) STRICT;",
        nullptr, nullptr, nullptr);

    GDALRasterBand* band = dem->GetRasterBand(1);
    hexes->ResetReading();
    while (OGRFeature* raw_feature = hexes->GetNextFeature()) {
        std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)>
            feature(raw_feature, OGRFeature::DestroyFeature);
        const OGRGeometry* geometry = feature->GetGeometryRef();
        if (!geometry) continue;

        OGREnvelope envelope;
        geometry->getEnvelope(&envelope);
        double a{}, b{}, c{}, d{};
        GDALApplyGeoTransform(inverse, envelope.MinX, envelope.MinY, &a, &b);
        GDALApplyGeoTransform(inverse, envelope.MaxX, envelope.MaxY, &c, &d);
        const int min_x = std::clamp(static_cast<int>(std::floor(std::min(a, c))), 1, dem->GetRasterXSize() - 2);
        const int max_x = std::clamp(static_cast<int>(std::ceil(std::max(a, c))), 1, dem->GetRasterXSize() - 2);
        const int min_y = std::clamp(static_cast<int>(std::floor(std::min(b, d))), 1, dem->GetRasterYSize() - 2);
        const int max_y = std::clamp(static_cast<int>(std::ceil(std::max(b, d))), 1, dem->GetRasterYSize() - 2);

        double slope_sum = 0.0;
        int count = 0;
        for (int y = min_y; y <= max_y; ++y) for (int x = min_x; x <= max_x; ++x) {
            double map_x{}, map_y{}, z_l{}, z_r{}, z_u{}, z_d{};
            GDALApplyGeoTransform(transform, x + 0.5, y + 0.5, &map_x, &map_y);
            OGRPoint point(map_x, map_y);
            if (!geometry->Contains(&point)) continue;
            band->RasterIO(GF_Read, x - 1, y, 1, 1, &z_l, 1, 1, GDT_Float64, 0, 0);
            band->RasterIO(GF_Read, x + 1, y, 1, 1, &z_r, 1, 1, GDT_Float64, 0, 0);
            band->RasterIO(GF_Read, x, y - 1, 1, 1, &z_u, 1, 1, GDT_Float64, 0, 0);
            band->RasterIO(GF_Read, x, y + 1, 1, 1, &z_d, 1, 1, GDT_Float64, 0, 0);
            const double gradient = std::hypot((z_r - z_l) / (2.0 * std::abs(transform[1])),
                                               (z_d - z_u) / (2.0 * std::abs(transform[5])));
            slope_sum += std::atan(gradient);
            ++count;
        }
        if (count == 0) continue;
        const double slope = slope_sum / count;
        const double solar = climate.annual_horizontal_irradiance_kwh_m2 * std::cos(slope);
        const double wind = 0.5 * climate.air_density_kg_m3 *
                            std::pow(climate.mean_wind_speed_m_s, 3) *
                            climate.small_wind_power_coefficient * (1.0 + 0.15 * std::sin(slope));

        sqlite3_stmt* statement = nullptr;
        sqlite3_prepare_v2(database,
            "INSERT INTO hex_renewable_resource VALUES(?,?,?,?,?) "
            "ON CONFLICT(hex_anchor,observed_unix_s) DO UPDATE SET "
            "solar_kwh_m2_year=excluded.solar_kwh_m2_year,wind_w_m2=excluded.wind_w_m2,"
            "mean_slope_degrees=excluded.mean_slope_degrees;",
            -1, &statement, nullptr);
        std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> close_statement(statement, sqlite3_finalize);
        sqlite3_bind_int64(statement, 1, feature->GetFieldAsInteger64("anchor"));
        sqlite3_bind_int64(statement, 2, observed_unix_s);
        sqlite3_bind_double(statement, 3, std::max(0.0, solar));
        sqlite3_bind_double(statement, 4, std::max(0.0, wind));
        sqlite3_bind_double(statement, 5, slope * 180.0 / std::acos(-1.0));
        if (sqlite3_step(statement) != SQLITE_DONE) throw std::runtime_error("resource upsert failed");
    }
}

}  // namespace eco_restoration
