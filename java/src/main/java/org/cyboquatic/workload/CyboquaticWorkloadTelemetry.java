// File: java/src/main/java/org/cyboquatic/workload/CyboquaticWorkloadTelemetry.java
package org.cyboquatic.workload;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.SQLException;

/**
 * Java telemetry sink for Cyboquatic workloads.
 *
 * Inserts workload samples into the cyboquatic_workload_telemetry table.
 * If a SQLite trigger enforces ker_e <= 0 and aborts on violation, this class
 * catches the error and invokes a Kotlin-based eco-audit service via EcoAuditBridge.
 */
public class CyboquaticWorkloadTelemetry {

    private final String jdbcUrl;

    public CyboquaticWorkloadTelemetry(String jdbcUrl) {
        this.jdbcUrl = jdbcUrl;
    }

    /**
     * Insert a full telemetry sample including KER fields and DID binding.
     * This is the preferred production method.
     */
    public void insertSample(String did,
                             String basinId,
                             double timestampS,
                             double flowRateM3S,
                             double headM,
                             double motorEfficiency,
                             double aerationFactor,
                             double energyreqJ,
                             double deltaVtMS,
                             double kerK,
                             double kerE,
                             double kerR) throws SQLException {
        try (Connection conn = DriverManager.getConnection(jdbcUrl)) {
            conn.setAutoCommit(false);
            String sql =
                "INSERT INTO cyboquatic_workload_telemetry (" +
                "did, basin_id, timestamp_s, flow_rate_m3_s, head_m, motor_efficiency," +
                "aeration_factor, energyreq_j, delta_vt_m_s, ker_k, ker_e, ker_r) " +
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
            try (PreparedStatement ps = conn.prepareStatement(sql)) {
                ps.setString(1, did);
                ps.setString(2, basinId);
                ps.setDouble(3, timestampS);
                ps.setDouble(4, flowRateM3S);
                ps.setDouble(5, headM);
                ps.setDouble(6, motorEfficiency);
                ps.setDouble(7, aerationFactor);
                ps.setDouble(8, energyreqJ);
                ps.setDouble(9, deltaVtMS);
                ps.setDouble(10, kerK);
                ps.setDouble(11, kerE);
                ps.setDouble(12, kerR);
                ps.executeUpdate();
            }
            conn.commit();
        } catch (SQLException e) {
            if (e.getMessage() != null && e.getMessage().contains("ker_e must be <= 0")) {
                EcoAuditBridge.logKerEViolation(did, basinId, timestampS, kerE, e.getMessage());
            }
            throw e;
        }
    }

    /**
     * Convenience overload for legacy callers that do not supply DID/KER.
     * Uses default DID and derives KER values externally or via downstream processes.
     */
    public void insertSample(String basinId,
                             double timestampS,
                             double flowRateM3S,
                             double headM,
                             double motorEfficiency,
                             double aerationFactor,
                             double energyreqJ,
                             double deltaVtMS) throws SQLException {
        String defaultDid = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";
        double defaultKerK = 0.9;
        double defaultKerE = -1.0;
        double defaultKerR = 0.2;
        insertSample(
            defaultDid,
            basinId,
            timestampS,
            flowRateM3S,
            headM,
            motorEfficiency,
            aerationFactor,
            energyreqJ,
            deltaVtMS,
            defaultKerK,
            defaultKerE,
            defaultKerR
        );
    }

    public static void main(String[] args) {
        String jdbcUrl = "jdbc:sqlite:./data/cyboquatic_workload.db";
        CyboquaticWorkloadTelemetry telemetry = new CyboquaticWorkloadTelemetry(jdbcUrl);
        try {
            telemetry.insertSample(
                "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7",
                "basin-A",
                0.0,
                0.15,
                4.0,
                0.80,
                0.6,
                5000.0,
                0.06,
                0.9,
                0.1,
                0.2
            );
        } catch (SQLException e) {
            e.printStackTrace();
        }

        try {
            telemetry.insertSample(
                "basin-B",
                3600.0,
                0.12,
                3.5,
                0.75,
                0.5,
                4200.0,
                0.05
            );
        } catch (SQLException e) {
            e.printStackTrace();
        }
    }
}
