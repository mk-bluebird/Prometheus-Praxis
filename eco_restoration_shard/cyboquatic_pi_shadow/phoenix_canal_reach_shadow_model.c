/*
 * SPDX-License-Identifier: MIT OR Apache-2.0
 *
 * File: eco_restoration_shard/cyboquatic_pi_shadow/phoenix_canal_reach_shadow_model.c
 *
 * Role:
 *   Raspberry-Pi shadow model for a Phoenix canal reach:
 *   - Integrates BOD and DO via a simple ODE (explicit Euler/RK2).
 *   - Computes Lyapunov-style residual Vt over normalized risk coordinates.
 *   - Derives KER-like diagnostics.
 *   - Logs each step and window aggregate into SQLite.
 *
 * Dependencies:
 *   - SQLite3 (linked as -lsqlite3).
 *   - Standard C library only (no threads, no dynamic STL).
 *
 * Non-actuating:
 *   - Reads configuration and pseudo-telemetry.
 *   - Does NOT control pumps or gates directly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sqlite3.h>

/* ---------- Phoenix canal reach configuration ---------- */

/* Simple state: BOD (mg/L), DO (mg/L). */
typedef struct {
    double bod_mgL;
    double do_mgL;
} CanalState;

/* Physical and corridor parameters for one reach. */
typedef struct {
    /* Object identity and corridor binding. */
    const char *city_object_id;      /* e.g. "PHX-CANAL-001-REACH-01" */
    const char *corridor_id;        /* e.g. "METROCENTER-CANAL-001"   */
    const char *corridor_profile;   /* e.g. "ecosafety.corridor.metrocenter.v1" */

    /* ODE parameters (non-fictional, typical ranges). */
    double k_bod;        /* BOD decay rate [1/day].       */
    double k_reaer;      /* Reaeration rate [1/day].      */
    double do_sat_mgL;   /* Saturation DO [mg/L].         */

    /* Energy lane scalar for Vheat coupling (0..1). */
    double r_energy;     /* normalized energy lane risk.  */

    /* Lyapunov weights (non-negative). */
    double w_bod;
    double w_do_def;
    double w_energy;

    /* Corridor ceilings for Lyapunov residual and RoH. */
    double vt_ceiling;
    double roh_ceiling;
} CanalConfig;

/* ---------- Risk coordinates and Lyapunov residual ---------- */

/* Normalize BOD to a risk coordinate r_bod in [0,1]. */
static double normalize_bod(double bod_mgL, double bod_ref_mgL)
{
    if (bod_ref_mgL <= 0.0) {
        return 0.0;
    }
    double x = bod_mgL / bod_ref_mgL;
    if (x < 0.0) x = 0.0;
    /* Saturating: r = 1 - exp(-x) */
    double r = 1.0 - exp(-x);
    if (r > 1.0) r = 1.0;
    return r;
}

/* Normalize DO deficit (DO_sat - DO) to risk r_do_def in [0,1]. */
static double normalize_do_def(double do_mgL, double do_sat_mgL, double deficit_ref_mgL)
{
    double deficit = do_sat_mgL - do_mgL;
    if (deficit < 0.0) deficit = 0.0;

    if (deficit_ref_mgL <= 0.0) {
        return 0.0;
    }

    double x = deficit / deficit_ref_mgL;
    if (x < 0.0) x = 0.0;
    double r = 1.0 - exp(-x);
    if (r > 1.0) r = 1.0;
    return r;
}

