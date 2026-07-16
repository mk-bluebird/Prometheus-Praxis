// filename: eco_restoration_shard/cyboquatic_progress/20260715/java/CyboquaticWorkloadTelemetry.java
// purpose: Java helper for non-actuating telemetry logging of cyboquatic workloads
//          into a daily SQLite DB table, aligned with energyreqJ and ΔVt semantics.

import java.sql.*;
import java.util.Objects;

public class CyboquaticWorkloadTelemetry {

    public static final String PHOENIX_EVIDENCE_HEX = "0x20260715PHXENERGYREQDV";

    public static void ensureSchema(Connection conn) throws SQLException {
        Objects.requireNonNull(conn, "Connection must not be null");
        try (Statement stmt = conn.createStatement()) {
            stmt.execute("PRAGMA foreign_keys = ON;");
            stmt.execute(
                "CREATE TABLE IF NOT EXISTS daily_progress (" +
                "  progress_id   INTEGER PRIMARY KEY AUTOINCREMENT," +
                "  yyyymmdd      TEXT NOT NULL," +
                "  domain        TEXT NOT NULL," +
                "  subtask_id    TEXT NOT NULL," +
                "  segment_id    TEXT NOT NULL," +
                "  flow_m3s      REAL NOT NULL," +
                "  head_loss_m   REAL NOT NULL," +
                "  density_kgm3  REAL NOT NULL," +
                "  g_ms2         REAL NOT NULL," +
                "  energyreq_j   REAL NOT NULL," +
                "  vt_before     REAL NOT NULL," +
                "  vt_after      REAL NOT NULL," +
                "  deltavt       REAL NOT NULL," +
                "  k_factor      REAL NOT NULL," +
                "  e_factor      REAL NOT NULL," +
                "  r_factor      REAL NOT NULL," +
                "  evidence_hex  TEXT NOT NULL," +
                "  prior_pointer TEXT NOT NULL" +
                ");"
            );
            stmt.execute(
                "CREATE INDEX IF NOT EXISTS idx_daily_progress_date " +
                "ON daily_progress(yyyymmdd);"
            );
            stmt.execute(
                "CREATE INDEX IF NOT EXISTS idx_daily_progress_segment_time " +
                "ON daily_progress(segment_id, yyyymmdd);"
            );
        }
    }

    public static void insertSample(Connection conn,
                                    String yyyymmdd,
                                    String subtaskId,
                                    String segmentId,
                                    double flow_m3s,
                                    double head_loss_m,
                                    double density_kgm3,
                                    double g_ms2,
                                    double energyreq_j,
                                    double vt_before,
                                    double vt_after,
                                    double deltavt,
                                    double k_factor,
                                    double e_factor,
                                    double r_factor,
                                    String priorPointerHex) throws SQLException {

        String sql = "INSERT INTO daily_progress (" +
                     "yyyymmdd, domain, subtask_id, segment_id, " +
                     "flow_m3s, head_loss_m, density_kgm3, g_ms2, energyreq_j, " +
                     "vt_before, vt_after, deltavt, k_factor, e_factor, r_factor, " +
                     "evidence_hex, prior_pointer" +
                     ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setString(1, yyyymmdd);
            ps.setString(2, "cyboquatic_workload");
            ps.setString(3, subtaskId);
            ps.setString(4, segmentId);
            ps.setDouble(5, flow_m3s);
            ps.setDouble(6, head_loss_m);
            ps.setDouble(7, density_kgm3);
            ps.setDouble(8, g_ms2);
            ps.setDouble(9, energyreq_j);
            ps.setDouble(10, vt_before);
            ps.setDouble(11, vt_after);
            ps.setDouble(12, deltavt);
            ps.setDouble(13, k_factor);
            ps.setDouble(14, e_factor);
            ps.setDouble(15, r_factor);
            ps.setString(16, PHOENIX_EVIDENCE_HEX);
            ps.setString(17, priorPointerHex);
            ps.executeUpdate();
        }
    }

    // Example usage: log one workload sample computed in C++.
    public static void main(String[] args) {
        String dbPath = "cyboquatic_daily_progress.sqlite";
        String url = "jdbc:sqlite:" + dbPath;
        try (Connection conn = DriverManager.getConnection(url)) {
            ensureSchema(conn);
            // Example values; in practice, pull from C++ outputs or sensors.
            insertSample(
                conn,
                "20260715",
                "PHX-CANAL-ENERGYREQDV-20260715",
                "PHX-CANAL-NODE-ENERGY-01",
                3.0,
                1.2,
                1000.0,
                9.81,
                35316.0,   // example J from C++ computation
                0.25,
                0.30,
                0.05,
                0.85,
                0.70,
                0.30,
                "0x20260714PHXPREVENERGYREQDV"
            );
        } catch (SQLException e) {
            e.printStackTrace();
        }
    }
}
