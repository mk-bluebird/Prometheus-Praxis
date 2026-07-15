// filename: eco_restoration_shard/cyboquatic_progress/20260713/cpp/cyboquatic_workload_energyreq_dvt.cpp
// domain: (d) Cyboquatic workload (energyreqJ, ΔVt) in C++
// purpose: Non-actuating diagnostic helper to compute workload risk, Lyapunov-style residual,
//          and K,E,R scores, and log into cyboquatic_daily_progress.sqlite for Phoenix nodes.

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <stdexcept>
#include <sqlite3.h>

struct WorkloadRiskVector {
    double renergy;
    double rhydraulic;
    double runcertainty;

    static double clamp01(double x) {
        if (x < 0.0) return 0.0;
        if (x > 1.0) return 1.0;
        return x;
    }

    WorkloadRiskVector clamped() const {
        WorkloadRiskVector v;
        v.renergy = clamp01(renergy);
        v.rhydraulic = clamp01(rhydraulic);
        v.runcertainty = clamp01(runcertainty);
        return v;
    }

    double residual() const {
        const double WENERGY = 0.8;
        const double WHYDRAULIC = 1.0;
        const double WUNCERTAINTY = 0.6;
        return WENERGY * renergy * renergy
             + WHYDRAULIC * rhydraulic * rhydraulic
             + WUNCERTAINTY * runcertainty * runcertainty;
    }
};

struct WorkloadSample {
    std::string sample_id;
    std::string node_id;
    std::string timestamp_utc;
    double energy_req_j;
    double energy_surplus_j;
    double hydraulic_risk;
    double uncertainty_risk;
    WorkloadRiskVector risk;
    double vt_before;
    double vt_after;
    double delta_vt;
    double k_factor;
    double e_factor;
    double r_factor;
};

static WorkloadRiskVector normalize_risk(double energy_req_j,
                                         double energy_surplus_j,
                                         double hydraulic_risk,
                                         double uncertainty_risk) {
    const double ENERGY_TAILWIND_SAFE_RATIO = 1.2;
    const double ENERGY_MIN_RATIO = 0.0;
    const double ENERGY_MAX_RATIO = 2.5;

    double ratio;
    if (energy_req_j <= 0.0) {
        ratio = ENERGY_MAX_RATIO;
    } else {
        ratio = energy_surplus_j / energy_req_j;
    }

    double renergy_raw;
    if (ratio >= ENERGY_TAILWIND_SAFE_RATIO) {
        renergy_raw = 0.0;
    } else if (ratio <= ENERGY_MIN_RATIO) {
        renergy_raw = 1.0;
    } else {
        double bounded_ratio = ratio;
        if (bounded_ratio > ENERGY_MAX_RATIO) {
            bounded_ratio = ENERGY_MAX_RATIO;
        }
        double span = ENERGY_TAILWIND_SAFE_RATIO - ENERGY_MIN_RATIO;
        double rel = (bounded_ratio - ENERGY_MIN_RATIO) / span;
        renergy_raw = 1.0 - rel;
        if (renergy_raw < 0.0) renergy_raw = 0.0;
        if (renergy_raw > 1.0) renergy_raw = 1.0;
    }

    WorkloadRiskVector v;
    v.renergy = renergy_raw;
    v.rhydraulic = hydraulic_risk;
    v.runcertainty = uncertainty_risk;
    return v.clamped();
}

static void compute_ker(const WorkloadRiskVector &risk,
                        double delta_vt,
                        double &k_out,
                        double &e_out,
                        double &r_out) {
    double vt = risk.residual();

    double max_r = risk.renergy;
    if (risk.rhydraulic > max_r) max_r = risk.rhydraulic;
    if (risk.runcertainty > max_r) max_r = risk.runcertainty;

    double k = 0.95 - 0.4 * max_r;
    if (delta_vt > 0.0) {
        k -= 0.25;
    }
    if (k < 0.0) k = 0.0;
    if (k > 1.0) k = 1.0;

    double e = 0.95 - vt;
    if (delta_vt > 0.0) {
        e -= 0.3;
    }
    if (e < 0.0) e = 0.0;
    if (e > 1.0) e = 1.0;

    double r = vt;
    if (delta_vt > 0.0) {
        r += delta_vt;
    }
    if (r < 0.0) r = 0.0;
    if (r > 1.0) r = 1.0;

    k_out = k;
    e_out = e;
    r_out = r;
}

