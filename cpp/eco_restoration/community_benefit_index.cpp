// File: cpp/eco_restoration/community_benefit_index.cpp

#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <sqlite3.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace eco_restoration {

struct CommunityNormalization {
    double income_low{};
    double income_high{};
    double vulnerability_low{};
    double vulnerability_high{};
    double green_access_low{};
    double green_access_high{};
    double income_weight{0.35};
    double vulnerability_weight{0.40};
    double green_access_weight{0.25};
};

double normalize(double value, double low, double high) {
    if (high <= low) throw std::invalid_argument("normalization range is invalid");
    return std::clamp((value - low) / (high - low), 0.0, 1.0);
}

double community_benefit_score(
    double median_income,
    double health_vulnerability,
    double green_access,
    const CommunityNormalization& normal) {

    const double income_need = 1.0 - normalize(median_income, normal.income_low, normal.income_high);
    const double vulnerability_need = normalize(
        health_vulnerability, normal.vulnerability_low, normal.vulnerability_high);
    const double green_need = 1.0 - normalize(
        green_access, normal.green_access_low, normal.green_access_high);

    const double weight_sum =
        normal.income_weight + normal.vulnerability_weight + normal.green_access_weight;
    if (weight_sum <= 0.0) throw std::invalid_argument("community weights must be positive");

    return std::clamp(
        (normal.income_weight * income_need +
         normal.vulnerability_weight * vulnerability_need +
         normal.green_access_weight * green_need) / weight_sum,
        0.0, 1.0);
}

void build_community_benefit_table(
    const std::string& hex_vector_path,
    const std::string& tract_vector_path,
    sqlite3* database,
    const CommunityNormalization& normal) {

    GDALAllRegister();
    GDALDataset* hexes = static_cast<GDALDataset*>(
        GDALOpenEx(hex_vector_path.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
    GDALDataset* tracts = static_cast<GDALDataset*>(
        GDALOpenEx(tract_vector_path.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));

    if (hexes == nullptr || tracts == nullptr || hexes->GetLayerCount() < 1 || tracts->GetLayerCount() < 1) {
        if (hexes != nullptr) GDALClose(hexes);
        if (tracts != nullptr) GDALClose(tracts);
        throw std::runtime_error("cannot open hex or tract vector layer");
    }

    OGRLayer* hex_layer = hexes->GetLayer(0);
    OGRLayer* tract_layer = tracts->GetLayer(0);
    const int anchor_field = hex_layer->GetLayerDefn()->GetFieldIndex("hex_anchor");
    const int income_field = tract_layer->GetLayerDefn()->GetFieldIndex("median_income");
    const int vulnerability_field = tract_layer->GetLayerDefn()->GetFieldIndex("health_vulnerability");
    const int green_field = tract_layer->GetLayerDefn()->GetFieldIndex("green_access");

    if (anchor_field < 0 || income_field < 0 || vulnerability_field < 0 || green_field < 0) {
        GDALClose(hexes);
        GDALClose(tracts);
        throw std::runtime_error("required hex or tract fields are absent");
    }

    sqlite3_exec(database,
        "CREATE TABLE IF NOT EXISTS community_benefit_score("
        "hex_anchor INTEGER PRIMARY KEY,median_income REAL NOT NULL,"
        "health_vulnerability REAL NOT NULL,green_access REAL NOT NULL,"
        "community_benefit REAL NOT NULL CHECK(community_benefit BETWEEN 0 AND 1)"
        ") STRICT;",
        nullptr, nullptr, nullptr);

    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT INTO community_benefit_score VALUES(?,?,?,?,?) "
        "ON CONFLICT(hex_anchor) DO UPDATE SET median_income=excluded.median_income,"
        "health_vulnerability=excluded.health_vulnerability,green_access=excluded.green_access,"
        "community_benefit=excluded.community_benefit;",
        -1, &statement, nullptr);

    hex_layer->ResetReading();
    while (OGRFeature* hex = hex_layer->GetNextFeature()) {
        OGRGeometry* hex_geometry = hex->GetGeometryRef();
        const double hex_area = hex_geometry == nullptr ? 0.0 : hex_geometry->get_Area();
        if (hex_area <= 0.0) {
            OGRFeature::DestroyFeature(hex);
            continue;
        }

        double income = 0.0;
        double vulnerability = 0.0;
        double green = 0.0;
        double overlap = 0.0;

        tract_layer->ResetReading();
        while (OGRFeature* tract = tract_layer->GetNextFeature()) {
            OGRGeometry* tract_geometry = tract->GetGeometryRef();
            if (tract_geometry != nullptr && hex_geometry->Intersects(tract_geometry)) {
                std::unique_ptr<OGRGeometry, decltype(&OGRGeometryFactory::destroyGeometry)> intersection(
                    hex_geometry->Intersection(tract_geometry), OGRGeometryFactory::destroyGeometry);
                const double weight = intersection == nullptr ? 0.0 : intersection->get_Area() / hex_area;
                income += weight * tract->GetFieldAsDouble(income_field);
                vulnerability += weight * tract->GetFieldAsDouble(vulnerability_field);
                green += weight * tract->GetFieldAsDouble(green_field);
                overlap += weight;
            }
            OGRFeature::DestroyFeature(tract);
        }

        if (overlap > 0.0) {
            income /= overlap;
            vulnerability /= overlap;
            green /= overlap;
            const auto anchor = static_cast<sqlite3_int64>(hex->GetFieldAsInteger64(anchor_field));
            sqlite3_bind_int64(statement, 1, anchor);
            sqlite3_bind_double(statement, 2, income);
            sqlite3_bind_double(statement, 3, vulnerability);
            sqlite3_bind_double(statement, 4, green);
            sqlite3_bind_double(statement, 5, community_benefit_score(income, vulnerability, green, normal));
            if (sqlite3_step(statement) != SQLITE_DONE) {
                sqlite3_finalize(statement);
                OGRFeature::DestroyFeature(hex);
                GDALClose(hexes);
                GDALClose(tracts);
                throw std::runtime_error("cannot persist community benefit score");
            }
            sqlite3_reset(statement);
            sqlite3_clear_bindings(statement);
        }
        OGRFeature::DestroyFeature(hex);
    }

    sqlite3_finalize(statement);
    GDALClose(hexes);
    GDALClose(tracts);
}

}  // namespace eco_restoration
