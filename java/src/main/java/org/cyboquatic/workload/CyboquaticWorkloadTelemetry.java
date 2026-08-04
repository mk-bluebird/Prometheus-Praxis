// File: java/src/main/java/org/cyboquatic/workload/CyboquaticWorkloadTelemetry.java
package org.cyboquatic.workload;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.SQLException;

/**
 * Java telemetry sink for Cyboquatic workloads.
 *
 * This class inserts workload samples into SQL telemetry tables, assuming
 * the schema defined in the corresponding SQL artifact.
 */
public class CyboquaticWorkloadTelemetry {

    private final String jdbcUrl;

    public CyboquaticWorkloadTelemetry(String jdbcUrl) {
        this.jdbcUrl = jdbcUrl;
    }

    public void insertSample(String basinId,
                             double timestampS,
                             double flowRateM3S,
                             double headM,
                             double motorEfficiency,
                             double aerationFactor,
                             double energyreqJ,
                             double deltaVtMS) throws SQLException {
        try (Connection conn = DriverManager.getConnection(jdbcUrl)) {
            conn.setAutoCommit(false);
            String sql = "INSERT INTO cyboquatic_workload_telemetry (" +
                         "basin_id, timestamp_s, flow_rate_m3_s, head_m, motor_efficiency," +
                         "aeration_factor, energyreq_j, delta_vt_m_s) " +
                         "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
            try (PreparedStatement ps = conn.prepareStatement(sql)) {
                ps.setString(1, basinId);
                ps.setDouble(2, timestampS);
                ps.setDouble(3, flowRateM3S);
                ps.setDouble(4, headM);
                ps.setDouble(5, motorEfficiency);
                ps.setDouble(6, aerationFactor);
                ps.setDouble(7, energyreqJ);
                ps.setDouble(8, deltaVtMS);
                ps.executeUpdate();
            }
            conn.commit();
        }
    }

    public static void main(String[] args) {
        String jdbcUrl = "jdbc:sqlite:./data/cyboquatic_workload.db";
        CyboquaticWorkloadTelemetry telemetry = new CyboquaticWorkloadTelemetry(jdbcUrl);
        try {
            telemetry.insertSample(
                "basin-A",
                0.0,
                0.15,
                4.0,
                0.75,
                0.6,
                4418.99225,
                0.06
            );
        } catch (SQLException e) {
            e.printStackTrace();
        }
    }
}
