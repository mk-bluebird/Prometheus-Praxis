// filename: src/tools/restoration_energy_mt6883_tool.cpp
// destination: eco_restoration_shard/src/tools/restoration_energy_mt6883_tool.cpp
// repo-target: github.com/mk-bluebird/eco_restoration_shard

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>
#include <vector>

#include <sqlite3.h>

namespace {

struct QueryOptions {
    std::string db_path;
    bool list_restoration_contracts_primary;
    bool list_prod_restoration_planes_phx;
    bool list_ecoperjoule_prod_phx;
    bool list_mt6883_lane_continuity;
};

class SqliteConnection {
public:
    explicit SqliteConnection(const std::string& path) : db_(nullptr) {
        if (sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to open SQLite DB at " + path);
        }
    }

    ~SqliteConnection() {
        if (db_ != nullptr) {
            sqlite3_close(db_);
        }
    }

    sqlite3* get() const {
        return db_;
    }

private:
    sqlite3* db_;
};

void print_row_sep() {
    std::puts("--------------------------------------------------------------------------");
}

void list_active_restoration_contracts_primary(sqlite3* db) {
    const char* sql =
        "SELECT contractid, contractname, versiontag, status, "
        "       bostrom_address, region, scope, kerdeployable, prodeligible, "
        "       createdutc, updatedutc "
        "FROM v_active_restoration_contracts_primary "
        "ORDER BY contractname, versiontag";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare v_active_restoration_contracts_primary query");
    }

    std::puts("Active restoration contracts bound to PRIMARY Bostrom address:");
    print_row_sep();

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const int64_t contractid       = sqlite3_column_int64(stmt, 0);
        const char*   contractname     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char*   versiontag       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const char*   status           = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char*   bostrom_address  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        const char*   region           = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        const char*   scope            = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        const int64_t kerdeployable    = sqlite3_column_int64(stmt, 7);
        const int64_t prodeligible     = sqlite3_column_int64(stmt, 8);
        const char*   createdutc       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        const char*   updatedutc       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));

        std::printf(
            "contractid=%lld name=%s version=%s status=%s region=%s scope=%s "
            "kerdeployable=%lld prodeligible=%lld bostrom=%s created=%s updated=%s\n",
            static_cast<long long>(contractid),
            contractname ? contractname : "",
            versiontag ? versiontag : "",
            status ? status : "",
            region ? region : "",
            scope ? scope : "",
            static_cast<long long>(kerdeployable),
            static_cast<long long>(prodeligible),
            bostrom_address ? bostrom_address : "",
            createdutc ? createdutc : "",
            updatedutc ? updatedutc : ""
        );
    }

    sqlite3_finalize(stmt);
}

void list_prod_restoration_planes_phx(sqlite3* db) {
    const char* sql =
        "SELECT planeid, plane_name, region, scope, lane, "
        "       kmetric, emetric, rmetric, vtresidual, "
        "       kerdeployable, prodeligible, "
        "       restorationradius_m, restorationradius_hours, "
        "       deltamass_window_kg, deltakarma_window, "
        "       createdutc, updatedutc "
        "FROM v_prod_eligible_restoration_planes "
        "WHERE region = 'Phoenix-AZ' "
        "ORDER BY plane_name";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare v_prod_eligible_restoration_planes query");
    }

    std::puts("PROD-eligible restoration planes for Phoenix-AZ:");
    print_row_sep();

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const int64_t planeid            = sqlite3_column_int64(stmt, 0);
        const char*   plane_name         = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char*   region             = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const char*   scope              = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char*   lane               = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        const double  kmetric            = sqlite3_column_double(stmt, 5);
        const double  emetric            = sqlite3_column_double(stmt, 6);
        const double  rmetric            = sqlite3_column_double(stmt, 7);
        const double  vtresidual         = sqlite3_column_double(stmt, 8);
        const int64_t kerdeployable      = sqlite3_column_int64(stmt, 9);
        const int64_t prodeligible       = sqlite3_column_int64(stmt, 10);
        const double  restorationradiusm = sqlite3_column_double(stmt, 11);
        const double  restorationhr      = sqlite3_column_double(stmt, 12);
        const double  deltamasskg        = sqlite3_column_double(stmt, 13);
        const double  deltakarma         = sqlite3_column_double(stmt, 14);
        const char*   createdutc         = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 15));
        const char*   updatedutc         = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 16));

        std::printf(
            "planeid=%lld name=%s region=%s scope=%s lane=%s "
            "K=%.3f E=%.3f R=%.3f Vt=%.3f "
            "kerdeployable=%lld prodeligible=%lld "
            "restorationradius_m=%.2f restorationradius_hours=%.2f "
            "deltamass_window_kg=%.3f deltakarma_window=%.3f "
            "created=%s updated=%s\n",
            static_cast<long long>(planeid),
            plane_name ? plane_name : "",
            region ? region : "",
            scope ? scope : "",
            lane ? lane : "",
            kmetric,
            emetric,
            rmetric,
            vtresidual,
            static_cast<long long>(kerdeployable),
            static_cast<long long>(prodeligible),
            restorationradiusm,
            restorationhr,
            deltamasskg,
            deltakarma,
            createdutc ? createdutc : "",
            updatedutc ? updatedutc : ""
        );
    }

    sqlite3_finalize(stmt);
}