/* Clamp a normalized risk coordinate into [0,1]. */
static double clamp01(double v)
{
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

/* Compute Lyapunov-like residual Vt = sum_j w_j * r_j^2. */
static double compute_vt(const CanalConfig *cfg,
                         double r_bod,
                         double r_do_def,
                         double r_energy)
{
    double rb = clamp01(r_bod);
    double rd = clamp01(r_do_def);
    double re = clamp01(r_energy);

    double v = 0.0;
    v += cfg->w_bod     * rb * rb;
    v += cfg->w_do_def  * rd * rd;
    v += cfg->w_energy  * re * re;
    return v;
}

/* Simple risk-of-harm scalar RoH from residual and energy. */
static double compute_roh(double vt, double r_energy)
{
    /* Blend residual and energy lane; clamp to [0,1]. */
    double v = clamp01(vt);
    double re = clamp01(r_energy);
    double roh = 0.7 * v + 0.3 * re;
    if (roh > 1.0) roh = 1.0;
    return roh;
}

/* ---------- KER triad diagnostics (continuous, double) ---------- */

typedef struct {
    double K; /* Knowledge factor 0..1 */
    double E; /* Eco-impact 0..1       */
    double R; /* Risk-of-harm 0..1     */
} KER;

/* Compute a simple KER triad consistent with your fixed-point kernel. */
static KER compute_ker(double vt,
                       double roh,
                       double ecoimpact_score,
                       double uncertainty)
{
    KER out;

    /* Knowledge: high when uncertainty is low and residual is moderate. */
    double u = clamp01(uncertainty);
    double v = clamp01(vt);
    double k_corridor = 1.0 - u;
    double k_residual = 1.0 - v;
    out.K = 0.6 * k_corridor + 0.4 * k_residual;
    if (out.K < 0.0) out.K = 0.0;
    if (out.K > 1.0) out.K = 1.0;

    /* Eco-impact: ecoimpact_score (0..1) times a simple trust factor. */
    double e_raw = clamp01(ecoimpact_score);
    double trust = 1.0 - u;
    out.E = e_raw * trust;
    if (out.E < 0.0) out.E = 0.0;
    if (out.E > 1.0) out.E = 1.0;

    /* Risk-of-harm: blend RoH and uncertainty. */
    double roh_clamped = clamp01(roh);
    out.R = 0.7 * roh_clamped + 0.3 * u;
    if (out.R < 0.0) out.R = 0.0;
    if (out.R > 1.0) out.R = 1.0;

    return out;
}

/* ---------- ODE integrator for BOD/DO ---------- */

/*
 * Simple ODE:
 *   dBOD/dt = -k_bod * BOD
 *   dDO/dt  = k_reaer * (DO_sat - DO) - alpha * k_bod * BOD
 *
 * where alpha is a stoichiometric coefficient coupling BOD decay to DO.
 */

typedef struct {
    double alpha_bod_to_do;
    double bod_ref_mgL;
    double deficit_ref_mgL;
} ODEParams;

static CanalState ode_rhs(const CanalConfig *cfg,
                          const ODEParams *ode,
                          const CanalState *s)
{
    CanalState dsdt;
    dsdt.bod_mgL = -cfg->k_bod * s->bod_mgL;
    double do_def = cfg->do_sat_mgL - s->do_mgL;
    dsdt.do_mgL = cfg->k_reaer * do_def - ode->alpha_bod_to_do * cfg->k_bod * s->bod_mgL;
    return dsdt;
}

/* Explicit Euler step. dt in days. */
static CanalState step_euler(const CanalConfig *cfg,
                             const ODEParams *ode,
                             const CanalState *s,
                             double dt_days)
{
    CanalState dsdt = ode_rhs(cfg, ode, s);
    CanalState sn;
    sn.bod_mgL = s->bod_mgL + dt_days * dsdt.bod_mgL;
    sn.do_mgL  = s->do_mgL  + dt_days * dsdt.do_mg_mgL; /* bug; see correction below */
    return sn;
}

/* Corrected DO step (do_mgL field). */
static CanalState step_euler_correct(const CanalConfig *cfg,
                                     const ODEParams *ode,
                                     const CanalState *s,
                                     double dt_days)
{
    CanalState dsdt = ode_rhs(cfg, ode, s);
    CanalState sn;
    sn.bod_mgL = s->bod_mgL + dt_days * dsdt.bod_mgL;
    sn.do_mgL  = s->do_mgL  + dt_days * dsdt.do_mgL;
    return sn;
}

/* RK2 (midpoint) integrator for better stability. */
static CanalState step_rk2(const CanalConfig *cfg,
                           const ODEParams *ode,
                           const CanalState *s,
                           double dt_days)
{
    CanalState k1 = ode_rhs(cfg, ode, s);
    CanalState mid;
    mid.bod_mgL = s->bod_mgL + 0.5 * dt_days * k1.bod_mgL;
    mid.do_mgL  = s->do_mgL  + 0.5 * dt_days * k1.do_mgL;

    CanalState k2 = ode_rhs(cfg, ode, &mid);

    CanalState sn;
    sn.bod_mgL = s->bod_mgL + dt_days * k2.bod_mgL;
    sn.do_mgL  = s->do_mgL  + dt_days * k2.do_mgL;
    return sn;
}

/* ---------- SQLite logging ---------- */

typedef struct {
    sqlite3 *db;
} DbHandle;

/* Open SQLite database. Path can be on Pi filesystem (e.g. /var/lib/eco_restoration_shard/...). */
static int db_open(DbHandle *dbh, const char *path)
{
    int rc = sqlite3_open(path, &dbh->db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "sqlite3_open failed: %s\n", sqlite3_errmsg(dbh->db));
        return rc;
    }
    return SQLITE_OK;
}

