// File: cpp/eco_restoration/hex_canopy_cover.cpp

#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <sqlite3.h>

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace eco_restoration {

struct HexCell {
    std::uint64_t anchor{};
    double center_x_m{};
    double center_y_m{};
    double edge_m{};
};

struct CanopyResult {
    std::uint64_t anchor{};
    double canopy_fraction{};
    std::uint64_t sampled_pixels{};
};

bool within_hex(double x, double y, const HexCell& hex) {
    const double dx = std::abs(x - hex.center_x_m);
    const double dy = std::abs(y - hex.center_y_m);
    return dx <= std::sqrt(3.0) * hex.edge_m * 0.5 &&
           std::sqrt(3.0) * dx + dy <= std::sqrt(3.0) * hex.edge_m;
}

std::vector<CanopyResult> estimate_canopy_cover(
    const std::string& imagery_path,
    const std::vector<HexCell>& hexes,
    int red_band_index,
    int nir_band_index,
    double ndvi_threshold) {

    GDALAllRegister();
    GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(imagery_path.c_str(), GA_ReadOnly));
    if (dataset == nullptr || red_band_index < 1 || nir_band_index < 1 ||
        red_band_index > dataset->GetRasterCount() || nir_band_index > dataset->GetRasterCount()) {
        if (dataset != nullptr) GDALClose(dataset);
        throw std::invalid_argument("imagery requires configured red and near-infrared bands");
    }

    const int width = dataset->GetRasterXSize();
    const int height = dataset->GetRasterYSize();
    double transform[6]{};
    if (dataset->GetGeoTransform(transform) != CE_None) {
        GDALClose(dataset);
        throw std::runtime_error("imagery requires a projected geotransform");
    }

    std::vector<double> red(static_cast<std::size_t>(width) * height);
    std::vector<double> nir(red.size());

    if (dataset->GetRasterBand(red_band_index)->RasterIO(
            GF_Read, 0, 0, width, height, red.data(), width, height, GDT_Float64, 0, 0) != CE_None ||
        dataset->GetRasterBand(nir_band_index)->RasterIO(
            GF_Read, 0, 0, width, height, nir.data(), width, height, GDT_Float64, 0, 0) != CE_None) {
        GDALClose(dataset);
        throw std::runtime_error("cannot read imagery bands");
    }
    GDALClose(dataset);

    std::vector<CanopyResult> results;
    for (const HexCell& hex : hexes) {
        std::uint64_t sampled = 0;
        std::uint64_t canopy = 0;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const double px = transform[0] + (x + 0.5) * transform[1] + (y + 0.5) * transform[2];
                const double py = transform[3] + (x + 0.5) * transform[4] + (y + 0.5) * transform[5];
                if (!within_hex(px, py, hex)) continue;

                const std::size_t index = static_cast<std::size_t>(y) * width + x;
                const double denominator = nir[index] + red[index];
                if (!std::isfinite(denominator) || std::abs(denominator) < 1e-12) continue;

                const double ndvi = (nir[index] - red[index]) / denominator;
                ++sampled;
                canopy += ndvi >= ndvi_threshold ? 1U : 0U;
            }
        }
        results.push_back({
            hex.anchor,
            sampled == 0U ? 0.0 : static_cast<double>(canopy) / static_cast<double>(sampled),
            sampled
        });
    }
    return results;
}

void persist_canopy_cover(sqlite3* database, const std::vector<CanopyResult>& results, std::int64_t observed_unix_s) {
    sqlite3_exec(database,
        "CREATE TABLE IF NOT EXISTS hex_canopy_cover("
        "hex_anchor INTEGER NOT NULL,observed_unix_s INTEGER NOT NULL,"
        "canopy_fraction REAL NOT NULL CHECK(canopy_fraction BETWEEN 0 AND 1),"
        "sampled_pixels INTEGER NOT NULL CHECK(sampled_pixels>=0),"
        "PRIMARY KEY(hex_anchor,observed_unix_s)) STRICT;",
        nullptr, nullptr, nullptr);

    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT INTO hex_canopy_cover VALUES(?,?,?,?) "
        "ON CONFLICT(hex_anchor,observed_unix_s) DO UPDATE SET "
        "canopy_fraction=excluded.canopy_fraction,sampled_pixels=excluded.sampled_pixels;",
        -1, &statement, nullptr);

    for (const CanopyResult& result : results) {
        sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(result.anchor));
        sqlite3_bind_int64(statement, 2, static_cast<sqlite3_int64>(observed_unix_s));
        sqlite3_bind_double(statement, 3, result.canopy_fraction);
        sqlite3_bind_int64(statement, 4, static_cast<sqlite3_int64>(result.sampled_pixels));
        if (sqlite3_step(statement) != SQLITE_DONE) {
            sqlite3_finalize(statement);
            throw std::runtime_error("cannot persist canopy cover");
        }
        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);
    }
    sqlite3_finalize(statement);
}

}  // namespace eco_restoration
