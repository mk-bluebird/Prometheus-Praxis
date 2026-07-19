// filename: ecorestorationshard/cyboquaticprogress/20260718/java/CyboquaticDailyTelemetry.java
// purpose: Java helper to insert energyreqJ and deltaVt telemetry rows into SQLite daily progress DB.
// repo-target: https://github.com/mk-bluebird/Prometheus-Praxis

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.SQLException;

public final class CyboquaticDailyTelemetry {

    // DB path consistent with cyboquaticprogress registry. [file:2][file:3]
    private static final String DB_PATH =
            "ecorestorationshard/cyboquaticprogress/db_cyboquatic_daily_progress.sqlite";

    private CyboquaticDailyTelemetry() {
    }

    public static Connection open() throws SQLException {
        String url = "jdbc:sqlite:" + DB_PATH;
        return DriverManager.getConnection(url);
    }

    public static void insertDailyProgress(
            Connection conn,
            String yyyymmdd,
            String domainId,
            String subtaskId,
            String nodeId,
            double energyreqJ,
            double deltaVt,
            double kScore,
            double eScore,
            double rScore,
            String evidenceHex,
            String signingDid,
            String priorAnchorId
    ) throws SQLException {

        String sql = "INSERT INTO dailyprogress (" +
                     "yyyymmdd, domain, subtaskid, nodeid, " +
                     "energyreqJ, deltaVt, kscore, escore, rscore, " +
                     "evidencehex, signingdid, prioranchorid" +
                     ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setString(1, yyyymmdd);
            ps.setString(2, domainId);
            ps.setString(3, subtaskId);
            ps.setString(4, nodeId);
            ps.setDouble(5, energyreqJ);
            ps.setDouble(6, deltaVt);
            ps.setDouble(7, kScore);
            ps.setDouble(8, eScore);
            ps.setDouble(9, rScore);
            ps.setString(10, evidenceHex);
            ps.setString(11, signingDid);
            ps.setString(12, priorAnchorId);
            ps.executeUpdate();
        }
    }

    public static void main(String[] args) {
        String yyyymmdd = "20260718";
        String domainId = "WORKLOADENERGYDV";
        String subtaskId = "PHX-WORKLOAD-ENERGYREQDV-2026-07-18";
        String nodeId = "AI-RACK-PHX-01";

        double energyreqJ = 12.5 * 3.6e6;
        double deltaVt = -0.05;
        double kScore = 0.95;
        double eScore = 0.91;
        double rScore = 0.12;

        String evidenceHex = "0x20260718PHXWORKLOADENERGYREQDV";
        String signingDid = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";
        String priorAnchorId = "PHXWORKLOADENERGYDV20260709";

        try (Connection conn = open()) {
            insertDailyProgress(conn,
                                yyyymmdd,
                                domainId,
                                subtaskId,
                                nodeId,
                                energyreqJ,
                                deltaVt,
                                kScore,
                                eScore,
                                rScore,
                                evidenceHex,
                                signingDid,
                                priorAnchorId);
        } catch (SQLException e) {
            System.err.println("Error inserting daily progress: " + e.getMessage());
        }
    }
}