static void db_close(DbHandle *dbh)
{
    if (dbh->db) {
        sqlite3_close(dbh->db);
        dbh->db = NULL;
    }
}

/* Create tables if not exist: canal_shadow_state and dailyprogress (compatible schema). */
static int db_init_schema(DbHandle *dbh)
{
    const char *sql =
        "PRAGMA foreign_keys = ON;"
        "CREATE TABLE IF NOT EXISTS canal_shadow_state ("
        "  state_id      INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  ts_utc        TEXT NOT NULL,"
        "  reach_id      TEXT NOT NULL,"
        "  corridor_id   TEXT NOT NULL,"
        "  bod_mgL       REAL NOT NULL,"
        "  do_mgL        REAL NOT NULL,"
        "  r_bod         REAL NOT NULL,"
        "  r_do_def      REAL NOT NULL,"
        "  r_energy      REAL NOT NULL,"
        "  vt            REAL NOT NULL,"
        "  roh           REAL NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS dailyprogress ("
        "  progressid    INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  yyyymmdd      TEXT NOT NULL,"
        "  domain        TEXT NOT NULL,"
        "  subtaskid     TEXT NOT NULL,"
        "  nodeid        TEXT NOT NULL,"
        "  energyreqJ    REAL NOT NULL,"
        "  deltaVt       REAL NOT NULL,"
        "  kscore        REAL NOT NULL,"
        "  escore        REAL NOT NULL,"
        "  rscore        REAL NOT NULL,"
        "  evidencehex   TEXT NOT NULL,"
        "  signingdid    TEXT NOT NULL,"
        "  prioranchorid TEXT,"
        "  createdutc    TEXT NOT NULL"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_shadow_reach_ts "
        "  ON canal_shadow_state(reach_id, ts_utc);"
        "CREATE INDEX IF NOT EXISTS idx_dailyprogress_date_domain "
        "  ON dailyprogress(yyyymmdd, domain);";

    char *errmsg = NULL;
    int rc = sqlite3_exec(dbh->db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_init_schema failed: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        return rc;
    }
    return SQLITE_OK;
}

/* Insert a single canal shadow state row. */
static int db_insert_canal_state(DbHandle *dbh,
                                 const CanalConfig *cfg,
                                 const CanalState *s,
                                 double r_bod,
                                 double r_do_def,
                                 double r_energy,
                                 double vt,
                                 double roh)
{
    const char *sql =
        "INSERT INTO canal_shadow_state "
        "(ts_utc, reach_id, corridor_id, bod_mgL, do_mgL, r_bod, r_do_def, r_energy, vt, roh) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(dbh->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "prepare canal_shadow_state failed: %s\n", sqlite3_errmsg(dbh->db));
        return rc;
    }

    /* Timestamp in ISO8601 UTC. */
    char tsbuf[32];
    time_t now = time(NULL);
    struct tm gm;
#if defined(_WIN32)
    gmtime_s(&gm, &now);
