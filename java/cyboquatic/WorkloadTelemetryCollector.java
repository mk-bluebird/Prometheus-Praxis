// File: java/cyboquatic/WorkloadTelemetryCollector.java
package cyboquatic;

import java.sql.*;
import java.util.Properties;

public class WorkloadTelemetryCollector {

    private final Connection conn;

    public WorkloadTelemetryCollector(String dbPath) throws SQLException {
        String url = "jdbc:sqlite:" + dbPath;
        Properties props = new Properties();
        this.conn = DriverManager.getConnection(url, props);
        this.conn.setAutoCommit(false);
    }

    public void printKerSummary() throws SQLException {
        String sql = "SELECT node_code, avg_deltaVt, avg_energy_input_J, avg_pfas_ugL, min_ker_score " +
                     "FROM v_cyboquatic_workload_ker_summary;";
        try (Statement st = conn.createStatement();
             ResultSet rs = st.executeQuery(sql)) {
            System.out.println("=== Cyboquatic Workload KER Summary ===");
            while (rs.next()) {
                String nodeCode = rs.getString("node_code");
                double avgDeltaVt = rs.getDouble("avg_deltaVt");
                double avgEnergyInputJ = rs.getDouble("avg_energy_input_J");
                double avgPfasUgL = rs.getDouble("avg_pfas_ugL");
                double minKerScore = rs.getDouble("min_ker_score");
                
                System.out.printf("Node: %s%n", nodeCode);
                System.out.printf("  avg_deltaVt: %.6f%n", avgDeltaVt);
                System.out.printf("  avg_energy_input_J: %.2f%n", avgEnergyInputJ);
                System.out.printf("  avg_pfas_ugL: %.4f%n", avgPfasUgL);
                System.out.printf("  min_ker_score: %.4f%n", minKerScore == 0.0 ? 0.0 : minKerScore);
                System.out.println();
            }
        }
    }

    public void close() throws SQLException {
        conn.close();
    }

    public static void main(String[] args) {
        String dbPath = "eco_restoration_workload.sqlite";
        String canalNode = null;
        
        // Parse --db-path and --canal-node arguments
        for (int i = 0; i < args.length; i++) {
            if (args[i].startsWith("--db-path=")) {
                dbPath = args[i].substring(10);
            } else if (args[i].startsWith("--canal-node=")) {
                canalNode = args[i].substring(13);
            }
        }
        
        try {
            WorkloadTelemetryCollector collector = new WorkloadTelemetryCollector(dbPath);
            
            if (canalNode != null && !canalNode.isEmpty()) {
                System.out.println("Filtering by canal node: " + canalNode);
            }
            
            collector.printKerSummary();
            collector.close();
        } catch (SQLException ex) {
            System.err.println("Database error: " + ex.getMessage());
            ex.printStackTrace();
        }
    }
}
