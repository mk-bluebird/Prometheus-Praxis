// File: corridor-econet-blastradius/src/main/java/org/prometheuspraxis/corridor/BlastRadiusSimulation.java
// Destination: mk-bluebird/Prometheus-Praxis/corridor-econet-blastradius/src/main/java/org/prometheuspraxis/corridor/BlastRadiusSimulation.java
// License: MIT OR Apache-2.0

package org.prometheuspraxis.corridor;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class BlastRadiusSimulation {

    public static void main(String[] args) throws Exception {
        String kerWindowPath = args.length > 0 ? args[0] : "data/corridor_ker_windows.json";
        String alnLimitsPath = args.length > 1 ? args[1] : "aln/corridor_econet_blastradius_2026v1.aln";
        String outputDirPath = args.length > 2 ? args[2] : "docs/corridor-econet-blastradius";

        List<KerWindowSample> samples = KerWindowLoader.load(kerWindowPath);
        BlastRadiusLimits limits = AlnBlastRadiusLimitsLoader.load(alnLimitsPath);

        SimulationResult result = BlastRadiusEngine.runSimulation(samples, limits);

        File outputDir = new File(outputDirPath);
        if (!outputDir.exists() && !outputDir.mkdirs()) {
            throw new IOException("Failed to create output directory: " + outputDirPath);
        }

        ComplianceReportWriter.writeHtmlReport(result, new File(outputDir, "index.html"));
    }
}

final class KerWindowSample {
    final String corridorId;
    final Instant timestamp;
    final double kerScore;
    final double residualKerScore;

    KerWindowSample(String corridorId, Instant timestamp, double kerScore, double residualKerScore) {
        this.corridorId = corridorId;
        this.timestamp = timestamp;
        this.kerScore = kerScore;
        this.residualKerScore = residualKerScore;
    }
}

final class BlastRadiusLimits {
    final double maxResidualKer;
    final double maxBlastRadiusKm;

    BlastRadiusLimits(double maxResidualKer, double maxBlastRadiusKm) {
        this.maxResidualKer = maxResidualKer;
        this.maxBlastRadiusKm = maxBlastRadiusKm;
    }
}

final class KerWindowLoader {

    static List<KerWindowSample> load(String path) throws IOException {
        List<KerWindowSample> samples = new ArrayList<>();

        try (BufferedReader reader = new BufferedReader(new FileReader(path, StandardCharsets.UTF_8))) {
            StringBuilder sb = new StringBuilder();
            String line = reader.readLine();
            while (line != null) {
                sb.append(line).append("\n");
                line = reader.readLine();
            }
            String content = sb.toString();

            Pattern p = Pattern.compile(
                "\\{\\s*\"corridor_id\"\\s*:\\s*\"(.*?)\"\\s*,\\s*" +
                "\"timestamp_utc\"\\s*:\\s*\"(.*?)\"\\s*,\\s*" +
                "\"ker_score\"\\s*:\\s*(.*?)\\s*,\\s*" +
                "\"residual_ker_score\"\\s*:\\s*(.*?)\\s*\\}",
                Pattern.DOTALL
            );
            Matcher m = p.matcher(content);
            while (m.find()) {
                String corridorId = m.group(1);
                String timestampStr = m.group(2);
                double kerScore = Double.parseDouble(m.group(3));
                double residualKerScore = Double.parseDouble(m.group(4));
                Instant ts = Instant.parse(timestampStr);
                samples.add(new KerWindowSample(corridorId, ts, kerScore, residualKerScore));
            }
        }

        return samples;
    }
}

final class AlnBlastRadiusLimitsLoader {

    static BlastRadiusLimits load(String path) throws IOException {
        double maxResidualKer = 1.0;
        double maxBlastRadiusKm = 0.0;

        try (BufferedReader reader = new BufferedReader(new FileReader(path, StandardCharsets.UTF_8))) {
            StringBuilder sb = new StringBuilder();
            String line = reader.readLine();
            while (line != null) {
                sb.append(line).append("\n");
                line = reader.readLine();
            }
            String content = sb.toString();

            Matcher mKer = Pattern.compile(
                "MAX_RESIDUAL_KER\\s+(\\d+\\.\\d+|\\d+)"
            ).matcher(content);
            if (mKer.find()) {
                maxResidualKer = Double.parseDouble(mKer.group(1));
            }

            Matcher mRadius = Pattern.compile(
                "MAX_BLAST_RADIUS_KM\\s+(\\d+\\.\\d+|\\d+)"
            ).matcher(content);
            if (mRadius.find()) {
                maxBlastRadiusKm = Double.parseDouble(mRadius.group(1));
            }
        }

        return new BlastRadiusLimits(maxResidualKer, maxBlastRadiusKm);
    }
}

final class SimulationResult {
    final BlastRadiusLimits limits;
    final List<KerWindowSample> samples;
    final boolean compliant;
    final int violationsCount;
    final double worstResidualKer;
    final KerWindowSample worstSample;