static WorkloadSample make_sample(const std::string &sample_id,
                                  const std::string &node_id,
                                  const std::string &timestamp_utc,
                                  double energy_req_j,
                                  double energy_surplus_j,
                                  double hydraulic_risk,
                                  double uncertainty_risk,
                                  double vt_before) {
    WorkloadSample s;
    s.sample_id = sample_id;
    s.node_id = node_id;
    s.timestamp_utc = timestamp_utc;
    s.energy_req_j = energy_req_j;
    s.energy_surplus_j = energy_surplus_j;
    s.hydraulic_risk = hydraulic_risk;
    s.uncertainty_risk = uncertainty_risk;

    WorkloadRiskVector risk_normalized = normalize_risk(
        energy_req_j,
        energy_surplus_j,
        hydraulic_risk,
        uncertainty_risk
    );
    s.risk = risk_normalized;

    s.vt_before = (vt_before < 0.0) ? 0.0 : vt_before;
    s.vt_after = s.risk.residual();
    s.delta_vt = s.vt_after - s.vt_before;

    compute_ker(s.risk, s.delta_vt, s.k_factor, s.e_factor, s.r_factor);
    return s;
}

static void ensure_daily_progress_schema(sqlite3 *db) {
    const char *sql =
        "PRAGMA foreign_keys=ON;"
        "CREATE TABLE IF NOT EXISTS daily_progress ("
        "  progress_id      INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  yyyymmdd         TEXT NOT NULL,"
        "  domain           TEXT NOT NULL,"
        "  subtask_id       TEXT NOT NULL,"
        "  node_id          TEXT NOT NULL,"
        "  sample_id        TEXT NOT NULL,"
        "  timestamp_utc    TEXT NOT NULL,"
        "  energy_req_j     REAL NOT NULL,"
        "  energy_surplus_j REAL NOT NULL,"
        "  hydraulic_risk   REAL NOT NULL,"
        "  uncertainty_risk REAL NOT NULL,"
        "  renergy          REAL NOT NULL,"
        "  rhydraulic       REAL NOT NULL,"
        "  runcertainty     REAL NOT NULL,"
        "  vt_before        REAL NOT NULL,"
        "  vt_after         REAL NOT NULL,"
        "  delta_vt         REAL NOT NULL,"
        "  k_factor         REAL NOT NULL,"
        "  e_factor         REAL NOT NULL,"
        "  r_factor         REAL NOT NULL,"
        "  phoenix_hex      TEXT NOT NULL,"
        "  prior_pointer    TEXT NOT NULL"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_daily_progress_date "
        "  ON daily_progress(yyyymmdd);"
        "CREATE INDEX IF NOT EXISTS idx_daily_progress_node_time "
        "  ON daily_progress(node_id, timestamp_utc);";

    char *errmsg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "unknown error";
        sqlite3_free(errmsg);
        throw std::runtime_error("Schema migration failed: " + msg);
    }
}

