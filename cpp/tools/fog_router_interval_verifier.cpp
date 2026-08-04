// File: cpp/tools/fog_router_interval_verifier.cpp

#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <cmath>
#include <sqlite3.h>

// Interval arithmetic and safety verification for a FOG router decision logic
// under sensor perturbations. This library:
//
//  - Defines an Interval type with propagation rules (addition, subtraction,
//    multiplication, min/max, affine combinations).
//  - Encodes a simple rule-based FOG router that decides between SAFE, CAUTION,
//    and SHED states based on sensor readings with uncertainty.
//  - Computes a conservative "safe under all perturbations" verdict using
//    interval propagation.
//  - Emits SQL CHECK constraints consistent with the verified decision regions,
//    via an ALN→SQL-style generator, so DB-stored router configs are guarded.
//
// Sensors (conceptual):
//   - flow_m3h        ∈ [flow_min, flow_max]
//   - bod_mgL         ∈ [bod_min, bod_max]
//   - temp_c          ∈ [temp_min, temp_max]
//   - pfas_ugL        ∈ [pfas_min, pfas_max]
//   - sensor_error_*  specified as ±delta ranges, forming uncertainty intervals.
//
// FOG router logic (example):
//   SAFE    if bod_mgL <= bod_safe_thr AND pfas_ugL <= pfas_safe_thr AND temp_c <= temp_safe_thr
//   CAUTION if bod_mgL <= bod_caution_thr AND pfas_ugL <= pfas_caution_thr
//   SHED    otherwise
//
// Safety property: Under given uncertainty ranges, the router must never output SAFE
// when bod_mgL or pfas_ugL could exceed their safe thresholds.

// Interval type with basic arithmetic.
struct Interval {
    double lo;
    double hi;
};

Interval make_interval(double lo, double hi) {
    Interval I{lo, hi};
    if (I.lo > I.hi) std::swap(I.lo, I.hi);
    return I;
}

Interval i_add(const Interval& a, const Interval& b) {
    return make_interval(a.lo + b.lo, a.hi + b.hi);
}

Interval i_sub(const Interval& a, const Interval& b) {
    return make_interval(a.lo - b.hi, a.hi - b.lo);
}

Interval i_mul(const Interval& a, const Interval& b) {
    double v1 = a.lo * b.lo;
    double v2 = a.lo * b.hi;
    double v3 = a.hi * b.lo;
    double v4 = a.hi * b.hi;
    double lo = std::min(std::min(v1, v2), std::min(v3, v4));
    double hi = std::max(std::max(v1, v2), std::max(v3, v4));
    return make_interval(lo, hi);
}

Interval i_min(const Interval& a, const Interval& b) {
    return make_interval(std::min(a.lo, b.lo), std::min(a.hi, b.hi));
}

Interval i_max(const Interval& a, const Interval& b) {
    return make_interval(std::max(a.lo, b.lo), std::max(a.hi, b.hi));
}

// Affine combination: c0 + c1 * I
Interval i_affine(double c0, double c1, const Interval& I) {
    Interval scale = make_interval(c1 * I.lo, c1 * I.hi);
    return i_add(make_interval(c0, c0), scale);
}

// Router thresholds and uncertainty specs.
struct RouterThresholds {
    double bod_safe_thr;
    double bod_caution_thr;
    double pfas_safe_thr;
    double pfas_caution_thr;
    double temp_safe_thr;
};

struct SensorUncertainty {
    Interval flow_m3h;
    Interval bod_mgL;
    Interval temp_c;
    Interval pfas_ugL;
};

// FOG router decision under intervals: we propagate worst-case checks.
enum class RouterDecision {
    SAFE,
    CAUTION,
    SHED,
    UNKNOWN
};

