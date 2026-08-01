// File: java/cyboquatic/TelemetryClient.java
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;

public class TelemetryClient {

    private final String dbPath;

    public TelemetryClient(String dbPath) {
        this.dbPath = dbPath;
    }

    private Connection connect() throws SQLException {
        String url = "jdbc:sqlite:" + dbPath;
        return DriverManager.getConnection(url);
    }

    public void insertFogFlow(String hexId,
                              String canalNodeId,
                              double fogConcentration,
                              double unmodeledMediaFlag,
                              double flowRateLps) throws SQLException {
        String sql = "INSERT INTO fog_flow("
                   + "hex_id, canal_node_id, fog_concentration_mgL, "
                   + "unmodeled_media_flag, flow_rate_Lps"
                   + ") VALUES (?,?,?,?,?);";
        try (Connection conn = connect();
             PreparedStatement pstmt = conn.prepareStatement(sql)) {
            pstmt.setString(1, hexId);
            pstmt.setString(2, canalNodeId);
            pstmt.setDouble(3, fogConcentration);
            pstmt.setDouble(4, unmodeledMediaFlag);
            pstmt.setDouble(5, flowRateLps);
            pstmt.executeUpdate();
        }
    }

    public void printHighRiskCycles(double maxAllowedR, double maxEnergyreqJ) throws SQLException {
        String sql = "SELECT cycle_id, hex_id, canal_node_id, energyreqJ, R "
                   + "FROM workload_cycle "
                   + "WHERE R > ? OR energyreqJ > ?;";
        try (Connection conn = connect();
             PreparedStatement pstmt = conn.prepareStatement(sql)) {
            pstmt.setDouble(1, maxAllowedR);
            pstmt.setDouble(2, maxEnergyreqJ);
            try (ResultSet rs = pstmt.executeQuery()) {
                while (rs.next()) {
                    int cycleId = rs.getInt("cycle_id");
                    String hexId = rs.getString("hex_id");
                    String canalNodeId = rs.getString("canal_node_id");
                    double energyreqJ = rs.getDouble("energyreqJ");
                    double R = rs.getDouble("R");
                    System.out.println("Cycle " + cycleId
                            + " hex=" + hexId
                            + " node=" + canalNodeId
                            + " energyreqJ=" + energyreqJ
                            + " R=" + R);
                }
            }
        }
    }

    public static void main(String[] args) {
        if (args.length < 1) {
            System.err.println("Usage: TelemetryClient <db_path>");
            return;
        }
        String dbPath = args[0];
        TelemetryClient client = new TelemetryClient(dbPath);
        try {
            client.insertFogFlow("phoenix_hex_001", "canal_node_001",
                    150.0, 1.0, 10.0);
            client.printHighRiskCycles(0.7, 50000.0);
        } catch (SQLException e) {
            e.printStackTrace();
        }
    }
}