static void insert_daily_progress(sqlite3 *db,
                                  const WorkloadSample &s,
                                  const std::string &yyyymmdd,
                                  const std::string &subtask_id,
                                  const std::string &phoenix_hex,
                                  const std::string &prior_pointer) {
    const char *sql =
        "INSERT INTO daily_progress ("
        "  yyyymmdd, domain, subtask_id, "
        "  node_id, sample_id, timestamp_utc, "
        "  energy_req_j, energy_surplus_j, "
        "  hydraulic_risk, uncertainty_risk, "
        "  renergy, rhydraulic, runcertainty, "
        "  vt_before, vt_after, delta_vt, "
        "  k_factor, e_factor, r_factor, "
        "  phoenix_hex, prior_pointer"
        ") VALUES ("
        "  ?, ?, ?, "
        "  ?, ?, ?, "
        "  ?, ?, "
        "  ?, ?, "
        "  ?, ?, ?, "
        "  ?, ?, ?, "
        "  ?, ?, ?, "
        "  ?, ?"
        ");";

    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Prepare failed: " + std::string(sqlite3_errmsg(db)));
    }

    int idx = 1;
    sqlite3_bind_text(stmt, idx++, yyyymmdd.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, "workload_energy_dvt", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, subtask_id.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_bind_text(stmt, idx++, s.node_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, s.sample_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, s.timestamp_utc.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_bind_double(stmt, idx++, s.energy_req_j);
    sqlite3_bind_double(stmt, idx++, s.energy_surplus_j);

    sqlite3_bind_double(stmt, idx++, s.hydraulic_risk);
    sqlite3_bind_double(stmt, idx++, s.uncertainty_risk);

    sqlite3_bind_double(stmt, idx++, s.risk.renergy);
    sqlite3_bind_double(stmt, idx++, s.risk.rhydraulic);
    sqlite3_bind_double(stmt, idx++, s.risk.runcertainty);

    sqlite3_bind_double(stmt, idx++, s.vt_before);
    sqlite3_bind_double(stmt, idx++, s.vt_after);
    sqlite3_bind_double(stmt, idx++, s.delta_vt);

    sqlite3_bind_double(stmt, idx++, s.k_factor);
    sqlite3_bind_double(stmt, idx++, s.e_factor);
    sqlite3_bind_double(stmt, idx++, s.r_factor);

    sqlite3_bind_text(stmt, idx++, phoenix_hex.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, prior_pointer.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Insert failed: " + std::string(sqlite3_errmsg(db)));
    }

    sqlite3_finalize(stmt);
}

// CLI:
//   cyboquatic_workload_energyreq_dvt <db_path> <node_id> <sample_id> <timestamp_utc>
//                                     <energy_req_j> <energy_surplus_j>
//                                     <hydraulic_risk> <uncertainty_risk> <vt_before>
int main(int argc, char **argv) {
    if (argc != 10) {
        std::cerr << "Usage: " << argv[0]
                  << " <db_path> <node_id> <sample_id> <timestamp_utc>"
                  << " <energy_req_j> <energy_surplus_j>"
                  << " <hydraulic_risk> <uncertainty_risk> <vt_before>\n";
        return 1;
    }

    std::string db_path = argv[1];
    std::string node_id = argv[2];
    std::string sample_id = argv[3];
    std::string timestamp_utc = argv[4];
    double energy_req_j = std::atof(argv[5]);
    double energy_surplus_j = std::atof(argv[6]);
    double hydraulic_risk = std::atof(argv[7]);
    double uncertainty_risk = std::atof(argv[8]);
    double vt_before = std::atof(argv[9]);

    const std::string yyyymmdd = "20260713";
    const std::string subtask_id = "PHX-CANAL-WL-2026-07-13";
    const std::string phoenix_hex = "0x20260713PHX3345NWorkloadEnergyDeltaVtCpp";
    const std::string prior_pointer = "20260709/workload_energy_dvt_rust";

    try {
        WorkloadSample sample = make_sample(
            sample_id,
            node_id,
            timestamp_utc,
            energy_req_j,
            energy_surplus_j,
            hydraulic_risk,
            uncertainty_risk,
            vt_before
        );

        sqlite3 *db = nullptr;
        int rc = sqlite3_open(db_path.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Unable to open DB: " + msg);
        }

        ensure_daily_progress_schema(db);
        insert_daily_progress(db, sample, yyyymmdd, subtask_id, phoenix_hex, prior_pointer);
        sqlite3_close(db);

        std::cout << "Recorded workload sample for node " << node_id
                  << " with K=" << sample.k_factor
                  << " E=" << sample.e_factor
                  << " R=" << sample.r_factor << "\n";

        return 0;
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 2;
    }
}
