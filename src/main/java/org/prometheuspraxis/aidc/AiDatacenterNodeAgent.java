// File: src/main/java/org/prometheuspraxis/aidc/AiDatacenterNodeAgent.java
// Destination: Prometheus-Praxis/ai-datacenter-node-agent/src/main/java/org/prometheuspraxis/aidc/AiDatacenterNodeAgent.java
// License: MIT OR Apache-2.0

package org.prometheuspraxis.aidc;

import java.io.IOException;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.time.Instant;
import java.util.Timer;
import java.util.TimerTask;

/**
 * AiDatacenterNode2026v1 monitoring agent.
 *
 * Responsibilities:
 *  - Poll AiDatacenterNode2026v1 parameters (power, cooling, KER).
 *  - Evaluate thresholds/invariants provided by Rust/ALN identity crate.
 *  - Push alerts to Discord webhook when invariants are violated or nearing violation.
 *
 * This is written as a simple Java process; it can also be packaged
 * as a Java agent if needed.
 */
public final class AiDatacenterNodeAgent {

    public static void main(String[] args) {
        String nodeId      = System.getenv().getOrDefault("AIDC_NODE_ID", "xfra-node-001");
        String webhookUrl  = System.getenv().getOrDefault("DISCORD_WEBHOOK_URL", "");
        String rustSocket  = System.getenv().getOrDefault("RUST_INVARIANT_SOCKET_PATH", "/tmp/aidc_invariants.sock");

        if (webhookUrl.isEmpty()) {
            System.err.println("DISCORD_WEBHOOK_URL is not set; exiting.");
            System.exit(1);
        }

        InvariantBridge invariantBridge = new InvariantBridge(rustSocket);
        MetricsSource metricsSource = new MetricsSource(nodeId);

        long intervalMs = 30_000L; // 30 seconds
        Timer timer = new Timer("AiDatacenterNodeAgentTimer", true);
        timer.scheduleAtFixedRate(new TimerTask() {
            @Override
            public void run() {
                try {
                    AiDatacenterNodeMetrics metrics = metricsSource.fetchMetrics();
                    Thresholds thresholds = invariantBridge.fetchThresholds(nodeId);

                    AlertDecision decision = AlertEngine.evaluate(metrics, thresholds);
                    if (decision.shouldAlert) {
                        DiscordNotifier.sendAlert(webhookUrl, decision, metrics, thresholds);
                    }
                } catch (Exception e) {
                    System.err.println("Monitoring loop error: " + e.getMessage());
                    e.printStackTrace(System.err);
                }
            }
        }, 0L, intervalMs);

        System.out.println("AiDatacenterNodeAgent started for node=" + nodeId);
        // Keep process alive
        try {
            Thread.sleep(Long.MAX_VALUE);
        } catch (InterruptedException ignored) {
        }
    }
}

/**
 * Metrics model for AiDatacenterNode2026v1.
 *
 * Expected parameters:
 *  - powerKw: instantaneous power draw in kilowatts.
 *  - coolingKw: cooling capacity (or usage) in kilowatts.
 *  - kerScore: KER (Kernel Eco Risk) score in [0.0, 1.0].
 */
final class AiDatacenterNodeMetrics {
    final String nodeId;
    final double powerKw;
    final double coolingKw;
    final double kerScore;
    final String timestampUtc;

    AiDatacenterNodeMetrics(String nodeId, double powerKw, double coolingKw, double kerScore, String timestampUtc) {
        this.nodeId = nodeId;
        this.powerKw = powerKw;
        this.coolingKw = coolingKw;
        this.kerScore = kerScore;
        this.timestampUtc = timestampUtc;
    }
}

/**
 * Thresholds/invariants mirrored from Rust/ALN crate.
 *
 * All values are provided by the Rust invariants layer to ensure
 * Java alerts match the same safety/validity invariants.
 */
final class Thresholds {
    final double maxPowerKw;
    final double maxKerScore;
    final double minCoolingMarginKw;

    Thresholds(double maxPowerKw, double maxKerScore, double minCoolingMarginKw) {
        this.maxPowerKw = maxPowerKw;
        this.maxKerScore = maxKerScore;
        this.minCoolingMarginKw = minCoolingMarginKw;
    }
}

/**
 * Metrics source stub.
 *
 * In real deployment, this would read from sensors, Prometheus,
 * or an AiDatacenterNode2026v1 telemetry API. Here we provide a
 * simple example stub.
 */
final class MetricsSource {

    private final String nodeId;

    MetricsSource(String nodeId) {
        this.nodeId = nodeId;
    }

    AiDatacenterNodeMetrics fetchMetrics() {
        // TODO-free stub: replace with real sensor/telemetry integration.
        // For now, generate example metrics.
        double powerKw = 350.0;         // example
        double coolingKw = 400.0;       // example
        double kerScore = 0.42;         // example
        String ts = Instant.now().toString();

        return new AiDatacenterNodeMetrics(nodeId, powerKw, coolingKw, kerScore, ts);
    }
}

