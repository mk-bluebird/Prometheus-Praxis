// File: java/cyboquatic/WorkloadTelemetryCollector.java
package cyboquatic;

import java.sql.*;
import java.time.Instant;
import java.util.Properties;

public class WorkloadTelemetryCollector {

    private final Connection conn;

    public WorkloadTelemetryCollector(String dbPath) throws SQLException {
        String url = "jdbc:sqlite:" + dbPath;
        Properties props = new Properties();
        this.conn = DriverManager.getConnection(url, props);
        this.conn.setAutoCommit(false);
        ensureSchema();
    }

    private void ensureSchema() throws SQLException {
        try (Statement st = conn.createStatement()) {
            st.executeUpdate(
                "CREATE TABLE IF NOT EXISTS cyboquatic_workload_telemetry (" +
                "    sample_id INTEGER PRIMARY KEY AUTOINCREMENT," +
                "    timestamp_utc TEXT NOT NULL," +
                "    canal_node TEXT NOT NULL," +
                "    energyreqJ REAL NOT NULL CHECK (energyreqJ >= 0.0)," +
                "    energy_input_J REAL NOT NULL CHECK (energy_input_J >= 0.0)," +
                "    deltaVt REAL NOT NULL CHECK (deltaVt >= 0.0 AND deltaVt <= 1.0)," +
                "    topo_stress_norm REAL NOT NULL CHECK (topo_stress_norm >= 0.0 AND topo_stress_norm <= 1.0)," +
                "    ker_hint TEXT NULL" +
                ");"
            );
        }
        conn.commit();
    }

    public void insertSample(String canalNode,
                             double energyreqJ,
                             double energyInputJ,
                             double deltaVt,
                             double topoStressNorm,
                             String kerHint) throws SQLException {
        String sql = "INSERT INTO cyboquatic_workload_telemetry" +
                     " (timestamp_utc, canal_node, energyreqJ, energy_input_J, deltaVt, topo_stress_norm, ker_hint)" +
                     " VALUES (?, ?, ?, ?, ?, ?, ?);";
        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setString(1, Instant.now().toString());
            ps.setString(2, canalNode);
            ps.setDouble(3, energyreqJ);
            ps.setDouble(4, energyInputJ);
            ps.setDouble(5, deltaVt);
            ps.setDouble(6, topoStressNorm);
            if (kerHint != null) {
                ps.setString(7, kerHint);
            } else {
                ps.setNull(7, Types.VARCHAR);
            }
            ps.executeUpdate();
        }
        conn.commit();
    }

    public void printEcoSafeSummary(String canalNode) throws SQLException {
        String sql = "SELECT AVG(deltaVt) AS avg_deltaVt," +
                     "       AVG(energy_input_J) AS avg_energy_input_J," +
                     "       COUNT(*) AS n_samples" +
                     "  FROM cyboquatic_workload_telemetry" +
                     " WHERE canal_node = ?;";
        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setString(1, canalNode);
            try (ResultSet rs = ps.executeQuery()) {
                if (rs.next()) {
                    double avgDeltaVt = rs.getDouble("avg_deltaVt");
                    double avgEnergyInputJ = rs.getDouble("avg_energy_input_J");
                    int n = rs.getInt("n_samples");
                    System.out.println("Canal node " + canalNode +
                                       " eco summary: n=" + n +
                                       " avgDeltaVt=" + avgDeltaVt +
                                       " avgEnergyInputJ=" + avgEnergyInputJ);
                }
            }
        }
    }

    public void close() throws SQLException {
        conn.close();
    }

    public static void main(String[] args) {
        try {
            WorkloadTelemetryCollector collector =
                new WorkloadTelemetryCollector("eco_restoration_workload.sqlite");

            collector.insertSample("PHX_CANAL_NODE_A", 1.2e6, 1.5e6, 0.45, 0.30, "KER:energy/topology corridor safe");
            collector.insertSample("PHX_CANAL_NODE_A", 1.0e6, 1.3e6, 0.40, 0.28, "KER:energy/topology corridor safe");

            collector.printEcoSafeSummary("PHX_CANAL_NODE_A");
            collector.close();
        } catch (SQLException ex) {
            ex.printStackTrace();
        }
    }
}
