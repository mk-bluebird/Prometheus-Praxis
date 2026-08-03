// File: java/src/main/java/eco/mobile/MobileHexMonitoringApp.java
package eco.mobile;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;

/**
 * Lightweight Java CLI app for mobile hex monitoring.
 * Connects to the C++ MCP stdio bridge (`mcp_stdio_governance_bridge`),
 * requests hex stability metrics, and prints them for field operators.
 *
 * This can be wrapped by a Swing UI if needed, but remains CLI-friendly.
 */
public class MobileHexMonitoringApp {

    private static String invokeCpp(String cppBinaryPath, String command) throws IOException, InterruptedException {
        Process proc = new ProcessBuilder(cppBinaryPath)
                .redirectErrorStream(true)
                .start();

        try (OutputStreamWriter writer = new OutputStreamWriter(proc.getOutputStream())) {
            writer.write(command);
            writer.write("\n");
            writer.flush();
        }

        StringBuilder sb = new StringBuilder();
        try (BufferedReader reader = new BufferedReader(
                new InputStreamReader(proc.getInputStream()))) {
            String line;
            while ((line = reader.readLine()) != null) {
                sb.append(line).append('\n');
            }
        }
        proc.waitFor();
        return sb.toString();
    }

    private static void showHexStabilitySnapshot(String cppBinaryPath) throws IOException, InterruptedException {
        String json = invokeCpp(cppBinaryPath, "hex_stability_carbon_snapshot");
        System.out.println("Hex Stability / Carbon Snapshot:");
        for (String line : json.split("\n")) {
            if (line.contains("\"hexId\"")) {
                String hexId = extractString(line, "hexId");
                double vResidual = extractDouble(line, "vResidual");
                double carbonIntensity = extractDouble(line, "carbonIntensity");
                double kerS = extractDouble(line, "kerS");

                System.out.printf("  %-12s V_res=%.3f CI=%.3f s=%.3f%n",
                        hexId, vResidual, carbonIntensity, kerS);
            }
        }
    }

    private static void showHexesNeedingAttention(String cppBinaryPath) throws IOException, InterruptedException {
        String json = invokeCpp(cppBinaryPath, "hexes_needing_attention");
        System.out.println("Hexes Needing Attention:");
        for (String line : json.split("\n")) {
            if (line.contains("\"hexId\"")) {
                String hexId = extractString(line, "hexId");
                String band = extractString(line, "carbonBand");
                double kerS = extractDouble(line, "kerS");
                double deltaVt = extractDouble(line, "deltaVt");

                System.out.printf("  %-12s band=%-10s s=%.3f ΔV_t=%.3f%n",
                        hexId, band, kerS, deltaVt);
            }
        }
    }

    private static String extractString(String line, String field) {
        String key = "\"" + field + "\"";
        int idx = line.indexOf(key);
        if (idx == -1) return "";
        int colon = line.indexOf(':', idx);
        int firstQuote = line.indexOf('"', colon + 1);
        int secondQuote = line.indexOf('"', firstQuote + 1);
        if (firstQuote == -1 || secondQuote == -1) return "";
        return line.substring(firstQuote + 1, secondQuote);
    }

    private static double extractDouble(String line, String field) {
        String key = "\"" + field + "\"";
        int idx = line.indexOf(key);
        if (idx == -1) return 0.0;
        int colon = line.indexOf(':', idx);
        if (colon == -1) return 0.0;
        int end = line.indexOf(',', colon + 1);
        if (end == -1) end = line.length();
        String raw = line.substring(colon + 1, end).trim().replace("}", "").trim();
        try {
            return Double.parseDouble(raw);
        } catch (NumberFormatException e) {
            return 0.0;
        }
    }

    public static void main(String[] args) throws Exception {
        String cppBinaryPath = "./mcp_stdio_governance_bridge";
        if (args.length > 0) {
            cppBinaryPath = args[0];
        }

        BufferedReader in = new BufferedReader(new InputStreamReader(System.in));
        System.out.println("Mobile Hex Monitoring App");
        System.out.println("C++ MCP bridge: " + cppBinaryPath);
        System.out.println("Commands: snapshot, attention, quit");

        while (true) {
            System.out.print("> ");
            String cmd = in.readLine();
            if (cmd == null || cmd.equalsIgnoreCase("quit")) {
                break;
            } else if (cmd.equalsIgnoreCase("snapshot")) {
                showHexStabilitySnapshot(cppBinaryPath);
            } else if (cmd.equalsIgnoreCase("attention")) {
                showHexesNeedingAttention(cppBinaryPath);
            } else {
                System.out.println("Unknown command. Use: snapshot, attention, quit");
            }
        }
    }
}