void list_ecoperjoule_prod_phx(sqlite3* db) {
    const char* sql =
        "SELECT nodeid, region, domain, "
        "       twindowstart, twindowend, "
        "       vtresidual, kscore, escore, rscore, "
        "       lane, kerdeployable, "
        "       ecoperjoule, theta_eco_min, "
        "       carbonnegativeok, "
        "       author_bostrom, author_contractid "
        "FROM v_cyboquatic_ecoperjoule_prod_phx "
        "ORDER BY nodeid, twindowstart";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare v_cyboquatic_ecoperjoule_prod_phx query");
    }

    std::puts("Phoenix-AZ PROD eco-per-joule windows for Cyboquatic MAR nodes:");
    print_row_sep();

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* nodeid          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* region          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* domain          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const char* tstart          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char* tend            = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        const double vtresidual     = sqlite3_column_double(stmt, 5);
        const double kscore         = sqlite3_column_double(stmt, 6);
        const double escore         = sqlite3_column_double(stmt, 7);
        const double rscore         = sqlite3_column_double(stmt, 8);
        const char* lane            = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        const int64_t kerdeployable = sqlite3_column_int64(stmt, 10);
        const double ecoperjoule    = sqlite3_column_double(stmt, 11);
        const double theta_eco_min  = sqlite3_column_double(stmt, 12);
        const int64_t carbonok      = sqlite3_column_int64(stmt, 13);
        const char* author_bostrom  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
        const char* author_contract = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 15));

        std::printf(
            "nodeid=%s region=%s domain=%s "
            "window=[%s,%s] Vt=%.3f K=%.3f E=%.3f R=%.3f "
            "lane=%s kerdeployable=%lld ecoperjoule=%.6e "
            "theta_eco_min=%.6e carbonnegativeok=%lld "
            "policy_author_bostrom=%s policy_contractid=%s\n",
            nodeid ? nodeid : "",
            region ? region : "",
            domain ? domain : "",
            tstart ? tstart : "",
            tend ? tend : "",
            vtresidual,
            kscore,
            escore,
            rscore,
            lane ? lane : "",
            static_cast<long long>(kerdeployable),
            ecoperjoule,
            theta_eco_min,
            static_cast<long long>(carbonok),
            author_bostrom ? author_bostrom : "",
            author_contract ? author_contract : ""
        );
    }

    sqlite3_finalize(stmt);
}

