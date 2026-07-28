// filename: ecorestorationshard/cyboquaticprogress/20260727/java/CyboquaticWorkloadTelemetry.java
// purpose: Java telemetry helper for cyboquatic workload (energyreqJ, ΔVt) writing into SQLite
// domain: (d) Cyboquatic workload energetics
// anchor: PHXWORKLOADENERGYDV20260727 / 0x20260727PHX3345NWorkloadEnergyDeltaVt

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.SQLException;
import java.time.Instant;

public final class CyboquaticWorkloadTelemetry {

    private CyboquaticWorkloadTelemetry() {
    }

    private static Connection open(String path) throws SQLException {
        String url = "jdbc:sqlite:" + path;
        return DriverManager.getConnection(url);
    }

    public static void ensureDailyProgressSchema(Connection conn) throws SQLException {
        String sql = ""
            + "CREATE TABLE IF NOT EXISTS dailyprogress (\n"
            + "  id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
            + "  yyyymmdd TEXT NOT NULL,\n"
            + "  domain TEXT NOT NULL,\n"
            + "  subtaskid TEXT NOT NULL,\n"
            + "  nodeid TEXT NOT NULL,\n"
            + "  energy_req_j REAL NOT NULL,\n"
            + "  energy_surplus_j REAL NOT NULL,\n"
            + "  r_energy REAL NOT NULL,\n"
            + "  r_hydraulics REAL NOT NULL,\n"
            + "  r_uncertainty REAL NOT NULL,\n"
            + "  vt_before REAL NOT NULL,\n"
            + "  vt_after REAL NOT NULL,\n"
            + "  delta_vt REAL NOT NULL,\n"
            + "  k_metric REAL,\n"
            + "  e_metric REAL,\n"
            + "  r_metric REAL,\n"
            + "  evidencehex TEXT NOT NULL,\n"
            + "  prioranchorhex TEXT,\n"
            + "  createdutc TEXT NOT NULL\n"
            + ");";
        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.execute();
        }
    }

    public static void insertWorkloadSlice(Connection conn,
                                           String yyyymmdd,
                                           String nodeId,
                                           double energyReqJ,
                                           double energySurplusJ,
                                           double rEnergy,
                                           double rHydraulics,
                                           double rUncertainty,
                                           double vtBefore,
                                           double vtAfter,
                                           double deltaVt,
                                           String priorAnchorHex) throws SQLException {

        String sql = ""
            + "INSERT INTO dailyprogress (\n"
            + "  yyyymmdd, domain, subtaskid, nodeid,\n"
            + "  energy_req_j, energy_surplus_j,\n"
            + "  r_energy, r_hydraulics, r_uncertainty,\n"
            + "  vt_before, vt_after, delta_vt,\n"
            + "  k_metric, e_metric, r_metric,\n"
            + "  evidencehex, prioranchorhex, createdutc\n"
            + ") VALUES (\n"
            + "  ?, ?, ?, ?,\n"
            + "  ?, ?,\n"
            + "  ?, ?, ?,\n"
            + "  ?, ?, ?,\n"
            + "  NULL, NULL, NULL,\n"
            + "  ?, ?, ?\n"
            + ");";

        String domain = "CYBOQUATIC";
        String subtaskId = "PHX-CANAL-WORKLOAD-ENERGYDV-20260727";
        String evidenceHex = "0x20260727PHX3345NWorkloadEnergyDeltaVt";
        String createdUtc = Instant.now().toString();

        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setString(1, yyyymmdd);
            ps.setString(2, domain);
            ps.setString(3, subtaskId);
            ps.setString(4, nodeId);
            ps.setDouble(5, energyReqJ);
            ps.setDouble(6, energySurplusJ);
            ps.setDouble(7, rEnergy);
            ps.setDouble(8, rHydraulics);
            ps.setDouble(9, rUncertainty);
            ps.setDouble(10, vtBefore);
            ps.setDouble(11, vtAfter);
            ps.setDouble(12, deltaVt);
            ps.setString(13, evidenceHex);
            ps.setString(14, priorAnchorHex);
            ps.setString(15, createdUtc);
            ps.executeUpdate();
        }
    }

    public static void main(String[] args) throws Exception {
        if (args.length < 9) {
            System.err.println(
                "Usage: CyboquaticWorkloadTelemetry db_path yyyymmdd nodeid "
                    + "energy_req_J energy_surplus_J r_energy r_hydraulics r_uncertainty vt_before");
            System.exit(1);
        }
        String dbPath = args[0];
        String yyyymmdd = args[1];
        String nodeId = args[2];
        double energyReqJ = Double.parseDouble(args[3]);
        double energySurplusJ = Double.parseDouble(args[4]);
        double rEnergy = Double.parseDouble(args[5]);
        double rHydraulics = Double.parseDouble(args[6]);
        double rUncertainty = Double.parseDouble(args[7]);
        double vtBefore = Double.parseDouble(args[8]);

        // vt_after and delta_vt can be computed by C++ kernel or a mirrored Java calculation.
        double vtAfter = rEnergy * rEnergy * 0.50
                       + rHydraulics * rHydraulics * 0.30
                       + rUncertainty * rUncertainty * 0.20;
        double deltaVt = vtAfter - vtBefore;

        String priorAnchorHex = "0x20260709PHX3345NWorkloadEnergyDeltaVt";

        try (Connection conn = open(dbPath)) {
            ensureDailyProgressSchema(conn);
            insertWorkloadSlice(conn, yyyymmdd, nodeId,
                                energyReqJ, energySurplusJ,
                                rEnergy, rHydraulics, rUncertainty,
                                vtBefore, vtAfter, deltaVt,
                                priorAnchorHex);
        }
    }
}
