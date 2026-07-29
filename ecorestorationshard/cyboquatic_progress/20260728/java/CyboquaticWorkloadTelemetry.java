// filename: ecorestorationshard/cyboquatic_progress/20260728/java/CyboquaticWorkloadTelemetry.java
// purpose: Java telemetry helper for cyboquatic workload residuals, writing into SQLite dailyprogress
// domain: (d) Cyboquatic workload (energyreqJ, ΔVt)
// note: uses JDBC, non-actuating, aligned with dbcyboquaticdailyprogress.sqlite schema

import java.sql.*;
import java.time.LocalDate;
import java.time.OffsetDateTime;
import java.time.ZoneOffset;

public final class CyboquaticWorkloadTelemetry {

    // Simplified DTO matching dailyprogress columns for domain d
    public static final class DailyProgressRow {
        public String domainId;        // e.g. "d"
        public String subtaskId;       // e.g. "PHX-CANAL-WL-2026-07-28"
        public LocalDate yyyymmdd;
        public String nodeId;
        public double energyReqJ;
        public double rEnergy;
        public double rHydraulics;
        public double rCarbon;
        public double rUncertainty;
        public double vtBefore;
        public double vtAfter;
        public double deltaVt;
        public String evidenceHex;
        public String signingDid;
        public String priorPointerHex;
    }

    private final Connection conn;

    public CyboquaticWorkloadTelemetry(Connection conn) {
        this.conn = conn;
    }

    // Ensure dailyprogress table has the necessary columns for workload telemetry
    public void ensureSchema() throws SQLException {
        try (Statement st = conn.createStatement()) {
            st.execute("PRAGMA foreign_keys = ON");
            st.execute("""
                CREATE TABLE IF NOT EXISTS dailyprogress (
                  id INTEGER PRIMARY KEY AUTOINCREMENT,
                  domain_id TEXT NOT NULL,
                  subtask_id TEXT NOT NULL,
                  yyyymmdd TEXT NOT NULL,
                  node_id TEXT NOT NULL,
                  energyreqj REAL NOT NULL,
                  r_energy REAL NOT NULL,
                  r_hydraulics REAL NOT NULL,
                  r_carbon REAL NOT NULL,
                  r_uncertainty REAL NOT NULL,
                  vt_before REAL NOT NULL,
                  vt_after REAL NOT NULL,
                  delta_vt REAL NOT NULL,
                  evidencehex TEXT NOT NULL,
                  signingdid TEXT NOT NULL,
                  priorpointerhex TEXT,
                  createdutc TEXT NOT NULL
                )
                """);
            st.execute("""
                CREATE INDEX IF NOT EXISTS idx_daily_domain_date
                ON dailyprogress(domain_id, yyyymmdd)
                """);
            st.execute("""
                CREATE INDEX IF NOT EXISTS idx_daily_node_date
                ON dailyprogress(node_id, yyyymmdd)
                """);
        }
    }

    // Insert a single cyboquatic workload residual row (non-actuating)
    public void insertDailyProgress(DailyProgressRow row) throws SQLException {
        String sql = """
            INSERT INTO dailyprogress (
              domain_id,
              subtask_id,
              yyyymmdd,
              node_id,
              energyreqj,
              r_energy,
              r_hydraulics,
              r_carbon,
              r_uncertainty,
              vt_before,
              vt_after,
              delta_vt,
              evidencehex,
              signingdid,
              priorpointerhex,
              createdutc
            ) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
            """;

        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            String dateStr = row.yyyymmdd.toString().replace("-", "");
            String createdUtc = OffsetDateTime.now(ZoneOffset.UTC).toString();

            ps.setString(1, row.domainId);
            ps.setString(2, row.subtaskId);
            ps.setString(3, dateStr);
            ps.setString(4, row.nodeId);
            ps.setDouble(5, row.energyReqJ);
            ps.setDouble(6, row.rEnergy);
            ps.setDouble(7, row.rHydraulics);
            ps.setDouble(8, row.rCarbon);
            ps.setDouble(9, row.rUncertainty);
            ps.setDouble(10, row.vtBefore);
            ps.setDouble(11, row.vtAfter);
            ps.setDouble(12, row.deltaVt);
            ps.setString(13, row.evidenceHex);
            ps.setString(14, row.signingDid);
            if (row.priorPointerHex != null) {
                ps.setString(15, row.priorPointerHex);
            } else {
                ps.setNull(15, Types.VARCHAR);
            }
            ps.setString(16, createdUtc);

            ps.executeUpdate();
        }
    }

    // Simple demonstration wiring for one node/day
    public static void main(String[] args) throws Exception {
        String dbPath = "dbcyboquaticdailyprogress.sqlite";
        Connection conn = DriverManager.getConnection("jdbc:sqlite:" + dbPath);
        CyboquaticWorkloadTelemetry telem = new CyboquaticWorkloadTelemetry(conn);
        telem.ensureSchema();

        DailyProgressRow row = new DailyProgressRow();
        row.domainId = "d";
        row.subtaskId = "PHX-CANAL-WL-2026-07-28";
        row.yyyymmdd = LocalDate.of(2026, 7, 28);
        row.nodeId = "PHX-CANAL-NODE-01";
        row.energyReqJ = 1.0e6;
        row.rEnergy = 1.0;          // corresponds to corridor edge
        row.rHydraulics = 0.35;
        row.rCarbon = 0.30;
        row.rUncertainty = 0.25;
        row.vtBefore = 0.35;
        row.vtAfter = 0.42;
        row.deltaVt = row.vtAfter - row.vtBefore;
        row.evidenceHex = "0x20260728PHXWORKLOADENERGYDV";
        row.signingDid = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";
        row.priorPointerHex = "0x20260709PHX3345NWorkloadEnergyDeltaVt";

        telem.insertDailyProgress(row);
        conn.close();
    }
}
