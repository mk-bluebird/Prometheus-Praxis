// File: cpp/eco_restoration/adaptive_hyperbolic_discount_schema.cpp

#include <iostream>
#include <string>
#include <stdexcept>
#include <sqlite3.h>

namespace prometheus_praxis {
namespace eco_restoration {

void exec_sql(sqlite3* db, const std::string& sql) {
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "";
        sqlite3_free(errmsg);
        throw std::runtime_error("SQLite error: " + msg);
    }
}

// Install schema and views for adaptive hyperbolic discounting of ker_e
// driven by climate urgency (heat-wave + grid carbon intensity).
void install_adaptive_discount_schema(sqlite3* db) {
    // Climate urgency view: aggregates near-term LST deviations and grid carbon intensity
    // to produce a per-time urgency score U(t).
    //
    // Assumed inputs:
    //   hex_lst_nearterm_with_recovery(h3_index, ts, lst_c, lst_baseline_c, lst_recent_c, lst_drop_c)
    //   grid_carbon_intensity(ts, c_intensity_gco2_per_kwh)
    //
    // Urgency formula (example):
    //   U(t) = β_lst * avg_positive(LST deviation) + β_grid * normalized(grid carbon intensity)
    // where:
    //   LST deviation = max(0, lst_recent_c - lst_baseline_c)
    //   normalized grid carbon = c_intensity / c_ref
    const std::string sql_climate_urgency = R"SQL(
        CREATE VIEW IF NOT EXISTS climate_urgency AS
        WITH lst_dev AS (
            SELECT
                ts,
                AVG(
                    CASE
                        WHEN lst_recent_c > lst_baseline_c
                        THEN lst_recent_c - lst_baseline_c
                        ELSE 0.0
                    END
                ) AS avg_lst_dev
            FROM hex_lst_nearterm_with_recovery
            GROUP BY ts
        ),
        grid_sig AS (
            SELECT
                ts,
                c_intensity_gco2_per_kwh AS c_intensity
            FROM grid_carbon_intensity
        )
        SELECT
            l.ts,
            l.avg_lst_dev,
            g.c_intensity,
            -- Example β coefficients; to be calibrated:
            -- β_lst = 0.5 (per °C), β_grid = 0.5 (normalized),
            -- c_ref = 300 gCO2/kWh.
            0.5 * l.avg_lst_dev
            + 0.5 * (g.c_intensity / 300.0) AS urgency
        FROM lst_dev l
        JOIN grid_sig g ON g.ts = l.ts;
    )SQL";

    // Adaptive hyperbolic discount sequence a_n for discrete steps n=0..N-1:
    // Classical hyperbolic discount: D(n) = 1 / (1 + k n).
    // Here we let k be time-varying via urgency U_n:
    //   k_n = k_base * (1 + γ * U_n)
    // and define recursive discount factors:
    //   a_0 = 1
    //   a_n = a_{n-1} / (1 + k_n), n >= 1
    //
    // For SQL implementation, we approximate discrete time using row_number over ts.
    // We compute k_n and a_n via a recursive CTE.
    const std::string sql_discount_sequence = R"SQL(
        CREATE VIEW IF NOT EXISTS ker_adaptive_discount AS
        WITH ordered_urgency AS (
            SELECT
                ts,
                urgency,
                ROW_NUMBER() OVER (ORDER BY ts ASC) - 1 AS n
            FROM climate_urgency
        ),
        discount_seq(n, ts, urgency, k_n, a_n) AS (
            -- Base step n=0
            SELECT
                ou.n,
                ou.ts,
                ou.urgency,
                -- k_n = k_base * (1 + γ * U_n); use k_base=0.02, γ=1.0 as example.
                0.02 * (1.0 + 1.0 * ou.urgency) AS k_n,
                1.0 AS a_n
            FROM ordered_urgency ou
            WHERE ou.n = 0

            UNION ALL

            -- Recursive step for n>0
            SELECT
                ou.n,
                ou.ts,
                ou.urgency,
                0.02 * (1.0 + 1.0 * ou.urgency) AS k_n,
                ds.a_n / (1.0 + 0.02 * (1.0 + 1.0 * ou.urgency)) AS a_n
            FROM ordered_urgency ou
            JOIN discount_seq ds ON ou.n = ds.n + 1
        )
        SELECT
            ts,
            urgency,
            k_n,
            a_n
        FROM discount_seq;
    )SQL";

    // Discounted ker_e view: applies adaptive hyperbolic discount a_n to ker_e
    // values indexed by ts. We assume ker_e(ts, value) table exists.
    const std::string sql_discounted_ker_e = R"SQL(
        CREATE VIEW IF NOT EXISTS ker_e_discounted AS
        SELECT
            k.ts,
            k.value AS ker_e_raw,
            d.a_n   AS discount_factor,
            k.value * d.a_n AS ker_e_discounted,
            d.urgency,
            d.k_n
        FROM ker_e k
        JOIN ker_adaptive_discount d ON d.ts = k.ts;
    )SQL";

    exec_sql(db, sql_climate_urgency);
    exec_sql(db, sql_discount_sequence);
    exec_sql(db, sql_discounted_ker_e);
}

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "ker_adaptive_discount.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    sqlite3* db = nullptr;
    int rc = sqlite3_open(db_path.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to open DB: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    try {
        install_adaptive_discount_schema(db);
        std::cout << "Adaptive hyperbolic discount schema + views installed into "
                  << db_path << std::endl;

        std::cout << "\n-- Example query: climate urgency per timestamp --\n";
        std::cout << "SELECT ts, avg_lst_dev, c_intensity, urgency FROM climate_urgency ORDER BY ts LIMIT 10;\n";

        std::cout << "\n-- Example query: adaptive discount sequence --\n";
        std::cout << "SELECT ts, urgency, k_n, a_n FROM ker_adaptive_discount ORDER BY ts LIMIT 10;\n";

        std::cout << "\n-- Example query: discounted ker_e --\n";
        std::cout << "SELECT ts, ker_e_raw, discount_factor, ker_e_discounted "
                  << "FROM ker_e_discounted ORDER BY ts LIMIT 10;\n";
    } catch (const std::exception& ex) {
        std::cerr << "Adaptive discount schema error: " << ex.what() << std::endl;
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}
