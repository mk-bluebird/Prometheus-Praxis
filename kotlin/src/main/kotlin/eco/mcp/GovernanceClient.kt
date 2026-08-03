// File: kotlin/src/main/kotlin/eco/mcp/GovernanceClient.kt
package eco.mcp

import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStreamWriter

// Data classes mirroring C++ MCP JSON responses.
// These are Kotlin-side shards wired to the C++ universal toolbox / stdio bridge.

data class HexStabilityCarbonRow(
    val hexId: String,
    val vResidual: Double,
    val carbonIntensity: Double,
    val kerS: Double
)

data class HexAttentionRow(
    val hexId: String,
    val carbonBand: String,
    val kerS: Double,
    val deltaVt: Double
)

data class HexAttentionResponse(
    val hexesNeedingAttention: List<HexAttentionRow>
)

data class HexStabilityCarbonSnapshot(
    val hexStabilityCarbon: List<HexStabilityCarbonRow>
)

class GovernanceClient(
    private val cppBinaryPath: String // e.g. "./universal_mcp_governance_toolbox" or "./mcp_stdio_governance_bridge"
) {

    /**
     * Invoke the C++ MCP toolbox over stdio with a single-line command.
     */
    private fun invokeCpp(command: String): String {
        val proc = ProcessBuilder(cppBinaryPath)
            .redirectErrorStream(true)
            .start()

        val writer = OutputStreamWriter(proc.outputStream)
        writer.write(command)
        writer.write("\n")
        writer.flush()
        writer.close()

        val reader = BufferedReader(InputStreamReader(proc.inputStream))
        val sb = StringBuilder()
        var line: String? = reader.readLine()
        while (line != null) {
            sb.append(line).append('\n')
            line = reader.readLine()
        }
        reader.close()
        proc.waitFor()
        return sb.toString()
    }

    /**
     * Fetch hexes needing attention from the C++ governance toolbox.
     * Uses the "hexes_needing_attention" command exposed by mcp_stdio_governance_bridge.cpp.
     */
    fun hexesNeedingAttention(): HexAttentionResponse {
        val json = invokeCpp("hexes_needing_attention")
        // Minimal JSON parsing without external libraries; assumes the canned structure.
        val rows = mutableListOf<HexAttentionRow>()
        val lines = json.lines()
        for (line in lines) {
            if (line.contains("\"hexId\"")) {
                val hexId = extractString(line, "hexId")
                val carbonBand = extractString(line, "carbonBand")
                val kerS = extractDouble(line, "kerS")
                val deltaVt = extractDouble(line, "deltaVt")
                rows.add(HexAttentionRow(hexId, carbonBand, kerS, deltaVt))
            }
        }
        return HexAttentionResponse(rows)
    }

    /**
     * Fetch a hex stability/carbon snapshot from the C++ toolbox.
     * Uses the "hex_stability_carbon_snapshot" command.
     */
    fun hexStabilityCarbonSnapshot(): HexStabilityCarbonSnapshot {
        val json = invokeCpp("hex_stability_carbon_snapshot")
        val rows = mutableListOf<HexStabilityCarbonRow>()
        val lines = json.lines()
        for (line in lines) {
            if (line.contains("\"hexId\"")) {
                val hexId = extractString(line, "hexId")
                val vResidual = extractDouble(line, "vResidual")
                val carbonIntensity = extractDouble(line, "carbonIntensity")
                val kerS = extractDouble(line, "kerS")
                rows.add(HexStabilityCarbonRow(hexId, vResidual, carbonIntensity, kerS))
            }
        }
        return HexStabilityCarbonSnapshot(rows)
    }

    // Simple helper to pull a string field from a JSON-like line.
    private fun extractString(line: String, field: String): String {
        val key = "\"$field\""
        val idx = line.indexOf(key)
        if (idx == -1) return ""
        val colon = line.indexOf(':', idx)
        val firstQuote = line.indexOf('"', colon + 1)
        val secondQuote = line.indexOf('"', firstQuote + 1)
        return if (firstQuote != -1 && secondQuote != -1) {
            line.substring(firstQuote + 1, secondQuote)
        } else ""
    }

    // Simple helper to pull a double field from a JSON-like line.
    private fun extractDouble(line: String, field: String): Double {
        val key = "\"$field\""
        val idx = line.indexOf(key)
        if (idx == -1) return 0.0
        val colon = line.indexOf(':', idx)
        if (colon == -1) return 0.0
        val end = line.indexOf(',', colon + 1).let { if (it == -1) line.length else it }
        val raw = line.substring(colon + 1, end).trim().trimEnd('}', ' ')
        return raw.toDoubleOrNull() ?: 0.0
    }
}

// Example usage entry point; this can be used by AI-chat integration.
fun main() {
    val client = GovernanceClient("./mcp_stdio_governance_bridge")

    val attention = client.hexesNeedingAttention()
    println("Hexes needing attention:")
    attention.hexesNeedingAttention.forEach {
        println("  ${it.hexId} band=${it.carbonBand} s=${it.kerS} ΔVt=${it.deltaVt}")
    }

    val snapshot = client.hexStabilityCarbonSnapshot()
    println("\nHex stability/carbon snapshot:")
    snapshot.hexStabilityCarbon.forEach {
        println("  ${it.hexId} Vres=${it.vResidual} CI=${it.carbonIntensity} s=${it.kerS}")
    }
}