void list_mt6883_lane_continuity(sqlite3* db) {
    const char* sql =
        "SELECT kernelid, region, lane, "
        "       kscore, escore, rscore, vtmax, "
        "       planesok, topologyok, "
        "       mt6883_registry_id, mt6883_ok, "
        "       neuroethic_radius_hours, neuroethic_ok, "
        "       author_bostrom, author_contractid "
        "FROM v_mt6883_lane_continuity "
        "ORDER BY region, kernelid";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare v_mt6883_lane_continuity query");
    }

    std::puts("MT6883 lane continuity and neuroethic constraints:");
    print_row_sep();

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char*  kernelid          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char*  region            = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char*  lane              = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const double kscore            = sqlite3_column_double(stmt, 3);
        const double escore            = sqlite3_column_double(stmt, 4);
        const double rscore            = sqlite3_column_double(stmt, 5);
        const double vtmax             = sqlite3_column_double(stmt, 6);
        const int64_t planesok         = sqlite3_column_int64(stmt, 7);
        const int64_t topologyok       = sqlite3_column_int64(stmt, 8);
        const int64_t mt6883_id        = sqlite3_column_int64(stmt, 9);
        const int64_t mt6883_ok        = sqlite3_column_int64(stmt, 10);
        const double neuro_radius_hrs  = sqlite3_column_double(stmt, 11);
        const int64_t neuro_ok         = sqlite3_column_int64(stmt, 12);
        const char*  author_bostrom    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13));
        const char*  author_contractid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));

        std::printf(
            "kernelid=%s region=%s lane=%s "
            "K=%.3f E=%.3f R=%.3f Vt=%.3f "
            "planesok=%lld topologyok=%lld "
            "mt6883_registry_id=%lld mt6883_ok=%lld "
            "neuroethic_radius_hours=%.2f neuroethic_ok=%lld "
            "author_bostrom=%s author_contractid=%s\n",
            kernelid ? kernelid : "",
            region ? region : "",
            lane ? lane : "",
            kscore,
            escore,
            rscore,
            vtmax,
            static_cast<long long>(planesok),
            static_cast<long long>(topologyok),
            static_cast<long long>(mt6883_id),
            static_cast<long long>(mt6883_ok),
            neuro_radius_hrs,
            static_cast<long long>(neuro_ok),
            author_bostrom ? author_bostrom : "",
            author_contractid ? author_contractid : ""
        );
    }

    sqlite3_finalize(stmt);
}

QueryOptions parse_args(int argc, char** argv) {
    QueryOptions opts;
    opts.db_path = "db/restorationindex.sqlite3";
    opts.list_restoration_contracts_primary = false;
    opts.list_prod_restoration_planes_phx   = false;
    opts.list_ecoperjoule_prod_phx          = false;
    opts.list_mt6883_lane_continuity        = false;

    if (argc < 2) {
        std::fprintf(
            stderr,
            "Usage: %s [--db PATH] "
            "[--contracts-primary] "
            "[--planes-restoration-phx] "
            "[--ecoperjoule-prod-phx] "
            "[--mt6883-lane]\n",
            argv[0]
        );
        std::exit(EXIT_FAILURE);
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--db" && i + 1 < argc) {
            opts.db_path = argv[++i];
        } else if (arg == "--contracts-primary") {
            opts.list_restoration_contracts_primary = true;
        } else if (arg == "--planes-restoration-phx") {
            opts.list_prod_restoration_planes_phx = true;
        } else if (arg == "--ecoperjoule-prod-phx") {
            opts.list_ecoperjoule_prod_phx = true;
        } else if (arg == "--mt6883-lane") {
            opts.list_mt6883_lane_continuity = true;
        } else {
            std::fprintf(stderr, "Unknown argument: %s\n", arg.c_str());
            std::exit(EXIT_FAILURE);
        }
    }

    return opts;
}

} // namespace

int main(int argc, char** argv) {
    try {
        QueryOptions opts = parse_args(argc, argv);
        SqliteConnection conn(opts.db_path);
        sqlite3* db = conn.get();

        if (opts.list_restoration_contracts_primary) {
            list_active_restoration_contracts_primary(db);
        }

        if (opts.list_prod_restoration_planes_phx) {
            list_prod_restoration_planes_phx(db);
        }

        if (opts.list_ecoperjoule_prod_phx) {
            list_ecoperjoule_prod_phx(db);
        }

        if (opts.list_mt6883_lane_continuity) {
            list_mt6883_lane_continuity(db);
        }

        if (!opts.list_restoration_contracts_primary &&
            !opts.list_prod_restoration_planes_phx &&
            !opts.list_ecoperjoule_prod_phx &&
            !opts.list_mt6883_lane_continuity) {
            std::fprintf(
                stderr,
                "No query selected. Use one of: "
                "--contracts-primary, "
                "--planes-restoration-phx, "
                "--ecoperjoule-prod-phx, "
                "--mt6883-lane\n"
            );
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "Error: %s\n", ex.what());
        return EXIT_FAILURE;
    }
}
