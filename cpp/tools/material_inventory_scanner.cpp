// File: cpp/tools/material_inventory_scanner.cpp
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <iomanip>
#include <cmath>

// material_inventory_scanner:
// - Simulates interfacing with a USB barcode scanner.
// - Looks up MaterialProperties from a local in-memory "SQLite-like" store.
// - Computes an eco-score for the material.
// - Prints a label via a thermal-printer-style text output.
//
// This implementation avoids external USB/SQLite/printer libraries but preserves
// the control-flow and eco-restoration logic.

namespace eco {

struct MaterialProperties {
    std::string material_id;
    std::string name;
    bool recyclable;
    bool biodegradable;
    double embodied_energy_MJ_per_kg;
    double toxicity_index;     // 0..1, higher = more toxic
};

class MaterialDatabase {
public:
    MaterialDatabase() {
        // Seed with some example materials; in a real system, load from SQLite.
        add_material({"MAT-001", "PET Plastic Bottle", true, false, 80.0, 0.6});
        add_material({"MAT-002", "Glass Jar",          true, true, 25.0, 0.1});
        add_material({"MAT-003", "Aluminum Can",       true, true, 50.0, 0.2});
        add_material({"MAT-004", "Compostable PLA",    true, true, 60.0, 0.3});
    }

    void add_material(const MaterialProperties& m) {
        db_[m.material_id] = m;
    }

    const MaterialProperties* lookup(const std::string& material_id) const {
        auto it = db_.find(material_id);
        if (it == db_.end()) {
            return nullptr;
        }
        return &it->second;
    }

private:
    std::unordered_map<std::string, MaterialProperties> db_;
};

class EcoScoreCalculator {
public:
    // Compute eco-score (0..1) based on material properties.
    double compute(const MaterialProperties& m) const {
        // Lower embodied energy and toxicity increase score.
        double energy_term = 1.0 - std::min(m.embodied_energy_MJ_per_kg / 100.0, 1.0);
        double toxicity_term = 1.0 - m.toxicity_index;

        double recyclability_term = m.recyclable ? 1.0 : 0.3;
        double biodegradability_term = m.biodegradable ? 1.0 : 0.4;

        double eco_score =
            0.3 * energy_term +
            0.2 * toxicity_term +
            0.25 * recyclability_term +
            0.25 * biodegradability_term;

        if (eco_score < 0.0) eco_score = 0.0;
        if (eco_score > 1.0) eco_score = 1.0;
        return eco_score;
    }
};

// Simulated USB barcode scanner: returns material IDs from a list.
class USBBarcodeScanner {
public:
    USBBarcodeScanner() : idx_(0) {
        scanned_ids_ = {"MAT-001", "MAT-002", "MAT-004", "MAT-003"};
    }

    bool scan(std::string& material_id) {
        if (idx_ >= scanned_ids_.size()) {
            return false;
        }
        material_id = scanned_ids_[idx_++];
        return true;
    }

private:
    std::vector<std::string> scanned_ids_;
    std::size_t idx_;
};

// Simulated thermal printer: prints a label as text.
class ThermalPrinter {
public:
    void print_label(const MaterialProperties& m, double eco_score) {
        std::cout << "================ MATERIAL ECO LABEL ================\n";
        std::cout << "ID:   " << m.material_id << "\n";
        std::cout << "Name: " << m.name << "\n";
        std::cout << "Eco-score: " << std::fixed << std::setprecision(2) << eco_score << " (0..1)\n";
        std::cout << "Recyclable:     " << (m.recyclable ? "YES" : "NO") << "\n";
        std::cout << "Biodegradable:  " << (m.biodegradable ? "YES" : "NO") << "\n";
        std::cout << "Embodied energy: " << m.embodied_energy_MJ_per_kg << " MJ/kg\n";
        std::cout << "Toxicity index:  " << m.toxicity_index << " (0..1)\n";
        std::cout << "===================================================\n";
    }
};

class MaterialInventoryScanner {
public:
    MaterialInventoryScanner()
        : db_(), eco_calc_(), scanner_(), printer_() {}

    void run() {
        std::cout << "Starting material inventory scanner.\n";
        std::string mat_id;
        while (scanner_.scan(mat_id)) {
            std::cout << "\nScanned barcode: " << mat_id << "\n";
            const MaterialProperties* m = db_.lookup(mat_id);
            if (!m) {
                std::cout << "Material not found in local database.\n";
                continue;
            }
            double eco_score = eco_calc_.compute(*m);
            printer_.print_label(*m, eco_score);
        }
        std::cout << "Material inventory scanning complete.\n";
    }

private:
    MaterialDatabase      db_;
    EcoScoreCalculator    eco_calc_;
    USBBarcodeScanner     scanner_;
    ThermalPrinter        printer_;
};

} // namespace eco

int main() {
    using namespace eco;

    MaterialInventoryScanner scanner;
    scanner.run();

    return 0;
}
