// File: java/eco/FogRouteInvoker.java
package eco;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;

/**
 * FogRouteInvoker
 *
 * Demonstrates Java–Lua integration for eco routing:
 * - Launches the Lua FOG router CLI script on a telemetry CSV.
 * - Reads node_code,FOG-band lines from Lua and prints them for Java-side tools.
 *
 * Usage:
 *   java eco.FogRouteInvoker <lua-exe> <fog_cli.lua> <telemetry.csv>
 *
 * Example:
 *   java eco.FogRouteInvoker lua lua/cyboquatic/fog_cli.lua data/telemetry.csv
 */
public class FogRouteInvoker {

    public static void main(String[] args) {
        if (args.length < 3) {
            System.out.println("Usage: FogRouteInvoker <lua-exe> <fog_cli.lua> <telemetry.csv>");
            return;
        }

        String luaExe = args[0];
        String luaScript = args[1];
        String csvPath = args[2];

        ProcessBuilder pb = new ProcessBuilder(luaExe, luaScript, csvPath);
        pb.redirectErrorStream(true);

        try {
            Process proc = pb.start();
            try (BufferedReader reader = new BufferedReader(
                    new InputStreamReader(proc.getInputStream()))) {
                System.out.println("=== Java–Lua FOG Routing ===");
                String line;
                while ((line = reader.readLine()) != null) {
                    // Each line is "node_code,FOG:BAND"
                    System.out.println(line);
                }
            }
            int exitCode = proc.waitFor();
            if (exitCode != 0) {
                System.err.println("Lua FOG CLI exited with code " + exitCode);
            }
        } catch (IOException | InterruptedException e) {
            e.printStackTrace();
        }
    }
}
