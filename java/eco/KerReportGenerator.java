// File: java/eco/KerReportGenerator.java
package eco;

import java.sql.*;
import java.time.Instant;
import java.util.ArrayList;
import java.util.List;

public class KerReportGenerator {

    private final Connection conn;

    public KerReportGenerator(String dbPath) throws SQLException {
        String url = "jdbc:sqlite:" + dbPath;
        this.conn = DriverManager.getConnection(url);
        this.conn.createStatement().execute("PRAGMA foreign_keys = ON;");
    }

    public void close() throws SQLException {
        conn.close();
    }

    static class HexKerRow {
        String hexId;
        double rHydraulics;
        double rEnergy;
        double rTopology;
        double rBiodiversity;
        double Vt;
        double k;
        double e;
        double s;
    }

    private List<HexKerRow> loadHexKerRows() throws SQLException {
        String sql = """
            SELECT hex_id,
                   r_hydraulics,
                   r_energy,
                   r_topology,
                   r_biodiversity,
                   w_h, w_e, w_t, w_b
            FROM phoenix_hex_registry;
        """;
        List<HexKerRow> rows = new ArrayList<>();
        try (Statement st = conn.createStatement();
             ResultSet rs = st.executeQuery(sql)) {
            while (rs.next()) {
                HexKerRow row = new HexKerRow();
                row.hexId = rs.getString("hex_id");
                row.rHydraulics = rs.getDouble("r_hydraulics");
                row.rEnergy = rs.getDouble("r_energy");
                row.rTopology = rs.getDouble("r_topology");
                row.rBiodiversity = rs.getDouble("r_biodiversity");
                double wH = rs.getDouble("w_h");
                double wE = rs.getDouble("w_e");
                double wT = rs.getDouble("w_t");
                double wB = rs.getDouble("w_b");
                row.Vt = wH * row.rHydraulics * row.rHydraulics
                       + wE * row.rEnergy * row.rEnergy
                       + wT * row.rTopology * row.rTopology
                       + wB * row.rBiodiversity * row.rBiodiversity;
                double rMax = Math.max(
                    Math.max(row.rHydraulics, row.rEnergy),
                    Math.max(row.rTopology, row.rBiodiversity)
                );
                row.k = 0.9; // example K band
                row.e = Math.max(0.0, 1.0 - rMax);
                row.s = row.k * row.e - rMax;
                rows.add(row);
            }
        }
        return rows;
    }

    public String generateTextReport() throws SQLException {
        StringBuilder sb = new StringBuilder();
        sb.append("Eco KER Corridor Report\n");
        sb.append("Generated at: ").append(Instant.now().toString()).append("\n");
        sb.append("Owner DID: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7\n\n");

        List<HexKerRow> rows = loadHexKerRows();
        for (HexKerRow row : rows) {
            sb.append("Hex ").append(row.hexId).append("\n");
            sb.append("  r_hydraulics  = ").append(String.format("%.3f", row.rHydraulics)).append("\n");
            sb.append("  r_energy      = ").append(String.format("%.3f", row.rEnergy)).append("\n");
            sb.append("  r_topology    = ").append(String.format("%.3f", row.rTopology)).append("\n");
            sb.append("  r_biodiversity= ").append(String.format("%.3f", row.rBiodiversity)).append("\n");
            sb.append("  Vt            = ").append(String.format("%.4f", row.Vt)).append("\n");
            sb.append("  k,e,ker_score = k=").append(String.format("%.3f", row.k))
              .append(" e=").append(String.format("%.3f", row.e))
              .append(" ker=").append(String.format("%.4f", row.s)).append("\n");
            sb.append("  corridor_health: ");
            if (row.s > 0.0) {
                sb.append("SAFE (positive KER, Lyapunov drift expected negative)\n");
            } else {
                sb.append("RISK (non-positive KER, corridor requires attention)\n");
            }
            sb.append("\n");
        }

        return sb.toString();
    }

    public String generateHtmlReport() throws SQLException {
        StringBuilder sb = new StringBuilder();
        sb.append("<!DOCTYPE html><html><head><meta charset=\"utf-8\">");
        sb.append("<title>Eco KER Corridor Report</title></head><body>");
        sb.append("<h1>Eco KER Corridor Report</h1>");
        sb.append("<p>Generated at: ").append(Instant.now().toString()).append("</p>");
        sb.append("<p>Owner DID: <code>bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7</code></p>");

        List<HexKerRow> rows = loadHexKerRows();
        sb.append("<table border=\"1\" cellspacing=\"0\" cellpadding=\"4\">");
        sb.append("<tr><th>Hex ID</th><th>r_hydraulics</th><th>r_energy</th><th>r_topology</th><th>r_biodiversity</th><th>Vt</th><th>k</th><th>e</th><th>ker_score</th><th>Health</th></tr>");
        for (HexKerRow row : rows) {
            String health = (row.s > 0.0) ? "SAFE" : "RISK";
            sb.append("<tr>");
            sb.append("<td>").append(row.hexId).append("</td>");
            sb.append("<td>").append(String.format("%.3f", row.rHydraulics)).append("</td>");
            sb.append("<td>").append(String.format("%.3f", row.rEnergy)).append("</td>");
            sb.append("<td>").append(String.format("%.3f", row.rTopology)).append("</td>");
            sb.append("<td>").append(String.format("%.3f", row.rBiodiversity)).append("</td>");
            sb.append("<td>").append(String.format("%.4f", row.Vt)).append("</td>");
            sb.append("<td>").append(String.format("%.3f", row.k)).append("</td>");
            sb.append("<td>").append(String.format("%.3f", row.e)).append("</td>");
            sb.append("<td>").append(String.format("%.4f", row.s)).append("</td>");
            sb.append("<td>").append(health).append("</td>");
            sb.append("</tr>");
        }
        sb.append("</table>");
        sb.append("</body></html>");
        return sb.toString();
    }

    public static void main(String[] args) {
        if (args.length < 2) {
            System.out.println("Usage: KerReportGenerator <sqlite-db-path> <mode>");
            System.out.println("  mode: text | html");
            return;
        }
        String dbPath = args[0];
        String mode = args[1];
        try {
            KerReportGenerator gen = new KerReportGenerator(dbPath);
            if ("text".equalsIgnoreCase(mode)) {
                System.out.println(gen.generateTextReport());
            } else if ("html".equalsIgnoreCase(mode)) {
                System.out.println(gen.generateHtmlReport());
            } else {
                System.out.println("Unknown mode: " + mode);
            }
            gen.close();
        } catch (SQLException ex) {
            ex.printStackTrace();
        }
    }
}