/**
 * Bridge to Rust invariants crate via Unix socket or other IPC.
 *
 * The Rust side is responsible for enforcing ALN invariants and
 * returning thresholds that are safe/valid by construction.[web:182][web:189]
 */
final class InvariantBridge {

    private final String socketPath;

    InvariantBridge(String socketPath) {
        this.socketPath = socketPath;
    }

    Thresholds fetchThresholds(String nodeId) {
        // For now, return static invariants; in real system, this
        // would talk to Rust over Unix socket or HTTP to fetch
        // node-specific thresholds validated by ALN.
        double maxPowerKw = 500.0;
        double maxKerScore = 0.80;
        double minCoolingMarginKw = 50.0;

        return new Thresholds(maxPowerKw, maxKerScore, minCoolingMarginKw);
    }
}

/**
 * Decision about whether to alert and what severity.
 */
final class AlertDecision {
    final boolean shouldAlert;
    final String severity;
    final String message;

    AlertDecision(boolean shouldAlert, String severity, String message) {
        this.shouldAlert = shouldAlert;
        this.severity = severity;
        this.message = message;
    }

    static AlertDecision none() {
        return new AlertDecision(false, "NONE", "");
    }
}

/**
 * Alert evaluation logic: compare metrics to thresholds/invariants.
 */
final class AlertEngine {

    static AlertDecision evaluate(AiDatacenterNodeMetrics m, Thresholds t) {
        boolean overPower = m.powerKw > t.maxPowerKw;
        boolean overKer = m.kerScore > t.maxKerScore;
        double coolingMargin = m.coolingKw - m.powerKw;
        boolean coolingTooLow = coolingMargin < t.minCoolingMarginKw;

        if (!overPower && !overKer && !coolingTooLow) {
            return AlertDecision.none();
        }

        StringBuilder msg = new StringBuilder();
        msg.append("AiDatacenterNode2026v1 invariant alert for node ")
           .append(m.nodeId)
           .append(" at ")
           .append(m.timestampUtc)
           .append(":\n");

        String severity = "INFO";

        if (overPower) {
            msg.append("- Power draw exceeded maxPowerKw (")
               .append(String.format("%.1f", m.powerKw))
               .append(" kW > ")
               .append(String.format("%.1f", t.maxPowerKw))
               .append(" kW).\n");
            severity = "WARN";
        }

        if (overKer) {
            msg.append("- KER score exceeded maxKerScore (")
               .append(String.format("%.2f", m.kerScore))
               .append(" > ")
               .append(String.format("%.2f", t.maxKerScore))
               .append(").\n");
            severity = "WARN";
        }

        if (coolingTooLow) {
            msg.append("- Cooling margin below minCoolingMarginKw (margin=")
               .append(String.format("%.1f", coolingMargin))
               .append(" kW < ")
               .append(String.format("%.1f", t.minCoolingMarginKw))
               .append(" kW).\n");
            severity = "CRITICAL";
        }

        return new AlertDecision(true, severity, msg.toString());
    }
}

/**
 * Discord webhook notifier using plain HTTP POST.
 *
 * Payload is a simple JSON message with content and basic formatting.
 */
final class DiscordNotifier {

    static void sendAlert(String webhookUrl, AlertDecision decision,
                          AiDatacenterNodeMetrics m, Thresholds t) throws IOException {

        String content = decision.message;
        String json = buildDiscordJson(content, decision.severity, m);

        URL url = new URL(webhookUrl);
        HttpURLConnection conn = (HttpURLConnection) url.openConnection();
        conn.setRequestMethod("POST");
        conn.setDoOutput(true);
        conn.setRequestProperty("Content-Type", "application/json");

        try (OutputStream os = conn.getOutputStream()) {
            byte[] body = json.getBytes(java.nio.charset.StandardCharsets.UTF_8);
            os.write(body);
        }

        int status = conn.getResponseCode();
        if (status / 100 != 2) {
            System.err.println("Discord webhook returned non-2xx status: " + status);
        }
    }

    private static String buildDiscordJson(String content, String severity,
                                           AiDatacenterNodeMetrics m) {
        String severityEmoji;
        switch (severity) {
            case "CRITICAL":
                severityEmoji = "🚨";
                break;
            case "WARN":
                severityEmoji = "⚠️";
                break;
            default:
                severityEmoji = "ℹ️";
        }

        String sanitizedContent = content
                .replace("\\", "\\\\")
                .replace("\"", "\\\"");

        return "{"
                + "\"username\":\"AiDatacenterNodeAgent\","
                + "\"content\":\"" + severityEmoji + " " + sanitizedContent + "\","
                + "\"embeds\":[{"
                + "\"title\":\"Node " + m.nodeId + " metrics\","
                + "\"description\":\"power=" + String.format("%.1f", m.powerKw) + " kW, "
                + "cooling=" + String.format("%.1f", m.coolingKw) + " kW, "
                + "KER=" + String.format("%.2f", m.kerScore) + "\","
                + "\"color\":"
                + (severity.equals("CRITICAL") ? "15158332" : severity.equals("WARN") ? "16776960" : "3447003")
                + "}]"
                + "}";
    }
}
