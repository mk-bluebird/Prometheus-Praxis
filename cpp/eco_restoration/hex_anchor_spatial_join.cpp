// File: cpp/eco_restoration/hex_anchor_spatial_join.cpp
#include <gdal_priv.h>
#include <ogrsf_frmts.h>

#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

struct LayerAggregate {
    std::string layer;
    std::size_t feature_count{};
    double overlap_area_m2{};
    double overlap_length_m{};
};

using DatasetPtr = std::unique_ptr<GDALDataset, decltype(&GDALClose)>;
using GeometryPtr = std::unique_ptr<OGRGeometry, decltype(&OGRGeometryFactory::destroyGeometry)>;

std::vector<LayerAggregate> join_hex_anchor(
    GDALDataset& dataset, const std::string& anchor_id,
    const std::string& anchor_layer_name = "hex_anchor") {
    OGRLayer* anchors = dataset.GetLayerByName(anchor_layer_name.c_str());
    if (anchors == nullptr) throw std::runtime_error("hex anchor layer not found");

    anchors->SetAttributeFilter(("anchor_id = '" + anchor_id + "'").c_str());
    OGRFeature* raw_anchor = anchors->GetNextFeature();
    if (raw_anchor == nullptr) throw std::runtime_error("anchor_id not found");
    std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)>
        anchor(raw_anchor, &OGRFeature::DestroyFeature);
    const OGRGeometry* source = anchor->GetGeometryRef();
    if (source == nullptr || source->IsEmpty()) throw std::runtime_error("anchor geometry is empty");

    GeometryPtr hex(source->clone(), &OGRGeometryFactory::destroyGeometry);
    std::vector<LayerAggregate> results;
    for (int index = 0; index < dataset.GetLayerCount(); ++index) {
        OGRLayer* layer = dataset.GetLayer(index);
        if (layer == anchors || layer == nullptr) continue;

        layer->SetSpatialFilter(hex.get());
        LayerAggregate aggregate{layer->GetName()};
        layer->ResetReading();
        while (OGRFeature* raw = layer->GetNextFeature()) {
            std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)>
                feature(raw, &OGRFeature::DestroyFeature);
            const OGRGeometry* geometry = feature->GetGeometryRef();
            if (geometry == nullptr || geometry->IsEmpty() || !geometry->Intersects(hex.get())) continue;
            GeometryPtr overlap(geometry->Intersection(hex.get()), &OGRGeometryFactory::destroyGeometry);
            if (overlap == nullptr || overlap->IsEmpty()) continue;
            ++aggregate.feature_count;
            aggregate.overlap_area_m2 += overlap->get_Area();
            aggregate.overlap_length_m += overlap->Length();
        }
        layer->SetSpatialFilter(nullptr);
        results.push_back(std::move(aggregate));
    }
    anchors->SetAttributeFilter(nullptr);
    return results;
}

}  // namespace eco_restoration

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: hex_anchor_spatial_join <geopackage> <anchor_id>\n";
        return 2;
    }
    GDALAllRegister();
    eco_restoration::DatasetPtr dataset(
        GDALDataset::Open(argv[1], GDAL_OF_VECTOR | GDAL_OF_READONLY),
        &GDALClose);
    if (!dataset) {
        std::cerr << "cannot open GeoPackage\n";
        return 1;
    }
    try {
        const auto results = eco_restoration::join_hex_anchor(*dataset, argv[2]);
        std::cout << std::fixed << std::setprecision(3) << "{\"anchor_id\":\"" << argv[2]
                  << "\",\"layers\":[";
        for (std::size_t i = 0; i < results.size(); ++i) {
            const auto& r = results[i];
            std::cout << (i == 0 ? "" : ",") << "{\"layer\":\"" << r.layer
                      << "\",\"feature_count\":" << r.feature_count
                      << ",\"overlap_area_m2\":" << r.overlap_area_m2
                      << ",\"overlap_length_m\":" << r.overlap_length_m << "}";
        }
        std::cout << "]}\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