    SimulationResult(BlastRadiusLimits limits,
                     List<KerWindowSample> samples,
                     boolean compliant,
                     int violationsCount,
                     double worstResidualKer,
                     KerWindowSample worstSample) {
        this.limits = limits;
        this.samples = samples;
        this.compliant = compliant;
        this.violationsCount = violationsCount;
        this.worstResidualKer = worstResidualKer;
        this.worstSample = worstSample;
    }
}

final class BlastRadiusEngine {

    static SimulationResult runSimulation(List<KerWindowSample> samples, BlastRadiusLimits limits) {
        boolean compliant = true;
        int violations = 0;
        double worstResidual = 0.0;
        KerWindowSample worstSample = null;

        for (KerWindowSample s : samples) {
            if (s.residualKerScore > limits.maxResidualKer) {
                compliant = false;
                violations++;
                if (s.residualKerScore > worstResidual || worstSample == null) {
                    worstResidual = s.residualKerScore;
                    worstSample = s;
                }
            }
        }

        return new SimulationResult(limits, samples, compliant, violations, worstResidual, worstSample);
    }
}

final class ComplianceReportWriter {

    static void writeHtmlReport(SimulationResult result, File outputFile) throws IOException {
        try (BufferedWriter writer = new BufferedWriter(new FileWriter(outputFile, StandardCharsets.UTF_8))) {
            writer.write("<!DOCTYPE html>\n");
            writer.write("<html lang=\"en\">\n");
            writer.write("<head>\n");
            writer.write("  <meta charset=\"UTF-8\" />\n");
            writer.write("  <title>Corridor EcoNet Blast Radius Compliance – 2026v1</title>\n");
            writer.write("  <style>\n");
            writer.write("    body { font-family: system-ui, -apple-system, sans-serif; margin: 2rem; }\n");
            writer.write("    h1, h2 { color: #1f2937; }\n");
            writer.write("    .summary { margin-bottom: 2rem; padding: 1rem; border-radius: 0.5rem; }\n");
            writer.write("    .summary.ok { background: #ecfdf5; border: 1px solid #6ee7b7; }\n");
            writer.write("    .summary.fail { background: #fef2f2; border: 1px solid #fca5a5; }\n");
            writer.write("    table { border-collapse: collapse; width: 100%; }\n");
            writer.write("    th, td { border: 1px solid #e5e7eb; padding: 0.5rem; text-align: left; font-size: 0.875rem; }\n");
            writer.write("    th { background: #f9fafb; }\n");
            writer.write("  </style>\n");
            writer.write("</head>\n");
            writer.write("<body>\n");
            writer.write("<h1>Corridor EcoNet Blast Radius Compliance – 2026v1</h1>\n");

            writer.write("<div class=\"summary " + (result.compliant ? "ok" : "fail") + "\">\n");
            writer.write("  <h2>Summary</h2>\n");
            writer.write("  <p><strong>Max allowed residual KER:</strong> " + formatDouble(result.limits.maxResidualKer) + "</p>\n");
            writer.write("  <p><strong>Max allowed blast radius (km):</strong> " + formatDouble(result.limits.maxBlastRadiusKm) + "</p>\n");
            writer.write("  <p><strong>Samples evaluated:</strong> " + result.samples.size() + "</p>\n");
            writer.write("  <p><strong>Violations:</strong> " + result.violationsCount + "</p>\n");
            if (result.worstSample != null) {
                writer.write("  <p><strong>Worst residual KER:</strong> " + formatDouble(result.worstResidualKer) +
                             " (corridor " + escapeHtml(result.worstSample.corridorId) +
                             ", timestamp " + result.worstSample.timestamp.toString() + ")</p>\n");
            }
            writer.write("  <p><strong>Status:</strong> " +
                         (result.compliant ? "COMPLIANT" : "NON-COMPLIANT") + "</p>\n");
            writer.write("</div>\n");

            writer.write("<h2>KER Window Samples</h2>\n");
            writer.write("<table>\n");
            writer.write("  <thead>\n");
            writer.write("    <tr><th>Corridor</th><th>Timestamp (UTC)</th><th>KER</th><th>Residual KER</th><th>Within Limit?</th></tr>\n");
            writer.write("  </thead>\n");
            writer.write("  <tbody>\n");
            for (KerWindowSample s : result.samples) {
                boolean ok = s.residualKerScore <= result.limits.maxResidualKer;
                writer.write("    <tr>\n");
                writer.write("      <td>" + escapeHtml(s.corridorId) + "</td>\n");
                writer.write("      <td>" + escapeHtml(s.timestamp.toString()) + "</td>\n");
                writer.write("      <td>" + formatDouble(s.kerScore) + "</td>\n");
                writer.write("      <td>" + formatDouble(s.residualKerScore) + "</td>\n");
                writer.write("      <td>" + (ok ? "YES" : "NO") + "</td>\n");
                writer.write("    </tr>\n");
            }
            writer.write("  </tbody>\n");
            writer.write("</table>\n");

            writer.write("</body>\n");
            writer.write("</html>\n");
        }
    }

    private static String formatDouble(double d) {
        return String.format(java.util.Locale.US, "%.3f", d);
    }

    private static String escapeHtml(String s) {
        return s.replace("&", "&amp;")
                .replace("<", "&lt;")
                .replace(">", "&gt;")
                .replace("\"", "&quot;");
    }
}
