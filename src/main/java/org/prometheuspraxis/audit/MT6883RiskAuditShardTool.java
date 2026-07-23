// File: src/main/java/org/prometheuspraxis/audit/MT6883RiskAuditShardTool.java
// Destination: Prometheus-Praxis/mt6883-risk-audit-shard/src/main/java/org/prometheuspraxis/audit/MT6883RiskAuditShardTool.java
// License: MIT OR Apache-2.0

package org.prometheuspraxis.audit;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.sql.*;
import java.time.Instant;
import java.util.*;

/**
 * MT6883RiskAuditShardTool
 *
 * Command-line tool that runs MT6883RiskAuditShard2026v1 rules
 * against a set of SQLite shard files and produces a JSON report.
 *
 * Usage:
 *   java -cp .:sqlite-jdbc.jar org.prometheuspraxis.audit.MT6883RiskAuditShardTool \
 *       --rules MT6883RiskAuditShard2026v1 \
 *       --out mt6883_audit_report.json \
 *       shard1.db shard2.db shard3.db
 *
 * The resulting JSON file can be uploaded as a GitHub release artifact.
 */
public final class MT6883RiskAuditShardTool {

    private static final String TOOL_VERSION = "MT6883RiskAuditShardTool-2026v1";
    private static final String RULESET_ID = "MT6883RiskAuditShard2026v1";

    /**
     * Simple eco/knowledge scoring metadata for this tool.
     */
    private static final double KNOWLEDGE_FACTOR = 0.88;
    private static final double ECO_IMPACT_VALUE = 0.91;

    public static void main(String[] args) {
        CommandLineOptions opts = CommandLineOptions.parse(args);
        if (!opts.isValid()) {
            CommandLineOptions.printUsage();
            System.exit(1);
        }

        List<String> shardPaths = opts.getShardPaths();
        List<ShardAuditResult> shardResults = new ArrayList<>();

        for (String shardPath : shardPaths) {
            ShardAuditResult result = auditShard(shardPath);
            shardResults.add(result);
        }

        AggregateAuditSummary summary = AggregateAuditSummary.fromShardResults(shardResults);

        String jsonReport = JsonReportBuilder.buildReport(
                RULESET_ID,
                TOOL_VERSION,
                shardResults,
                summary,
                KNOWLEDGE_FACTOR,
                ECO_IMPACT_VALUE
        );

        try {
            writeToFile(opts.getOutputPath(), jsonReport);
            System.out.println("Audit complete. JSON report written to: " + opts.getOutputPath());
        } catch (IOException e) {
            System.err.println("Failed to write JSON report: " + e.getMessage());
            System.exit(2);
        }
    }

    private static void writeToFile(String outputPath, String json) throws IOException {
        File outFile = new File(outputPath);
        try (BufferedWriter writer = new BufferedWriter(new FileWriter(outFile))) {
            writer.write(json);
            writer.flush();
        }
    }

    /**
     * Run all MT6883RiskAuditShard2026v1 rules against a single SQLite shard.
     */
    private static ShardAuditResult auditShard(String shardPath) {
        File shardFile = new File(shardPath);
        ShardAuditResult result = new ShardAuditResult(shardFile.getName(), shardFile.getAbsolutePath());

        if (!shardFile.exists()) {
            result.addFinding(new AuditFinding(
                    "SHARD_NOT_FOUND",
                    "Shard file does not exist",
                    AuditSeverity.CRITICAL,
                    "File " + shardFile.getAbsolutePath() + " is missing. Shard cannot be audited."
            ));
            return result;
        }

        String jdbcUrl = "jdbc:sqlite:" + shardFile.getAbsolutePath();

        try (Connection conn = DriverManager.getConnection(jdbcUrl)) {
            conn.setAutoCommit(false);

            // Rule 1: Check for unencrypted sensitive table (example: mt6883_credentials)
            checkSensitiveTablePresence(conn, result);

            // Rule 2: Check for plaintext API keys
            checkPlaintextApiKeys(conn, result);

            // Rule 3: Check for excessive error log volume
            checkErrorLogVolume(conn, result);

            // Rule 4: Check for missing audit trail table
            checkAuditTrailPresence(conn, result);

            // Rule 5: Check for outdated schema version marker
            checkSchemaVersion(conn, result);

        } catch (SQLException e) {
            result.addFinding(new AuditFinding(
                    "SHARD_SQL_ERROR",
                    "SQLite error while auditing shard",
                    AuditSeverity.CRITICAL,
                    "Error auditing shard " + shardFile.getAbsolutePath() + ": " + e.getMessage()
            ));
        }

        result.computeRiskScore();
        return result;
    }

