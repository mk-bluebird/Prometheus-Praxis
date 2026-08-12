// File: cpp/tools/ppx_geotiff_heat_risk_ingest.cpp
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include <gdal_priv.h>
#include <ogr_spatialref.h>
#include <sqlite3.h>

namespace ppx::eco_restoration {

struct SqliteCloser {
    void operator()(sqlite3* db) const { if (db) sqlite3_close(db); }
};
struct DatasetCloser {
    void operator()(GDALDataset* dataset) const { if (dataset) GDALClose(dataset); }
};
struct StatementCloser {
    void operator()(sqlite3_stmt* statement) const { if (statement) sqlite3_finalize(statement); }
};
struct TransformCloser {
    void operator()(OGRCoordinateTransformation* transform) const {
        if (transform) OCTDestroyCoordinateTransformation(transform);
    }
};

using Database = std::unique_ptr<sqlite3, SqliteCloser>;
using Dataset = std::unique_ptr<GDALDataset, DatasetCloser>;
using Statement = std::unique_ptr<sqlite3_stmt, StatementCloser>;
using Transform = std::unique_ptr<OGRCoordinateTransformation, TransformCloser>;

void require_sqlite(int code, sqlite3* db, const char* operation) {
    if (code != SQLITE_OK && code != SQLITE_DONE && code != SQLITE_ROW) {
        throw std::runtime_error(std::string(operation) + ": " + sqlite3_errmsg(db));
    }
}

double heat_risk(double lst_c, double reference_c, double range_c) {
    if (!std::isfinite(lst_c) || range_c <= 0.0) throw std::invalid_argument("invalid LST calibration");
    return std::clamp((lst_c - reference_c) / range_c, 0.0, 1.0);
}

void ingest_heat_risk(const char* geotiff, const char* database_path, double reference_c,
                      double range_c, const char* observed_utc) {
    GDALAllRegister();
    Dataset raster(static_cast<GDALDataset*>(GDALOpen(geotiff, GA_ReadOnly)));
    if (!raster) throw std::runtime_error("unable to open GeoTIFF");

    double geo[6]{};
    if (raster->GetGeoTransform(geo) != CE_None) throw std::runtime_error("GeoTIFF lacks geotransform");
    double inverse_geo[6]{};
    if (!GDALInvGeoTransform(geo, inverse_geo)) throw std::runtime_error("geotransform is not invertible");

    OGRSpatialReference wgs84;
    wgs84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    wgs84.SetWellKnownGeogCS("WGS84");
    const OGRSpatialReference* raster_srs = raster->GetSpatialRef();
    if (!raster_srs) throw std::runtime_error("GeoTIFF lacks coordinate reference system");
    Transform transform(OGRCreateCoordinateTransformation(&wgs84, raster_srs));
    if (!transform) throw std::runtime_error("cannot create WGS84-to-raster transformation");

    GDALRasterBand* band = raster->GetRasterBand(1);
    if (!band) throw std::runtime_error("GeoTIFF lacks first raster band");
    int has_nodata = 0;
    const double nodata = band->GetNoDataValue(&has_nodata);

    sqlite3* raw_db = nullptr;
    require_sqlite(sqlite3_open_v2(database_path, &raw_db, SQLITE_OPEN_READWRITE, nullptr), raw_db, "open database");
    Database db(raw_db);
    require_sqlite(sqlite3_exec(db.get(),
        "CREATE TABLE IF NOT EXISTS ppx_heat_risk("
        "hex_anchor INTEGER NOT NULL,observed_utc TEXT NOT NULL,lst_c REAL NOT NULL,"
        "r_heat REAL NOT NULL CHECK(r_heat BETWEEN 0.0 AND 1.0),"
        "PRIMARY KEY(hex_anchor,observed_utc)) STRICT;", nullptr, nullptr, nullptr), db.get(), "create heat table");

    sqlite3_stmt* raw_select = nullptr;
    require_sqlite(sqlite3_prepare_v2(db.get(), "SELECT hex_anchor,latitude_deg,longitude_deg FROM ppx_hex_anchor;",
        -1, &raw_select, nullptr), db.get(), "prepare anchor selection");
    Statement select(raw_select);

    sqlite3_stmt* raw_insert = nullptr;
    require_sqlite(sqlite3_prepare_v2(db.get(),
        "INSERT INTO ppx_heat_risk(hex_anchor,observed_utc,lst_c,r_heat) VALUES(?,?,?,?) "
        "ON CONFLICT(hex_anchor,observed_utc) DO UPDATE SET lst_c=excluded.lst_c,r_heat=excluded.r_heat;",
        -1, &raw_insert, nullptr), db.get(), "prepare heat insert");
    Statement insert(raw_insert);

    require_sqlite(sqlite3_exec(db.get(), "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr), db.get(), "begin transaction");
    while (sqlite3_step(select.get()) == SQLITE_ROW) {
        double longitude = sqlite3_column_double(select.get(), 2);
        double latitude = sqlite3_column_double(select.get(), 1);
        if (!transform->Transform(1, &longitude, &latitude)) continue;

        double pixel = 0.0, line = 0.0;
        GDALApplyGeoTransform(inverse_geo, longitude, latitude, &pixel, &line);
        const int x = static_cast<int>(std::floor(pixel));
        const int y = static_cast<int>(std::floor(line));
        if (x < 0 || y < 0 || x >= raster->GetRasterXSize() || y >= raster->GetRasterYSize()) continue;

        double lst_c = 0.0;
        if (band->RasterIO(GF_Read, x, y, 1, 1, &lst_c, 1, 1, GDT_Float64, 0, 0) != CE_None) continue;
        if ((has_nodata && lst_c == nodata) || !std::isfinite(lst_c)) continue;

        sqlite3_reset(insert.get());
        sqlite3_bind_int64(insert.get(), 1, sqlite3_column_int64(select.get(), 0));
        sqlite3_bind_text(insert.get(), 2, observed_utc, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(insert.get(), 3, lst_c);
        sqlite3_bind_double(insert.get(), 4, heat_risk(lst_c, reference_c, range_c));
        require_sqlite(sqlite3_step(insert.get()), db.get(), "insert heat risk");
    }
    require_sqlite(sqlite3_exec(db.get(), "COMMIT;", nullptr, nullptr, nullptr), db.get(), "commit transaction");
}

}  // namespace ppx::eco_restoration

int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cerr << "Usage: ppx_geotiff_heat_risk RASTER.tif DB.sqlite T_REF_C T_RANGE_C UTC\n";
        return EXIT_FAILURE;
    }
    try {
        ppx::eco_restoration::ingest_heat_risk(
            argv[1], argv[2], std::stod(argv[3]), std::stod(argv[4]), argv[5]);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
