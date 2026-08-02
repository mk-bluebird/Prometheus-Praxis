// File: cpp/tools/csv_material_loader.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace eco {

struct MaterialRecord {
    std::string name;
    double density_kg_m3;
    double embodied_energy_MJ_kg;
    double biodegradation_half_life_days;
    double toxicity_index;
    double recyclable_fraction;
};

class CsvMaterialLoader {
public:
    std::vector<MaterialRecord> load(const std::string &path) const {
        std::ifstream file(path);
        std::vector<MaterialRecord> materials;
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << path << "\n";
            return materials;
        }
        std::string line;
        bool first = true;
        while (std::getline(file, line)) {
            if (first) { first = false; continue; }
            std::stringstream ss(line);
            MaterialRecord rec{};
            std::string token;

            std::getline(ss, rec.name, ',');
            std::getline(ss, token, ','); rec.density_kg_m3 = std::stod(token);
            std::getline(ss, token, ','); rec.embodied_energy_MJ_kg = std::stod(token);
            std::getline(ss, token, ','); rec.biodegradation_half_life_days = std::stod(token);
            std::getline(ss, token, ','); rec.toxicity_index = std::stod(token);
            std::getline(ss, token, ','); rec.recyclable_fraction = std::stod(token);

            materials.push_back(rec);
        }
        return materials;
    }
};

} // namespace eco

int main() {
    eco::CsvMaterialLoader loader;
    auto materials = loader.load("materials.csv");
    std::cout << "Loaded " << materials.size() << " materials.\n";
    for (const auto &m : materials) {
        std::cout << "  " << m.name << " density: " << m.density_kg_m3 << "\n";
    }
    return 0;
}