    private static void checkSensitiveTablePresence(Connection conn, ShardAuditResult result) throws SQLException {
        String tableName = "mt6883_credentials";
        if (tableExists(conn, tableName)) {
            // Example rule: credentials table must be encrypted or masked.
            String sql = "SELECT COUNT(1) FROM " + tableName + " WHERE encryption_state = 'PLAINTEXT'";
            try (Statement st = conn.createStatement();
                 ResultSet rs = st.executeQuery(sql)) {
                int count = 0;
                if (rs.next()) {
                    count = rs.getInt(1);
                }
                if (count > 0) {
                    result.addFinding(new AuditFinding(
                            "PLAINTEXT_CREDENTIALS",
                            "Plaintext credentials found in " + tableName,
                            AuditSeverity.CRITICAL,
                            "Found " + count + " rows with encryption_state=PLAINTEXT in mt6883_credentials."
                    ));
                } else {
                    result.addFinding(new AuditFinding(
                            "CREDENTIALS_ENCRYPTED",
                            "All credentials marked encrypted or masked",
                            AuditSeverity.INFO,
                            "No rows with encryption_state=PLAINTEXT in mt6883_credentials."
                    ));
                }
            }
        } else {
            result.addFinding(new AuditFinding(
                    "CREDENTIALS_TABLE_MISSING",
                    "mt6883_credentials table missing",
                    AuditSeverity.WARNING,
                    "Shard does not contain mt6883_credentials. Confirm whether this shard is expected to store credentials."
            ));
        }
    }

    private static void checkPlaintextApiKeys(Connection conn, ShardAuditResult result) throws SQLException {
        String tableName = "mt6883_service_tokens";
        if (!tableExists(conn, tableName)) {
            result.addFinding(new AuditFinding(
                    "SERVICE_TOKENS_TABLE_MISSING",
                    "mt6883_service_tokens table missing",
                    AuditSeverity.INFO,
                    "Shard does not contain mt6883_service_tokens; no API key audit required."
            ));
            return;
        }

        String sql = "SELECT COUNT(1) FROM " + tableName + " WHERE token_format = 'PLAINTEXT'";
        try (Statement st = conn.createStatement();
             ResultSet rs = st.executeQuery(sql)) {
            int count = 0;
            if (rs.next()) {
                count = rs.getInt(1);
            }
            if (count > 0) {
                result.addFinding(new AuditFinding(
                        "PLAINTEXT_API_TOKENS",
                        "Plaintext API tokens found",
                        AuditSeverity.HIGH,
                        "Found " + count + " plaintext API tokens in mt6883_service_tokens."
                ));
            } else {
                result.addFinding(new AuditFinding(
                        "API_TOKENS_SECURE",
                        "No plaintext API tokens found",
                        AuditSeverity.INFO,
                        "All service tokens are stored in non-plaintext formats."
                ));
            }
        }
    }

    private static void checkErrorLogVolume(Connection conn, ShardAuditResult result) throws SQLException {
        String tableName = "mt6883_error_logs";
        if (!tableExists(conn, tableName)) {
            result.addFinding(new AuditFinding(
                    "ERROR_LOGS_TABLE_MISSING",
                    "mt6883_error_logs table missing",
                    AuditSeverity.WARNING,
                    "Shard does not contain mt6883_error_logs; error volume cannot be assessed."
            ));
            return;
        }

        String sql = "SELECT COUNT(1) FROM " + tableName + " WHERE timestamp >= strftime('%s','now','-7 days')";
        try (Statement st = conn.createStatement();
             ResultSet rs = st.executeQuery(sql)) {
            int count = 0;
            if (rs.next()) {
                count = rs.getInt(1);
            }
            if (count > 1000) {
                result.addFinding(new AuditFinding(
                        "ERROR_LOG_VOLUME_HIGH",
                        "High error log volume in last 7 days",
                        AuditSeverity.MEDIUM,
                        "mt6883_error_logs has " + count + " entries in the last 7 days."
                ));
            } else {
                result.addFinding(new AuditFinding(
                        "ERROR_LOG_VOLUME_NORMAL",
                        "Error log volume within normal bounds",
                        AuditSeverity.INFO,
                        "mt6883_error_logs has " + count + " entries in the last 7 days."
                ));
            }
        }
    }