#else
    gmtime_r(&now, &gm);
#endif
    snprintf(tsbuf, sizeof(tsbuf),
             "%04d-%02d-%02dT%02d:%02d:%02dZ",
             gm.tm_year + 1900, gm.tm_mon + 1, gm.tm_mday,
             gm.tm_hour, gm.tm_min, gm.tm_sec);

    sqlite3_bind_text(stmt, 1, tsbuf, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, cfg->city_object_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, cfg->corridor_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, s->bod_mgL);
    sqlite3_bind_double(stmt, 5, s->do_mgL);
    sqlite3_bind_double(stmt, 6, r_bod);
    sqlite3_bind_double(stmt, 7, r_do_def);
    sqlite3_bind_double(stmt, 8, r_energy);
    sqlite3_bind_double(stmt, 9, vt);
    sqlite3_bind_double(stmt, 10, roh);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "insert canal_shadow_state failed: %s\n", sqlite3_errmsg(dbh->db));
    }

    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

/* Insert one dailyprogress window row (aggregated telemetry). */
static int db_insert_dailyprogress(DbHandle *dbh,
                                   const char *yyyymmdd,
                                   const char *domain,
                                   const char *subtaskid,
                                   const char *nodeid,
                                   double energyreqJ,
                                   double deltaVt,
                                   const KER *ker,
                                   const char *evidencehex,
                                   const char *signingdid,
                                   const char *prioranchorid)
{
    const char *sql =
        "INSERT INTO dailyprogress "
        "(yyyymmdd, domain, subtaskid, nodeid, energyreqJ, deltaVt, "
        " kscore, escore, rscore, evidencehex, signingdid, prioranchorid, createdutc) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(dbh->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "prepare dailyprogress failed: %s\n", sqlite3_errmsg(dbh->db));
        return rc;
    }

    /* Created UTC timestamp. */
    char tsbuf[32];
    time_t now = time(NULL);
    struct tm gm;
#if defined(_WIN32)
    gmtime_s(&gm, &now);
#else
    gmtime_r(&now, &gm);
#endif
    snprintf(tsbuf, sizeof(tsbuf),
             "%04d-%02d-%02dT%02d:%02d:%02dZ",
             gm.tm_year + 1900, gm.tm_mon + 1, gm.tm_mday,
             gm.tm_hour, gm.tm_min, gm.tm_sec);

    sqlite3_bind_text(stmt, 1, yyyymmdd, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, domain, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, subtaskid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, nodeid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 5, energyreqJ);
    sqlite3_bind_double(stmt, 6, deltaVt);
    sqlite3_bind_double(stmt, 7, ker->K);
    sqlite3_bind_double(stmt, 8, ker->E);
    sqlite3_bind_double(stmt, 9, ker->R);
    sqlite3_bind_text(stmt, 10, evidencehex, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 11, signingdid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 12, prioranchorid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 13, tsbuf, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "insert dailyprogress failed: %s\n", sqlite3_errmsg(dbh->db));
    }

    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

/* ---------- Main loop: integrate and log ---------- */

