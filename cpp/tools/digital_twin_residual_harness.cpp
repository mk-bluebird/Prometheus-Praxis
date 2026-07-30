// File: cpp/tools/digital_twin_residual_harness.cpp
// Repo path: cpp/tools/digital_twin_residual_harness.cpp
//
// Purpose:
//   Concrete, non-actuating C++ harness that:
//     1. Reads production hex-anchored residual data from SQLite.
//     2. Builds a digital twin copy (in-memory or separate SQLite).
//     3. Computes Merkle roots for production and twin RiskVectors.
//     4. Runs residual improvement and KER band checks on twin
//        under proposed code changes.
//     5. Refuses merge unless:
//          - twin and production commitments (Merkle roots) line up,
//          - twin simulations show non-regression in V_t and K,E,R.
//
// Dependencies:
//   - libsqlite3
//   - risk_vector_merkle.cpp (for RiskVector, serialization, Merkle utilities)
//   - ker_residual_core.hpp (for KerResidual compute)

#include <sqlite3.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "../eco_restoration/risk_vector_merkle.cpp"  // assumes same directory structure
#include "../ecosafety_core_v2/cpp/ker_residual_core.hpp"

struct ResidualRow {
    std::string segment_id;
    std::string yyyymmdd;
    RiskVector  rv;
    double      vt_residual;
    double      K;
    double      E;
    double      R;
    std::string evidence_hex;
};

// Fetch production residual windows from SQLite.
// Assumes ker_residual_window table exists with canonical columns.
std::vector<ResidualRow> fetch_production_residuals(sqlite3* db) {
    std::vector<ResidualRow> rows;

    const char* sql =
        "SELECT segment_id, yyyymmdd,"
        "       r_energy, r_hydraulics, r_pfas, r_cold,"
        "       r_bod, r_tss, r_cec, r_carbon, r_biodiversity,"
        "       r_materials, r_neurorights, r_topology, r_dataquality, r_uncertainty,"
        "       vt_residual, k_knowledge, e_ecoimpact, r_risk, evidence_hex "
        "FROM ker_residual_window "
        "ORDER BY segment_id, yyyymmdd;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare ker_residual_window query");
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ResidualRow row;
        const char* seg  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        row.segment_id   = seg ? seg : "";
        row.yyyymmdd     = date ? date : "";

        RiskVector rv;
        rv.set(RiskPlane::ENERGY,       static_cast<float>(sqlite3_column_double(stmt, 2)));
        rv.set(RiskPlane::HYDRAULICS,   static_cast<float>(sqlite3_column_double(stmt, 3)));
        rv.set(RiskPlane::PFAS,         static_cast<float>(sqlite3_column_double(stmt, 4)));
        rv.set(RiskPlane::COLD,         static_cast<float>(sqlite3_column_double(stmt, 5)));
        rv.set(RiskPlane::BOD,          static_cast<float>(sqlite3_column_double(stmt, 6)));
        rv.set(RiskPlane::TSS,          static_cast<float>(sqlite3_column_double(stmt, 7)));
        rv.set(RiskPlane::CEC,          static_cast<float>(sqlite3_column_double(stmt, 8)));
        rv.set(RiskPlane::CARBON,       static_cast<float>(sqlite3_column_double(stmt, 9)));
        rv.set(RiskPlane::BIODIVERSITY, static_cast<float>(sqlite3_column_double(stmt, 10)));
        rv.set(RiskPlane::MATERIALS,    static_cast<float>(sqlite3_column_double(stmt, 11)));
        rv.set(RiskPlane::NEURORIGHTS,  static_cast<float>(sqlite3_column_double(stmt, 12)));
        rv.set(RiskPlane::TOPOLOGY,     static_cast<float>(sqlite3_column_double(stmt, 13)));
        rv.set(RiskPlane::DATAQUALITY,  static_cast<float>(sqlite3_column_double(stmt, 14)));
        rv.set(RiskPlane::UNCERTAINTY,  static_cast<float>(sqlite3_column_double(stmt, 15)));
        row.rv = rv;

        row.vt_residual = sqlite3_column_double(stmt, 16);
        row.K           = sqlite3_column_double(stmt, 17);
        row.E           = sqlite3_column_double(stmt, 18);
        row.R           = sqlite3_column_double(stmt, 19);

        const char* hex = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 20));
        row.evidence_hex = hex ? hex : "";

        rows.push_back(row);
    }

    sqlite3_finalize(stmt);
    return rows;
}