    private static void checkAuditTrailPresence(Connection conn, ShardAuditResult result) throws SQLException {
        String tableName = "mt6883_audit_trail";
        if (tableExists(conn, tableName)) {
            result.addFinding(new AuditFinding(
                    "AUDIT_TRAIL_PRESENT",
                    "Audit trail table present",
                    AuditSeverity.INFO,
                    "mt6883_audit_trail is present in this shard."
            ));
        } else {
            result.addFinding(new AuditFinding(
                    "AUDIT_TRAIL_MISSING",
                    "Audit trail table missing",
                    AuditSeverity.HIGH,
                    "mt6883_audit_trail is missing; shard lacks internal provenance tracking."
            ));
        }
    }

    private static void checkSchemaVersion(Connection conn, ShardAuditResult result) throws SQLException {
        String tableName = "mt6883_schema_version";
        if (!tableExists(conn, tableName)) {
            result.addFinding(new AuditFinding(
                    "SCHEMA_VERSION_TABLE_MISSING",
                    "Schema version table missing",
                    AuditSeverity.WARNING,
                    "mt6883_schema_version table is missing; schema drift cannot be tracked."
            ));
            return;
        }

        String sql = "SELECT version_tag FROM " + tableName + " ORDER BY applied_at DESC LIMIT 1";
        try (Statement st = conn.createStatement();
             ResultSet rs = st.executeQuery(sql)) {
            String versionTag = null;
            if (rs.next()) {
                versionTag = rs.getString(1);
            }
            if (versionTag == null) {
                result.addFinding(new AuditFinding(
                        "SCHEMA_VERSION_UNKNOWN",
                        "No schema version tag found",
                        AuditSeverity.MEDIUM,
                        "mt6883_schema_version exists but contains no version_tag."
                ));
            } else if (!versionTag.startsWith("MT6883SCHEMA-2026")) {
                result.addFinding(new AuditFinding(
                        "SCHEMA_VERSION_OUTDATED",
                        "Outdated schema version detected: " + versionTag,
                        AuditSeverity.MEDIUM,
                        "Latest schema version_tag is " + versionTag + "; expected prefix MT6883SCHEMA-2026."
                ));
            } else {
                result.addFinding(new AuditFinding(
                        "SCHEMA_VERSION_OK",
                        "Schema version appears current: " + versionTag,
                        AuditSeverity.INFO,
                        "Schema version_tag " + versionTag + " matches expected 2026-series."
                ));
            }
        }
    }