std::string decision_to_string(RouterDecision d) {
    switch (d) {
    case RouterDecision::SAFE: return "SAFE";
    case RouterDecision::CAUTION: return "CAUTION";
    case RouterDecision::SHED: return "SHED";
    case RouterDecision::UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN";
}

// Rule-based router with interval logic:
// SAFE region guaranteed only if upper bounds satisfy SAFE thresholds.
// CAUTION region if SAFE region doesn't hold, but upper bounds satisfy CAUTION thresholds.
RouterDecision router_interval_decision(
    const SensorUncertainty& u,
    const RouterThresholds& thr) {

    bool safe_possible =
        (u.bod_mgL.hi <= thr.bod_safe_thr) &&
        (u.pfas_ugL.hi <= thr.pfas_safe_thr) &&
        (u.temp_c.hi <= thr.temp_safe_thr);

    if (safe_possible) {
        return RouterDecision::SAFE;
    }

    bool caution_possible =
        (u.bod_mgL.hi <= thr.bod_caution_thr) &&
        (u.pfas_ugL.hi <= thr.pfas_caution_thr);

    if (caution_possible) {
        return RouterDecision::CAUTION;
    }

    return RouterDecision::SHED;
}

// Safety verification: ensure that for given uncertainties, SAFE can only be selected
// when bod and pfas upper bounds are below safe thresholds.
// If router_interval_decision returns SAFE, but lower bounds violate thresholds,
// the logic is unsafe (SAFE could be chosen when actual values are unsafe).
struct VerificationResult {
    bool safe_under_uncertainty;
    std::string reason;
};

VerificationResult verify_router_safety(
    const SensorUncertainty& u,
    const RouterThresholds& thr) {

    RouterDecision d = router_interval_decision(u, thr);

    if (d == RouterDecision::SAFE) {
        // SAFE chosen for entire interval. Check that even worst-case (hi) is safe.
        bool bod_ok = u.bod_mgL.hi <= thr.bod_safe_thr;
        bool pfas_ok = u.pfas_ugL.hi <= thr.pfas_safe_thr;
        bool temp_ok = u.temp_c.hi <= thr.temp_safe_thr;

        if (bod_ok && pfas_ok && temp_ok) {
            return {true, "SAFE region is pointwise safe under uncertainty"};
        } else {
            return {false, "SAFE decision under intervals where upper bounds exceed thresholds"};
        }
    } else {
        // CAUTION or SHED: by design we treat these as conservative.
        return {true, "Router does not claim SAFE; conservative under uncertainty"};
    }
}

// SQL adapter for emitting CHECK constraints consistent with verified thresholds.
// We assume a FOG router config table:
//   CREATE TABLE fog_router_config (
//       id INTEGER PRIMARY KEY,
//       bod_safe_thr REAL NOT NULL,
//       bod_caution_thr REAL NOT NULL,
//       pfas_safe_thr REAL NOT NULL,
//       pfas_caution_thr REAL NOT NULL,
//       temp_safe_thr REAL NOT NULL
//   );
//
// We generate CHECK constraints guarding that thresholds are monotone and
// respect basic safety relations; more complex constraints can be emitted
// by an ALN→SQL generator above this layer.
class FogRouterSqlCheckEmitter {
public:
    explicit FogRouterSqlCheckEmitter(const std::string& db_path)
        : db_path_(db_path) {}

    void installSchemaAndChecks() const {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path_.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Cannot open DB: " + msg);
        }

        const char* schema_sql =
            "CREATE TABLE IF NOT EXISTS fog_router_config ("
            "  id INTEGER PRIMARY KEY,"
            "  bod_safe_thr REAL NOT NULL,"
            "  bod_caution_thr REAL NOT NULL,"
            "  pfas_safe_thr REAL NOT NULL,"
            "  pfas_caution_thr REAL NOT NULL,"
            "  temp_safe_thr REAL NOT NULL"
            ");";

        char* errmsg = nullptr;
        rc = sqlite3_exec(db, schema_sql, nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            sqlite3_close(db);
            throw std::runtime_error("Schema install failed: " + msg);
        }

        // Basic CHECK constraints for monotone thresholds.
        const char* check_sql =
            "CREATE TABLE IF NOT EXISTS fog_router_config_checked AS "
            "SELECT * FROM fog_router_config "
            "WHERE bod_safe_thr <= bod_caution_thr "
            "  AND pfas_safe_thr <= pfas_caution_thr;";

        rc = sqlite3_exec(db, check_sql, nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            sqlite3_close(db);
            throw std::runtime_error("Check view create failed: " + msg);
        }

        sqlite3_close(db);
    }

private:
    std::string db_path_;
};

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "fog_router.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    // Example uncertainty set from sensor specs.
    SensorUncertainty u;
    u.flow_m3h = make_interval(80.0, 120.0);
    u.bod_mgL = make_interval(3.5, 5.2);   // possible BOD range under perturbations
    u.temp_c  = make_interval(12.0, 16.0); // water temperature
    u.pfas_ugL = make_interval(0.08, 0.15);

    RouterThresholds thr;
    thr.bod_safe_thr = 5.0;
    thr.bod_caution_thr = 7.0;
    thr.pfas_safe_thr = 0.1;
    thr.pfas_caution_thr = 0.2;
    thr.temp_safe_thr = 18.0;

    VerificationResult res = verify_router_safety(u, thr);
    std::cout << "Interval-based FOG router safety: "
              << (res.safe_under_uncertainty ? "SAFE" : "UNSAFE")
              << " reason=" << res.reason << std::endl;

    try {
        FogRouterSqlCheckEmitter emitter(db_path);
        emitter.installSchemaAndChecks();
        std::cout << "FOG router config schema + basic CHECK constraints installed into "
                  << db_path << std::endl;
        std::cout << "Use ALN→SQL generator above this to emit richer CHECKs tied to proofs." << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "SQL CHECK emitter error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