// Compute Merkle root of production RiskVectors.
std::array<uint8_t, 32> compute_production_merkle_root(const std::vector<ResidualRow>& rows) {
    std::vector<std::array<uint8_t, 32>> leaves;
    leaves.reserve(rows.size());
    for (const auto& row : rows) {
        leaves.push_back(risk_vector_leaf_hash(row.rv));
    }
    return build_merkle_root(leaves);
}

// Twin simulation: recompute vt, K,E,R using proposed residual kernel.
// Here we reuse the existing KerResidual kernel for demonstration,
// but in practice you would swap in the new code and keep the old
// results for comparison.
std::vector<ResidualRow> simulate_twin(const std::vector<ResidualRow>& prodRows,
                                       const ecosafety_core_v2::PlaneWeights& pw) {
    std::vector<ResidualRow> twinRows = prodRows;

    for (auto& row : twinRows) {
        // Build RiskVector in KER kernel format.
        ecosafety_core_v2::RiskVector rvKernel;
        // Map planes into kernel RiskVector.
        rvKernel.set(ecosafety_core_v2::RiskPlane::ENERGY,
                     static_cast<double>(row.rv.get(RiskPlane::ENERGY)));
        rvKernel.set(ecosafety_core_v2::RiskPlane::HYDRAULICS,
                     static_cast<double>(row.rv.get(RiskPlane::HYDRAULICS)));
        rvKernel.set(ecosafety_core_v2::RiskPlane::PFAS,
                     static_cast<double>(row.rv.get(RiskPlane::PFAS)));
        rvKernel.set(ecosafety_core_v2::RiskPlane::COLD,
                     static_cast<double>(row.rv.get(RiskPlane::COLD)));
        rvKernel.set(ecosafety_core_v2::RiskPlane::BOD,
                     static_cast<double>(row.rv.get(RiskPlane::BOD)));
        rvKernel.set(ecosafety_core_v2::RiskPlane::TSS,
                     static_cast<double>(row.rv.get(RiskPlane::TSS)));
        rvKernel.set(ecosafety_core_v2::RiskPlane::CEC,
                     static_cast<double>(row.rv.get(RiskPlane::CEC)));
        rvKernel.set(ecosafety_core_v2::RiskPlane::CARBON,
                     static_cast<double>(row.rv.get(RiskPlane::CARBON)));
        rvKernel.set(ecosafety_core_v2::RiskPlane::BIODIVERSITY,
                     static_cast<double>(row.rv.get(RiskPlane::BIODIVERSITY)));
        rvKernel.set(ecosafety_core_v2::RiskPlane::MATERIALS,
                     static_cast<double>(row.rv.get(RiskPlane::MATERIALS)));
        rvKernel.set(ecosafety_core_v2::RiskPlane::NEURORIGHTS,
                     static_cast<double>(row.rv.get(RiskPlane::NEURORIGHTS)));
        rvKernel.set(ecosafety_core_v2::RiskPlane::TOPOLOGY,
                     static_cast<double>(row.rv.get(RiskPlane::TOPOLOGY)));
        rvKernel.set(ecosafety_core_v2::RiskPlane::DATAQUALITY,
                     static_cast<double>(row.rv.get(RiskPlane::DATAQUALITY)));
        rvKernel.set(ecosafety_core_v2::RiskPlane::UNCERTAINTY,
                     static_cast<double>(row.rv.get(RiskPlane::UNCERTAINTY)));

        ecosafety_core_v2::KerResidual kr = ecosafety_core_v2::compute_ker_residual(rvKernel, pw);
        row.vt_residual = kr.vt;
        row.K           = kr.k;
        row.E           = kr.e;
        row.R           = kr.r;
        // evidence_hex unchanged; twin results are diagnostics only.
    }

    return twinRows;
}

