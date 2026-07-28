// filename: src/java/org/prometheuspraxis/cyboquatic/CyboquaticWorkloadReportWriter.java
// license: MIT OR Apache-2.0
// role: Non-actuating workload KER and lane decision report generator for cyboquatic nodes.
// note: Uses only Java standard library and JDBC; no external dependencies, no device IO, no actuation.

package org.prometheuspraxis.cyboquatic;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.nio.charset.StandardCharsets;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;

public final class CyboquaticWorkloadReportWriter {

    public static final class WorkloadSample {
        public final String nodeId;
        public final String windowStartUtc;
        public final String windowEndUtc;
        public final double k;
        public final double e;
        public final double r;
        public final double vt;
        public final double alwaysImproveScore;
        public final String lane;
        public final boolean safeToPromote;

        public WorkloadSample(
                String nodeId,
                String windowStartUtc,
                String windowEndUtc,
                double k,
                double e,
                double r,
                double vt,
                double alwaysImproveScore,
                String lane,
                boolean safeToPromote
        ) {
            this.nodeId = nodeId;
            this.windowStartUtc = windowStartUtc;
            this.windowEndUtc = windowEndUtc;
            this.k = k;
            this.e = e;
            this.r = r;
            this.vt = vt;
            this.alwaysImproveScore = alwaysImproveScore;
            this.lane = lane;
            this.safeToPromote = safeToPromote;
        }
    }

    private CyboquaticWorkloadReportWriter() {
        // Utility class; no instances.
    }

    /**
     * Generate an HTML report from the dbcyboquaticdailyprogress.sqlite database.
     *
     * @param dbPath      Path to SQLite DB (e.g., workspacedb/dbcyboquaticdailyprogress.sqlite).
     * @param outputHtml  Destination HTML file.
     */
    public static void writeHtmlReport(String dbPath, File outputHtml) throws Exception {
        WorkloadSummary summary = loadSummary(dbPath);
        try (BufferedWriter writer = new BufferedWriter(
                new FileWriter(outputHtml, StandardCharsets.UTF_8))) {

            writer.write("<!DOCTYPE html>");
            writer.write("<html lang=\"en\">");
            writer.write("<head>");
            writer.write("<meta charset=\"UTF-8\"/>");
            writer.write("<title>Cyboquatic Workload KER and Lane Report</title>");
            writer.write("<style>");
            writer.write("body{font-family:system-ui,-apple-system,sans-serif;margin:2rem;}");
            writer.write("h1,h2{color:#1f2937;}");
            writer.write(".summary{margin-bottom:2rem;padding:1rem;border-radius:0.5rem;}");
            writer.write(".summary-ok{background:#ecfdf5;border:1px solid #6ee7b7;}");
            writer.write(".summary-warn{background:#fef2f2;border:1px solid #fca5a5;}");
            writer.write("table{border-collapse:collapse;width:100%;}");
            writer.write("th,td{border:1px solid #e5e7eb;padding:0.5rem;text-align:left;font-size:0.875rem;}");
            writer.write("th{background:#f9fafb;}");
            writer.write("</style>");
            writer.write("</head>");
            writer.write("<body>");

            writer.write("<h1>Cyboquatic Workload KER and Lane Report</h1>");

            String summaryClass = summary.allSafeToPromote ? "summary-ok" : "summary-warn";
            writer.write("<div class=\"summary " + summaryClass + "\">");
            writer.write("<h2>Summary</h2>");
            writer.write("<p><strong>Samples evaluated:</strong> " + summary.totalSamples + "</p>");
            writer.write("<p><strong>Nodes (unique):</strong> " + summary.uniqueNodes + "</p>");
            writer.write("<p><strong>Safe-to-promote samples:</strong> " + summary.safeToPromoteCount + "</p>");
            writer.write("<p><strong>Production lane samples:</strong> " + summary.productionSamples + "</p>");
            if (summary.worstSample != null) {
                writer.write("<p><strong>Worst AlwaysImprove score:</strong> "
                        + formatDouble(summary.worstAlwaysImproveScore)
                        + " (node "
                        + escapeHtml(summary.worstSample.nodeId)
                        + ", window "
                        + escapeHtml(summary.worstSample.windowStartUtc)
                        + " → "
                        + escapeHtml(summary.worstSample.windowEndUtc)
                        + ")</p>");
            }
            writer.write("</div>");

            writer.write("<h2>Workload Samples</h2>");
            writer.write("<table>");
            writer.write("<thead><tr>");
            writer.write("<th>Node</th>");
            writer.write("<th>Window Start (UTC)</th>");
            writer.write("<th>Window End (UTC)</th>");
            writer.write("<th>Lane</th>");
            writer.write("<th>K</th>");
            writer.write("<th>E</th>");
            writer.write("<th>R</th>");
            writer.write("<th>Vt</th>");
            writer.write("<th>AlwaysImprove</th>");
            writer.write("<th>Safe to Promote?</th>");
            writer.write("</tr></thead>");
            writer.write("<tbody>");
            for (WorkloadSample s : summary.samples) {
                boolean ok = s.safeToPromote;
                writer.write("<tr>");
                writer.write("<td>" + escapeHtml(s.nodeId) + "</td>");
                writer.write("<td>" + escapeHtml(s.windowStartUtc) + "</td>");
                writer.write("<td>" + escapeHtml(s.windowEndUtc) + "</td>");
                writer.write("<td>" + escapeHtml(s.lane) + "</td>");
                writer.write("<td>" + formatDouble(s.k) + "</td>");
                writer.write("<td>" + formatDouble(s.e) + "</td>");
                writer.write("<td>" + formatDouble(s.r) + "</td>");
                writer.write("<td>" + formatDouble(s.vt) + "</td>");
                writer.write("<td>" + formatDouble(s.alwaysImproveScore) + "</td>");
                writer.write("<td>" + (ok ? "YES" : "NO") + "</td>");
                writer.write("</tr>");
            }
            writer.write("</tbody>");
            writer.write("</table>");

            writer.write("</body>");
            writer.write("</html>");
        }
    }