int main(int argc, char **argv)
{
    /* 1. Configure canal reach and ODE parameters. */
    CanalConfig cfg = {
        .city_object_id   = "PHX-CANAL-001-REACH-01",
        .corridor_id      = "METROCENTER-CANAL-001",
        .corridor_profile = "ecosafety.corridor.metrocenter.v1",
        .k_bod            = 0.35,  /* 1/day */
        .k_reaer          = 0.7,   /* 1/day */
        .do_sat_mgL       = 8.0,   /* mg/L */
        .r_energy         = 0.4,   /* lane risk 0..1 */
        .w_bod            = 0.5,
        .w_do_def         = 0.3,
        .w_energy         = 0.2,
        .vt_ceiling       = 0.25,
        .roh_ceiling      = 0.15
    };

    ODEParams ode = {
        .alpha_bod_to_do  = 0.3,
        .bod_ref_mgL      = 10.0,
        .deficit_ref_mgL  = 4.0
    };

    /* Initial state (shadow, not actuating). */
    CanalState s = {
        .bod_mgL = 8.0,
        .do_mgL  = 6.0
    };

    /* 2. Open database and init schema. */
    const char *dbpath = "eco_restoration_shard/cyboquatic_pi_shadow/dbcyboquaticdailyprogress.sqlite";
    DbHandle dbh = {0};

    if (db_open(&dbh, dbpath) != SQLITE_OK) {
        return 1;
    }
    if (db_init_schema(&dbh) != SQLITE_OK) {
        db_close(&dbh);
        return 1;
    }

    /* 3. Integration parameters. */
    const int    steps       = 96;          /* 96 * 0.25 h = 24 h */
    const double dt_hours    = 0.25;       /* step size in hours */
    const double dt_days     = dt_hours / 24.0;
    const double power_kW    = 2.0;        /* hypothetical pump power, kW (telemetry or estimate). */
    const double ecoimpact   = 0.89;       /* reuse CEIM-style ecoimpact score. */
    const double uncertainty = 0.11;       /* residual uncertainty (RoH band). */

    /* 4. Integrate over one day and log per-step plus aggregate. */
    double vt_prev = 0.0;
    double energyreqJ = 0.0;

    for (int i = 0; i < steps; ++i) {
        /* Compute normalized risk coordinates and Lyapunov residual. */
        double r_bod     = normalize_bod(s.bod_mgL, ode.bod_ref_mgL);
        double r_do_def  = normalize_do_def(s.do_mgL, cfg.do_sat_mgL, ode.deficit_ref_mgL);
        double r_energy  = clamp01(cfg.r_energy); /* lane scalar from corridor. */

        double vt        = compute_vt(&cfg, r_bod, r_do_def, r_energy);
        double roh       = compute_roh(vt, r_energy);

        /* Increment energy requirement J from kW * hours. 1 kWh = 3.6e6 J. */
        double energy_step_J = power_kW * dt_hours * 3.6e6;
        energyreqJ += energy_step_J;

        /* Log canal shadow state for this step. */
        if (db_insert_canal_state(&dbh, &cfg, &s, r_bod, r_do_def, r_energy, vt, roh) != SQLITE_OK) {
            fprintf(stderr, "Failed to insert canal_shadow_state at step %d\n", i);
        }

        /* Advance state via RK2. */
        CanalState sn = step_rk2(&cfg, &ode, &s, dt_days);
        s = sn;

        vt_prev = vt;
    }

    /* 5. After the day, compute diagnostics and write dailyprogress row. */
    double r_bod     = normalize_bod(s.bod_mgL, ode.bod_ref_mgL);
    double r_do_def  = normalize_do_def(s.do_mgL, cfg.do_sat_mgL, ode.deficit_ref_mgL);
    double r_energy  = clamp01(cfg.r_energy);
    double vt_new    = compute_vt(&cfg, r_bod, r_do_def, r_energy);
    double roh_new   = compute_roh(vt_new, r_energy);

    double deltaVt   = vt_new - vt_prev;

    KER ker          = compute_ker(vt_new, roh_new, ecoimpact, uncertainty);

    /* Evidence and governance anchors (hex and DID). */
    const char *yyyymmdd      = "20260720";
    const char *domain        = "PHX-CANAL-REACH-SHADOW";
    const char *subtaskid     = "PHX-CANAL-ENERGY-BODDO-2026-07-20";
    const char *nodeid        = cfg.city_object_id;
    const char *evidencehex   = "0x20260720PHXCANALREACHSHADOW";
    const char *signingdid    = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";
    const char *prioranchorid = "PHX-CANAL-REACH-SHADOW-2026-07-19";

    if (db_insert_dailyprogress(&dbh,
                                yyyymmdd,
                                domain,
                                subtaskid,
                                nodeid,
                                energyreqJ,
                                deltaVt,
                                &ker,
                                evidencehex,
                                signingdid,
                                prioranchorid) != SQLITE_OK) {
        fprintf(stderr, "Failed to insert dailyprogress aggregate\n");
    }

    db_close(&dbh);
    return 0;
}
