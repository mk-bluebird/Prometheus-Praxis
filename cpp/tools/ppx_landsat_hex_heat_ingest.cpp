// File: cpp/tools/ppx_landsat_hex_heat_ingest.cpp
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

#include <gdal_priv.h>
#include <ogr_spatialref.h>
#include <sqlite3.h>

namespace ppx::eco_restoration {

class LandsatHexHeatIngest {
public:
    void ingest(const std::string& geotiff_path, const std::string& sqlite_path,
                double scale_to_kelvin, double offset_kelvin, double reference_c,
                double range_c, const std::string& observed_utc) const {
        if (range_c <= 0.0 || scale_to_kelvin <= 0.0) {
            throw std::invalid_argument("invalid Landsat LST calibration");
        }

        GDALAllRegister();
        GDALDataset* raster = static_cast<GDALDataset*>(GDALOpen(geotiff_path.c_str(), GA_ReadOnly));
        if (!raster) throw std::runtime_error("cannot open Landsat LST GeoTIFF");

        sqlite3* database = nullptr;
        if (sqlite3_open_v2(sqlite_path.c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
            GDALClose(raster);
            throw std::runtime_error("cannot open SQLite database");
        }

        try {
            write_hex_risk(raster, database, scale_to_kelvin, offset_kelvin,
                           reference_c, range_c, observed_utc);
            sqlite3_close(database);
            GDALClose(raster);
        } catch (...) {
            sqlite3_close(database);
            GDALClose(raster);
            throw;
        }
    }

private:
    static void write_hex_risk(
        GDALDataset* raster, sqlite3* database, double scale, double offset,
        double reference_c, double range_c, const std::string& observed_utc) {
        double transform[6]{};
        double inverse[6]{};
        if (raster->GetGeoTransform(transform) != CE_None ||
            !GDALInvGeoTransform(transform, inverse)) {
            throw std::runtime_error("GeoTIFF must have an invertible geotransform");
        }

        const OGRSpatialReference* raster_srs = raster->GetSpatialRef();
        if (!raster_srs || !raster->GetRasterBand(1)) {
            throw std::runtime_error("GeoTIFF must contain spatial reference and LST band");
        }

        OGRSpatialReference wgs84;
        wgs84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        wgs84.SetWellKnownGeogCS("WGS84");
        OGRCoordinateTransformation* projection =
            OGRCreateCoordinateTransformation(&wgs84, raster_srs);
        if (!projection) throw std::runtime_error("cannot project hex centroids to Landsat CRS");

        sqlite3_stmt* anchors = nullptr;
        sqlite3_stmt* upsert = nullptr;
        const char* query = "SELECT hex_anchor,latitude_deg,longitude_deg FROM ppx_hex_anchor;";
        const char* write =
            "INSERT INTO ppx_heat_risk(hex_anchor,observed_utc,lst_c,r_heat) VALUES(?,?,?,?) "
            "ON CONFLICT(hex_anchor,observed_utc) DO UPDATE SET lst_c=excluded.lst_c,r_heat=excluded.r_heat;";

        if (sqlite3_prepare_v2(database, query, -1, &anchors, nullptr) != SQLITE_OK ||
            sqlite3_prepare_v2(database, write, -1, &upsert, nullptr) != SQLITE_OK) {
            sqlite3_finalize(anchors); sqlite3_finalize(upsert);
            OCTDestroyCoordinateTransformation(projection);
            throw std::runtime_error("cannot prepare SQLite statements");
        }

        GDALRasterBand* band = raster->GetRasterBand(1);
        int has_nodata = 0;
        const double nodata = band->GetNoDataValue(&has_nodata);
        sqlite3_exec(database, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr);

        while (sqlite3_step(anchors) == SQLITE_ROW) {
            double longitude = sqlite3_column_double(anchors, 2);
            double latitude = sqlite3_column_double(anchors, 1);
            if (!projection->Transform(1, &longitude, &latitude)) continue;

            double pixel = 0.0, line = 0.0;
            GDALApplyGeoTransform(inverse, longitude, latitude, &pixel, &line);
            const int x = static_cast<int>(std::floor(pixel));
            const int y = static_cast<int>(std::floor(line));
            if (x < 0 || y < 0 || x >= raster->GetRasterXSize() || y >= raster->GetRasterYSize()) continue;

            double raw_lst = 0.0;
            if (band->RasterIO(GF_Read, x, y, 1, 1, &raw_lst, 1, 1, GDT_Float64, 0, 0) != CE_None ||
                (has_nodata && raw_lst == nodata)) continue;

            const double lst_c = raw_lst * scale + offset - 273.15;
            const double risk = std::clamp((lst_c - reference_c) / range_c, 0.0, 1.0);
            sqlite3_reset(upsert);
            sqlite3_bind_int64(upsert, 1, sqlite3_column_int64(anchors, 0));
            sqlite3_bind_text(upsert, 2, observed_utc.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(upsert, 3, lst_c);
            sqlite3_bind_double(upsert, 4, risk);
            if (sqlite3_step(upsert) != SQLITE_DONE) {
                sqlite3_finalize(anchors); sqlite3_finalize(upsert);
                OCTDestroyCoordinateTransformation(projection);
                throw std::runtime_error("cannot persist per-hex LST");
            }
        }

        sqlite3_exec(database, "COMMIT;", nullptr, nullptr, nullptr);
        sqlite3_finalize(anchors);
        sqlite3_finalize(upsert);
        OCTDestroyCoordinateTransformation(projection);
    }
};

}  // namespace ppx::eco_restoration