    private static boolean tableExists(Connection conn, String tableName) throws SQLException {
        String sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?";
        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setString(1, tableName);
            try (ResultSet rs = ps.executeQuery()) {
                return rs.next();
            }
        }
    }

    // -------------------------------------------------------------------------
    // Command-line options parsing
    // -------------------------------------------------------------------------

    private static final class CommandLineOptions {
        private final String outputPath;
        private final List<String> shardPaths;
        private final boolean valid;

        private CommandLineOptions(String outputPath, List<String> shardPaths, boolean valid) {
            this.outputPath = outputPath;
            this.shardPaths = shardPaths;
            this.valid = valid;
        }

        public static CommandLineOptions parse(String[] args) {
            String out = null;
            List<String> shards = new ArrayList<>();

            for (int i = 0; i < args.length; i++) {
                String arg = args[i];
                if ("--out".equals(arg) && i + 1 < args.length) {
                    out = args[++i];
                } else if ("--rules".equals(arg) && i + 1 < args.length) {
                    // For now we only support MT6883RiskAuditShard2026v1,
                    // but we parse and ignore the value to keep the interface future-proof.
                    String ruleset = args[++i];
                    if (!RULESET_ID.equals(ruleset)) {
                        System.err.println("Warning: requested ruleset " + ruleset +
                                " does not match supported " + RULESET_ID + ". Using " + RULESET_ID + ".");
                    }
                } else if (arg.startsWith("--")) {
                    System.err.println("Unknown option: " + arg);
                } else {
                    shards.add(arg);
                }
            }

            boolean ok = out != null && !shards.isEmpty();
            return new CommandLineOptions(out, shards, ok);
        }

        public static void printUsage() {
            System.err.println("Usage:");
            System.err.println("  java -cp .:sqlite-jdbc.jar org.prometheuspraxis.audit.MT6883RiskAuditShardTool \\");
            System.err.println("      --rules MT6883RiskAuditShard2026v1 \\");
            System.err.println("      --out mt6883_audit_report.json \\");
            System.err.println("      shard1.db shard2.db ...");
        }

        public boolean isValid() {
            return valid;
        }

        public String getOutputPath() {
            return outputPath;
        }

        public List<String> getShardPaths() {
            return shardPaths;
        }
    }

    // -------------------------------------------------------------------------
    // Audit model types
    // -------------------------------------------------------------------------

    private enum AuditSeverity {
        INFO,
        WARNING,
        MEDIUM,
        HIGH,
        CRITICAL
    }

    private static final class AuditFinding {
        private final String code;
        private final String title;
        private final AuditSeverity severity;
        private final String detail;

        AuditFinding(String code, String title, AuditSeverity severity, String detail) {
            this.code = code;
            this.title = title;
            this.severity = severity;
            this.detail = detail;
        }

        public String getCode() {
            return code;
        }

        public String getTitle() {
            return title;
        }

        public AuditSeverity getSeverity() {
            return severity;
        }

        public String getDetail() {
            return detail;
        }

        public int severityWeight() {
            switch (severity) {
                case INFO:
                    return 0;
                case WARNING:
                    return 1;
                case MEDIUM:
                    return 2;
                case HIGH:
                    return 3;
                case CRITICAL:
                    return 5;
                default:
                    return 0;
            }
        }
    }

    private static final class ShardAuditResult {
        private final String shardName;
        private final String shardPath;
        private final List<AuditFinding> findings;
        private double riskScore;

        ShardAuditResult(String shardName, String shardPath) {
            this.shardName = shardName;
            this.shardPath = shardPath;
            this.findings = new ArrayList<>();
            this.riskScore = 0.0;
        }

        public void addFinding(AuditFinding finding) {
            findings.add(finding);
        }

        public void computeRiskScore() {
            int totalWeight = 0;
            for (AuditFinding f : findings) {
                totalWeight += f.severityWeight();
            }
            // Normalize: simple heuristic where 0 is safe, 1 is extreme
            // Assuming 50 as a rough upper bound across all rules.
            this.riskScore = Math.min(1.0, totalWeight / 50.0);
        }

        public String getShardName() {
            return shardName;
        }

        public String getShardPath() {
            return shardPath;
        }

        public List<AuditFinding> getFindings() {
            return findings;
        }

        public double getRiskScore() {
            return riskScore;
        }
    }

    private static final class AggregateAuditSummary {
        private final int shardCount;
        private final double maxRisk;
        private final double avgRisk;
        private final int criticalFindings;
        private final int highFindings;

        AggregateAuditSummary(int shardCount, double maxRisk, double avgRisk, int criticalFindings, int highFindings) {
            this.shardCount = shardCount;
            this.maxRisk = maxRisk;
            this.avgRisk = avgRisk;
            this.criticalFindings = criticalFindings;
            this.highFindings = highFindings;
        }

        public static AggregateAuditSummary fromShardResults(List<ShardAuditResult> results) {
            int shardCount = results.size();
            double maxRisk = 0.0;
            double sumRisk = 0.0;
            int critical = 0;
            int high = 0;

            for (ShardAuditResult r : results) {
                double rs = r.getRiskScore();
                sumRisk += rs;
                if (rs > maxRisk) {
                    maxRisk = rs;
                }
                for (AuditFinding f : r.getFindings()) {
                    if (f.getSeverity() == AuditSeverity.CRITICAL) {
                        critical++;
                    } else if (f.getSeverity() == AuditSeverity.HIGH) {
                        high++;
                    }
                }
            }

            double avgRisk = shardCount > 0 ? sumRisk / shardCount : 0.0;

            return new AggregateAuditSummary(shardCount, maxRisk, avgRisk, critical, high);
        }
    }

    // -------------------------------------------------------------------------
    // JSON report builder (no external JSON libraries)
    // -------------------------------------------------------------------------

    private static final class JsonReportBuilder {

        public static String buildReport(
                String rulesetId,
                String toolVersion,
                List<ShardAuditResult> shardResults,
                AggregateAuditSummary summary,
                double knowledgeFactor,
                double ecoImpactValue
        ) {
            StringBuilder sb = new StringBuilder();
            sb.append("{\n");

            appendField(sb, "ruleset_id", rulesetId, true);
            appendField(sb, "tool_version", toolVersion, true);
            appendField(sb, "generated_at_utc", Instant.now().toString(), true);
            appendDoubleField(sb, "knowledge_factor", knowledgeFactor, true);
            appendDoubleField(sb, "eco_impact_value", ecoImpactValue, true);

            sb.append("  \"summary\": {\n");
            appendIntField(sb, "shard_count", summary.shardCount, true, 2);
            appendDoubleField(sb, "max_risk_score", summary.maxRisk, true, 2);
            appendDoubleField(sb, "avg_risk_score", summary.avgRisk, true, 2);
            appendIntField(sb, "critical_findings", summary.criticalFindings, true, 2);
            appendIntField(sb, "high_findings", summary.highFindings, false, 2);
            sb.append("  },\n");

            sb.append("  \"shards\": [\n");
            for (int i = 0; i < shardResults.size(); i++) {
                ShardAuditResult r = shardResults.get(i);
                sb.append("    {\n");
                appendField(sb, "shard_name", r.getShardName(), true, 6);
                appendField(sb, "shard_path", r.getShardPath(), true, 6);
                appendDoubleField(sb, "risk_score", r.getRiskScore(), true, 6);

                sb.append("      \"findings\": [\n");
                List<AuditFinding> findings = r.getFindings();
                for (int j = 0; j < findings.size(); j++) {
                    AuditFinding f = findings.get(j);
                    sb.append("        {\n");
                    appendField(sb, "code", f.getCode(), true, 10);
                    appendField(sb, "title", f.getTitle(), true, 10);
                    appendField(sb, "severity", f.getSeverity().name(), true, 10);
                    appendField(sb, "detail", f.getDetail(), false, 10);
                    sb.append("        }");
                    if (j < findings.size() - 1) {
                        sb.append(",");
                    }
                    sb.append("\n");
                }
                sb.append("      ]\n");
                sb.append("    }");
                if (i < shardResults.size() - 1) {
                    sb.append(",");
                }
                sb.append("\n");
            }
            sb.append("  ]\n");
            sb.append("}\n");

            return sb.toString();
        }

        private static void appendField(StringBuilder sb, String name, String value, boolean comma) {
            appendField(sb, name, value, comma, 2);
        }

        private static void appendField(StringBuilder sb, String name, String value, boolean comma, int indent) {
            indent(sb, indent);
            sb.append("\"").append(escape(name)).append("\": ");
            sb.append("\"").append(escape(value)).append("\"");
            if (comma) {
                sb.append(",");
            }
            sb.append("\n");
        }

        private static void appendDoubleField(StringBuilder sb, String name, double value, boolean comma) {
            appendDoubleField(sb, name, value, comma, 2);
        }

        private static void appendDoubleField(StringBuilder sb, String name, double value, boolean comma, int indent) {
            indent(sb, indent);
            sb.append("\"").append(escape(name)).append("\": ");
            sb.append(String.format(java.util.Locale.ROOT, "%.4f", value));
            if (comma) {
                sb.append(",");
            }
            sb.append("\n");
        }

        private static void appendIntField(StringBuilder sb, String name, int value, boolean comma, int indent) {
            indent(sb, indent);
            sb.append("\"").append(escape(name)).append("\": ");
            sb.append(value);
            if (comma) {
                sb.append(",");
            }
            sb.append("\n");
        }

        private static void indent(StringBuilder sb, int spaces) {
            for (int i = 0; i < spaces; i++) {
                sb.append(' ');
            }
        }

        private static String escape(String s) {
            if (s == null) {
                return "";
            }
            return s.replace("\\", "\\\\").replace("\"", "\\\"");
        }
    }
}