// Compare production and twin commitments and residuals.
// Return true if twin shows non-regression and K,E,R in band.
bool check_twin_non_regression(const std::vector<ResidualRow>& prodRows,
                               const std::vector<ResidualRow>& twinRows) {
    if (prodRows.size() != twinRows.size()) {
        throw std::runtime_error("Production and twin row counts differ");
    }

    bool ok = true;
    const double eps = 1e-6;

    for (std::size_t i = 0; i < prodRows.size(); ++i) {
        const auto& p = prodRows[i];
        const auto& t = twinRows[i];

        // Residual non-regression: V_t^twin <= V_t^prod + eps.
        if (t.vt_residual > p.vt_residual + eps) {
            std::cerr << "Residual regression at segment " << p.segment_id
                      << " date " << p.yyyymmdd << ": twin Vt=" << t.vt_residual
                      << " > prod Vt=" << p.vt_residual << "\n";
            ok = false;
        }

        // K,E,R band checks (reuse RESEARCH/PILOT/PROD thresholds as needed).
        // For now, just check that twin does not worsen R.
        if (t.R > p.R + eps) {
            std::cerr << "Risk R worsened at segment " << p.segment_id
                      << " date " << p.yyyymmdd << ": twin R=" << t.R
                      << " > prod R=" << p.R << "\n";
            ok = false;
        }
    }

    return ok;
}

int main(int argc, char** argv) {
    const char* db_path = "dbcyboquaticdailyprogress.sqlite";
    if (argc > 1) {
        db_path = argv[1];
    }

    sqlite3* db = nullptr;
    if (sqlite3_open(db_path, &db) != SQLITE_OK) {
        std::cerr << "Failed to open SQLite DB: " << db_path << "\n";
        return 1;
    }

    try {
        // 1. Fetch production residuals.
        auto prodRows = fetch_production_residuals(db);
        if (prodRows.empty()) {
            std::cerr << "No ker_residual_window rows found.\n";
            sqlite3_close(db);
            return 1;
        }

        // 2. Compute production Merkle root.
        auto prodRoot = compute_production_merkle_root(prodRows);
        std::cout << "Production Merkle root: " << hash_to_hex(prodRoot) << "\n";

        // 3. Define plane weights for KER kernel (example).
        ecosafety_core_v2::PlaneWeights pw;
        for (std::size_t i = 0; i < ecosafety_core_v2::RISK_PLANE_COUNT; ++i) {
            pw.w[i] = 1.0; // equal weights; replace with calibrated values.
        }

        // 4. Simulate twin under proposed residual kernel.
        auto twinRows = simulate_twin(prodRows, pw);

        // 5. Compute twin Merkle root.
        auto twinRoot = compute_production_merkle_root(twinRows);
        std::cout << "Twin Merkle root: " << hash_to_hex(twinRoot) << "\n";

        // 6. Check commitments line up: for a pure diagnostic twin, we expect
        //    identical input risk vectors, so leaf hashes and roots should match.
        if (prodRoot != twinRoot) {
            std::cerr << "Twin and production Merkle roots differ; refusing merge.\n";
            sqlite3_close(db);
            return 1;
        }

        // 7. Check twin non-regression in residual and K,E,R bands.
        bool ok = check_twin_non_regression(prodRows, twinRows);
        if (!ok) {
            std::cerr << "Twin simulations show regression; refusing merge.\n";
            sqlite3_close(db);
            return 1;
        }

        std::cout << "Twin simulations show non-regression; changes may proceed to review.\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}
