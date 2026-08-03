// File: java/eco/EcoSchemaValidator.java
package eco;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;

public class EcoSchemaValidator {

    private final Connection conn;

    public EcoSchemaValidator(String dbPath) throws SQLException {
        String url = "jdbc:sqlite:" + dbPath;
        this.conn = DriverManager.getConnection(url);
        this.conn.createStatement().execute("PRAGMA foreign_keys = ON;");
    }

    public void close() throws SQLException {
        conn.close();
    }

    private boolean tableExists(String name) throws SQLException {
        String sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='" + name + "';";
        try (Statement st = conn.createStatement();
             ResultSet rs = st.executeQuery(sql)) {
            return rs.next();
        }
    }

    private void assertTable(String name) throws SQLException {
        if (!tableExists(name)) {
            System.out.println("[SCHEMA FAIL] Missing table: " + name);
        } else {
            System.out.println("[SCHEMA OK] Table present: " + name);
        }
    }

    private void checkMcpSchema() throws SQLException {
        System.out.println("=== MCP Schema ===");
        assertTable("mcp_repo");
        assertTable("mcp_file");
        assertTable("mcp_tool");
        assertTable("mcp_endpoint");
    }

    private void checkCanalSchema() throws SQLException {
        System.out.println("=== Canal / Workload Schema ===");
        assertTable("canal_node");
        assertTable("cyboquatic_workload_telemetry");
        assertTable("canal_ker_canal_invariant");
    }

    private void checkPfasSchema() throws SQLException {
        System.out.println("=== PFAS Corridor Schema ===");
        assertTable("pfas_corridor_state");
    }

    private void checkHexRegistrySchema() throws SQLException {
        System.out.println("=== Phoenix Hex Registry Schema ===");
        assertTable("phoenix_hex_registry");
    }

    private void checkForeignKeys() throws SQLException {
        System.out.println("=== Foreign Key Integrity ===");
        // Simple foreign key checks for key eco tables.
        String[][] tables = {
            {"mcp_file", "repoid"},
            {"mcp_tool", "repoid"},
            {"mcp_tool", "fileid"},
            {"cyboquatic_workload_telemetry", "node_id"},
            {"canal_ker_canal_invariant", "node_id"},
            {"pfas_corridor_state", "node_id"}
        };
        for (String[] t : tables) {
            String table = t[0];
            String fkcol = t[1];
            if (!tableExists(table)) continue;
            String sql = "PRAGMA foreign_key_list('" + table + "');";
            try (Statement st = conn.createStatement();
                 ResultSet rs = st.executeQuery(sql)) {
                boolean found = false;
                while (rs.next()) {
                    String fromCol = rs.getString("from");
                    if (fkcol.equals(fromCol)) {
                        found = true;
                        break;
                    }
                }
                if (found) {
                    System.out.println("[FK OK] " + table + "." + fkcol + " has a foreign key.");
                } else {
                    System.out.println("[FK FAIL] " + table + "." + fkcol + " missing foreign key.");
                }
            }
        }
    }

    private void checkCheckConstraints() throws SQLException {
        System.out.println("=== CHECK Constraints (Sample) ===");
        // For SQLite, we inspect table SQL definitions for key CHECK constraints
        // described in governance docs.[11][59]
        String[] tables = {
            "canal_node",
            "cyboquatic_workload_telemetry",
            "canal_ker_canal_invariant",
            "phoenix_hex_registry"
        };
        for (String table : tables) {
            if (!tableExists(table)) continue;
            String sql = "SELECT sql FROM sqlite_master WHERE type='table' AND name='" + table + "';";
            try (Statement st = conn.createStatement();
                 ResultSet rs = st.executeQuery(sql)) {
                if (rs.next()) {
                    String ddl = rs.getString("sql");
                    boolean ok = true;
                    if ("canal_node".equals(table)) {
                        ok &= ddl.contains("ker_band IN ('RESEARCH', 'EXPPROD', 'PROD')");
                        ok &= ddl.contains("fog_band IN ('COLD_SURVIVAL_MONITOR','RESTORATION_PREFERRED','NEEDS_DIAGNOSTIC')");
                    }
                    if ("cyboquatic_workload_telemetry".equals(table)) {
                        ok &= ddl.contains("deltaVt >= 0.0 AND deltaVt <= 1.0");
                        ok &= ddl.contains("topo_stress_norm >= 0.0 AND topo_stress_norm <= 1.0");
                    }
                    if ("canal_ker_canal_invariant".equals(table)) {
                        ok &= ddl.contains("k_value >= 0.0 AND k_value <= 1.0");
                        ok &= ddl.contains("e_value >= 0.0 AND e_value <= 1.0");
                        ok &= ddl.contains("r_value >= 0.0 AND r_value <= 1.0");
                    }
                    if ("phoenix_hex_registry".equals(table)) {
                        ok &= ddl.contains("r_hydraulics");
                        ok &= ddl.contains("r_energy");
                        ok &= ddl.contains("r_topology");
                        ok &= ddl.contains("r_biodiversity");
                    }
                    if (ok) {
                        System.out.println("[CHECK OK] Key constraints present on " + table);
                    } else {
                        System.out.println("[CHECK WARN] Some expected constraints missing on " + table);
                    }
                }
            }
        }
    }

    public void validateAll() throws SQLException {
        checkMcpSchema();
        checkCanalSchema();
        checkPfasSchema();
        checkHexRegistrySchema();
        checkForeignKeys();
        checkCheckConstraints();
    }

    public static void main(String[] args) {
        if (args.length < 1) {
            System.out.println("Usage: EcoSchemaValidator <sqlite-db-path>");
            return;
        }
        String dbPath = args[0];
        try {
            EcoSchemaValidator validator = new EcoSchemaValidator(dbPath);
            validator.validateAll();
            validator.close();
        } catch (SQLException ex) {
            ex.printStackTrace();
        }
    }
}
