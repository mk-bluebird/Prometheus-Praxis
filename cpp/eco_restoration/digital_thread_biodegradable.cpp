// File: cpp/eco_restoration/digital_thread_biodegradable.cpp

#include <string>
#include <stdexcept>
#include <sqlite3.h>

// Helper to install the digital thread schema for biodegradable compound batches.
class DigitalThreadSchemaInstaller {
public:
    explicit DigitalThreadSchemaInstaller(sqlite3* db)
        : db_(db) {
        if (!db_) throw std::runtime_error("SQLite DB pointer must not be null");
    }

    void install() {
        createBatchTable();
        createStageTables();
        createFootprintView();
    }

private:
    sqlite3* db_;

    void execSql(const std::string& sql) {
        char* errMsg = nullptr;
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string msg = "SQLite error: ";
            if (errMsg) msg += errMsg;
            sqlite3_free(errMsg);
            throw std::runtime_error(msg);
        }
    }

    void createBatchTable() {
        const char* sql =
            "CREATE TABLE IF NOT EXISTS biodegradable_batch ("
            " batch_id           TEXT PRIMARY KEY,"
            " material_name      TEXT NOT NULL,"
            " manufacture_utc    INTEGER NOT NULL,"
            " embodied_carbon_kg REAL NOT NULL,"     -- cradle emissions
            " iso_cert_14851     INTEGER NOT NULL,"  -- 0/1 flags for ISO 14851/14855
            " iso_cert_14855     INTEGER NOT NULL,"
            " oecd_301_score     REAL NOT NULL,"     -- biodegradability scalar [0,1]"
            ");";
        execSql(sql);
    }

    void createStageTables() {
        const char* transportSql =
            "CREATE TABLE IF NOT EXISTS biodegradable_transport_stage ("
            " stage_id           INTEGER PRIMARY KEY AUTOINCREMENT,"
            " batch_id           TEXT NOT NULL,"
            " mode               TEXT NOT NULL,"     -- e.g. 'TRUCK', 'RAIL'"
            " distance_km        REAL NOT NULL,"
            " carbon_cost_kg     REAL NOT NULL,"
            " stage_utc          INTEGER NOT NULL,"
            " FOREIGN KEY(batch_id) REFERENCES biodegradable_batch(batch_id)"
            ");";
        execSql(transportSql);

        const char* basinSql =
            "CREATE TABLE IF NOT EXISTS biodegradable_basin_stage ("
            " stage_id           INTEGER PRIMARY KEY AUTOINCREMENT,"
            " batch_id           TEXT NOT NULL,"
            " basin_id           TEXT NOT NULL,"
            " applied_mass_kg    REAL NOT NULL,"
            " sequestration_kg   REAL NOT NULL,"   -- estimated carbon sequestered"
            " respired_kg        REAL NOT NULL,"  -- biogenic CO2 back to atmosphere"
            " stage_utc          INTEGER NOT NULL,"
            " telemetry_ref      TEXT NOT NULL,"  -- link to PFAS/eco telemetry row id"
            " FOREIGN KEY(batch_id) REFERENCES biodegradable_batch(batch_id)"
            ");";
        execSql(basinSql);

        const char* merkleSql =
            "CREATE TABLE IF NOT EXISTS biodegradable_thread_merkle ("
            " batch_id        TEXT PRIMARY KEY,"
            " manufacture_root_hex TEXT NOT NULL,"
            " transport_root_hex   TEXT NOT NULL,"
            " basin_root_hex       TEXT NOT NULL,"
            " thread_root_hex      TEXT NOT NULL,"
            " did_owner       TEXT NOT NULL,"
            " created_utc     INTEGER NOT NULL,"
            " FOREIGN KEY(batch_id) REFERENCES biodegradable_batch(batch_id)"
            ");";
        execSql(merkleSql);
    }

    void createFootprintView() {
        const char* viewSql =
            "CREATE VIEW IF NOT EXISTS v_biodegradable_batch_footprint AS "
            "SELECT b.batch_id, b.material_name, "
            "       b.embodied_carbon_kg "
            "       + IFNULL(SUM(t.carbon_cost_kg), 0.0) "
            "       + IFNULL(SUM(bs.respired_kg), 0.0) "
            "       - IFNULL(SUM(bs.sequestration_kg), 0.0) "
            "         AS net_carbon_kg "
            "FROM biodegradable_batch b "
            "LEFT JOIN biodegradable_transport_stage t "
            "  ON t.batch_id = b.batch_id "
            "LEFT JOIN biodegradable_basin_stage bs "
            "  ON bs.batch_id = b.batch_id "
            "GROUP BY b.batch_id, b.material_name, b.embodied_carbon_kg;";
        execSql(viewSql);
    }
};
