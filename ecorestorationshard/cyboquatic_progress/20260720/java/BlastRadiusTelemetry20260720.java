// filename: ecorestorationshard/cyboquatic_progress/20260720/java/BlastRadiusTelemetry20260720.java
// destination: ecorestorationshard/cyboquatic_progress/20260720/java/BlastRadiusTelemetry20260720.java
// domain: g (blast-radius surcharge envelopes).[file:2]

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.SQLException;

public final class BlastRadiusTelemetry20260720 {

    private static final String DB_PATH =
            "jdbc:sqlite:ecorestorationshard/db/dbcyboquaticdailyprogress.sqlite"; // shared DB.[file:2]

    public static void insertBlastRadiusSample(
            String yyyymmdd,
            String canalSegmentId,
            String canalNodeId,
            double surchargeLevelM,
            double breachProb,
            double radiusM,
            String impactClass,
            String kerBandTag,
            String evidenceHex
    ) throws SQLException {
        String sql = "INSERT INTO blastradius_surcharge (" +
                "yyyymmdd, canal_segment_id, canal_node_id, " +
                "surcharge_level_m, breach_prob, radius_m, " +
                "impact_class, ker_band_tag, evidence_hex, created_utc" +
                ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))";

        try (Connection conn = DriverManager.getConnection(DB_PATH);
             PreparedStatement ps = conn.prepareStatement(sql)) {

            ps.setString(1, yyyymmdd);
            ps.setString(2, canalSegmentId);
            ps.setString(3, canalNodeId);
            ps.setDouble(4, surchargeLevelM);
            ps.setDouble(5, breachProb);
            ps.setDouble(6, radiusM);
            ps.setString(7, impactClass);
            ps.setString(8, kerBandTag);
            ps.setString(9, evidenceHex);

            ps.executeUpdate();
        }
    }

    public static void main(String[] args) {
        try {
            insertBlastRadiusSample(
                    "20260720",
                    "PHX-CANAL-SEG-001",
                    "CANAL-PHX-SEG-001",
                    0.30,
                    0.05,
                    25.0,
                    "LOW",
                    "PHXBLASTRADIUS20260720",
                    "0x20260720PHXBLASTRADIUSSEG001"
            );
        } catch (SQLException e) {
            e.printStackTrace();
        }
    }
}