    private static final class WorkloadSummary {
        final java.util.List<WorkloadSample> samples;
        final int totalSamples;
        final int uniqueNodes;
        final int safeToPromoteCount;
        final int productionSamples;
        final boolean allSafeToPromote;
        final double worstAlwaysImproveScore;
        final WorkloadSample worstSample;

        WorkloadSummary(
                java.util.List<WorkloadSample> samples,
                int totalSamples,
                int uniqueNodes,
                int safeToPromoteCount,
                int productionSamples,
                boolean allSafeToPromote,
                double worstAlwaysImproveScore,
                WorkloadSample worstSample
        ) {
            this.samples = samples;
            this.totalSamples = totalSamples;
            this.uniqueNodes = uniqueNodes;
            this.safeToPromoteCount = safeToPromoteCount;
            this.productionSamples = productionSamples;
            this.allSafeToPromote = allSafeToPromote;
            this.worstAlwaysImproveScore = worstAlwaysImproveScore;
            this.worstSample = worstSample;
        }
    }

    // Load workload samples and compute simple summary statistics from SQLite.
    private static WorkloadSummary loadSummary(String dbPath) throws SQLException {
        java.util.List<WorkloadSample> samples = new java.util.ArrayList<>();
        java.util.Set<String> nodes = new java.util.HashSet<>();

        int safeCount = 0;
        int prodCount = 0;
        double worstScore = 1.0;
        WorkloadSample worstSample = null;

        String url = "jdbc:sqlite:" + dbPath;
        try (Connection conn = DriverManager.getConnection(url)) {
            // Example dailyprogress schema: adjust column names to match actual DB.
            String sql =
                    "SELECT node_id, window_start_utc, window_end_utc, " +
                    "       k_knowledge AS k, e_ecoimpact AS e, r_risk AS r, " +
                    "       vt, always_improve_score, lane, safetopromote_ok " +
                    "FROM dailyprogress";
            try (PreparedStatement ps = conn.prepareStatement(sql)) {
                try (ResultSet rs = ps.executeQuery()) {
                    while (rs.next()) {
                        String nodeId = rs.getString("node_id");
                        String startUtc = rs.getString("window_start_utc");
                        String endUtc = rs.getString("window_end_utc");
                        double k = rs.getDouble("k");
                        double e = rs.getDouble("e");
                        double r = rs.getDouble("r");
                        double vt = rs.getDouble("vt");
                        double aiScore = rs.getDouble("always_improve_score");
                        String lane = rs.getString("lane");
                        boolean safe = rs.getInt("safetopromote_ok") != 0;

                        WorkloadSample sample = new WorkloadSample(
                                nodeId,
                                startUtc,
                                endUtc,
                                k,
                                e,
                                r,
                                vt,
                                aiScore,
                                lane,
                                safe
                        );
                        samples.add(sample);
                        nodes.add(nodeId);

                        if (safe) {
                            safeCount++;
                        }
                        if ("PRODUCTION".equalsIgnoreCase(lane)) {
                            prodCount++;
                        }
                        if (aiScore < worstScore) {
                            worstScore = aiScore;
                            worstSample = sample;
                        }
                    }
                }
            }
        }

        int total = samples.size();
        boolean allSafe = (total > 0) && (safeCount == total);
        return new WorkloadSummary(
                samples,
                total,
                nodes.size(),
                safeCount,
                prodCount,
                allSafe,
                worstScore,
                worstSample
        );
    }

    private static String formatDouble(double d) {
        return String.format(java.util.Locale.US, "%.3f", d);
    }

    private static String escapeHtml(String s) {
        if (s == null) {
            return "";
        }
        String out = s;
        out = out.replace("&", "&amp;");
        out = out.replace("<", "&lt;");
        out = out.replace(">", "&gt;");
        out = out.replace("\"", "&quot;");
        return out;
    }

    /**
     * Minimal CLI entry point.
     * Usage: java org.prometheuspraxis.cyboquatic.CyboquaticWorkloadReportWriter db_path output_html
     */
    public static void main(String[] args) throws Exception {
        if (args.length != 2) {
            System.err.println("Usage: java org.prometheuspraxis.cyboquatic.CyboquaticWorkloadReportWriter <db_path> <output_html>");
            System.exit(1);
        }
        String dbPath = args[0];
        File out = new File(args[1]);
        writeHtmlReport(dbPath, out);
        System.out.println("Report written to " + out.getAbsolutePath());
    }
}
