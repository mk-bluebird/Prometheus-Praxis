package mcp

import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStreamWriter
import java.nio.charset.StandardCharsets
import kotlin.system.exitProcess

/*
 * MCP KER + Synapse Governance Client (Kotlin)
 *
 * Wraps mcp_ker_synapse_server_named as a subprocess, sends JSON requests,
 * parses JSON responses, and exposes typed rows for governance reasoning.
 *
 * Non-actuating, read-only; intended for dashboards, planners, and AI-chat integration.
 */

data class HexStabilityCarbonRow(
    val hexId: String,
    val regionName: String,
    val topologyBand: String,
    val primaryPlane: String,
    val workloadCount: Int,
    val totalDeltaVt: Double,
    val avgDeltaVt: Double,
    val maxDeltaVt: Double,
    val avgKerK: Double,
    val avgKerE: Double,
    val avgKerR: Double,
    val avgKerS: Double,
    val avgCarbonIntensityGco2Kwh: Double,
    val countGreenBand: Int,
    val countNeutralBand: Int,
    val countRedBand: Int,
    val violationsDvtGlobal: Int,
    val violationsKerNonpositive: Int,
    val violationsJointKerDvt: Int,
    val violationsRedBandKer: Int,
    val violationsProdRedBand: Int
)

data class KerOverviewRow(
    val kind: String,          // "MODULE" or "TOOL"
    val band: String,          // repo_name or roleband
    val name: String,          // relpath or toolname
    val laneDefault: String,
    val primaryPlane: String,
    val role: String,          // module_role or synapse_class
    val kerK: Double,
    val kerE: Double,
    val kerR: Double,
    val kerS: Double,
    val neuroFlag: Boolean,
    val nonActuating: Boolean,
    val citizenReady: Boolean
)

data class SynapseSafeRow(
    val synapseId: Int,
    val producerLang: String,
    val producerRelpath: String,
    val consumerLang: String,
    val consumerRelpath: String,
    val synapseClass: String,
    val transportKind: String,
    val laneDefault: String,
    val primaryPlane: String,
    val nonActuating: Boolean,
    val allowsReadonly: Boolean,
    val allowsActuation: Boolean,
    val neuroFlag: Boolean,
    val kerK: Double,
    val kerE: Double,
    val kerR: Double,
    val kerS: Double
)

class McpKerSynapseClient(
    private val serverPath: String,
    private val sqliteDbPath: String
) {
    private val process: Process
    private val writer: OutputStreamWriter
    private val reader: BufferedReader

    init {
        val pb = ProcessBuilder(serverPath, sqliteDbPath)
        pb.redirectErrorStream(true)
        process = pb.start()
        writer = OutputStreamWriter(process.outputStream, StandardCharsets.UTF_8)
        reader = BufferedReader(InputStreamReader(process.inputStream, StandardCharsets.UTF_8))
    }

    fun close() {
        try {
            writer.close()
            reader.close()
            process.destroy()
        } catch (_: Exception) {
        }
    }

    private fun sendRequestAndReadResponse(requestJson: String): String {
        writer.write(requestJson)
        writer.write("\n")
        writer.flush()
        return reader.readLine() ?: """{ "ok": false, "error": "no response" }"""
    }

    // Minimal JSON parsing helpers (for sketch; replace with a robust JSON library in production).

    private fun extractRowsJson(response: String): List<String> {
        // Very crude: find "rows": [ ... ] and split naive curly blocks.
        val rowsStart = response.indexOf("\"rows\": [")
        if (rowsStart < 0) return emptyList()
        val arrayStart = response.indexOf('[', rowsStart)
        val arrayEnd = response.lastIndexOf(']')
        if (arrayStart < 0 || arrayEnd < arrayStart) return emptyList()
        val arrayContent = response.substring(arrayStart + 1, arrayEnd).trim()
        if (arrayContent.isEmpty()) return emptyList()
        val rows = mutableListOf<String>()
        var depth = 0
        var start = -1
        for ((i, c) in arrayContent.withIndex()) {
            if (c == '{') {
                if (depth == 0) start = i
                depth++
            } else if (c == '}') {
                depth--
                if (depth == 0 && start >= 0) {
                    rows.add(arrayContent.substring(start, i + 1))
                    start = -1
                }
            }
        }
        return rows
    }

    private fun getField(objJson: String, field: String): String? {
        val key = "\"$field\""
        val idx = objJson.indexOf(key)
        if (idx < 0) return null
        val colon = objJson.indexOf(':', idx + key.length)
        if (colon < 0) return null
        var valStr = objJson.substring(colon + 1).trim()
        // Trim trailing comma
        if (valStr.endsWith(",")) {
            valStr = valStr.substring(0, valStr.length - 1).trim()
        }
        // If quoted string
        if (valStr.startsWith("\"")) {
            valStr = valStr.substring(1)
            val endQuote = valStr.indexOf('"')
            if (endQuote >= 0) {
                return valStr.substring(0, endQuote)
            }
        } else {
            // numeric or boolean; stop at comma or end brace
            val end = valStr.indexOfAny(charArrayOf(',', '}'))
            val cleaned = if (end >= 0) valStr.substring(0, end) else valStr
            return cleaned.trim()
        }
        return null
    }

    // Parsers for specific tools

    fun hexStabilityCarbon(limit: Int = 50, primaryPlane: String = "HYDRAULICS"): List<HexStabilityCarbonRow> {
        val requestJson = """
            { "tool": "hex_stability_carbon", "params": { "limit": $limit, "primary_plane": "$primaryPlane" } }
        """.trimIndent()
        val response = sendRequestAndReadResponse(requestJson)
        val rowsJson = extractRowsJson(response)
        val result = mutableListOf<HexStabilityCarbonRow>()
        for (obj in rowsJson) {
            val row = HexStabilityCarbonRow(
                hexId = getField(obj, "hex_id") ?: "",
                regionName = getField(obj, "region_name") ?: "",
                topologyBand = getField(obj, "topology_band") ?: "",
                primaryPlane = getField(obj, "primary_plane") ?: "",
                workloadCount = (getField(obj, "workload_count") ?: "0").toInt(),
                totalDeltaVt = (getField(obj, "total_delta_v_t") ?: "0.0").toDouble(),
                avgDeltaVt = (getField(obj, "avg_delta_v_t") ?: "0.0").toDouble(),
                maxDeltaVt = (getField(obj, "max_delta_v_t") ?: "0.0").toDouble(),
                avgKerK = (getField(obj, "avg_ker_k") ?: "0.0").toDouble(),
                avgKerE = (getField(obj, "avg_ker_e") ?: "0.0").toDouble(),
                avgKerR = (getField(obj, "avg_ker_r") ?: "0.0").toDouble(),
                avgKerS = (getField(obj, "avg_ker_s") ?: "0.0").toDouble(),
                avgCarbonIntensityGco2Kwh = (getField(obj, "avg_carbon_intensity_gco2_kwh") ?: "0.0").toDouble(),
                countGreenBand = (getField(obj, "count_green_band") ?: "0").toInt(),
                countNeutralBand = (getField(obj, "count_neutral_band") ?: "0").toInt(),
                countRedBand = (getField(obj, "count_red_band") ?: "0").toInt(),
                violationsDvtGlobal = (getField(obj, "violations_dvt_global") ?: "0").toInt(),
                violationsKerNonpositive = (getField(obj, "violations_ker_nonpositive") ?: "0").toInt(),
                violationsJointKerDvt = (getField(obj, "violations_joint_ker_dvt") ?: "0").toInt(),
                violationsRedBandKer = (getField(obj, "violations_red_band_ker") ?: "0").toInt(),
                violationsProdRedBand = (getField(obj, "violations_prod_red_band") ?: "0").toInt()
            )
            result.add(row)
        }
        return result
    }

    fun synapseSafeForEco(limit: Int = 50, primaryPlane: String = "ANY"): List<SynapseSafeRow> {
        val requestJson = """
            { "tool": "synapse_safe_for_eco", "params": { "limit": $limit, "primary_plane": "$primaryPlane" } }
        """.trimIndent()
        val response = sendRequestAndReadResponse(requestJson)
        val rowsJson = extractRowsJson(response)
        val result = mutableListOf<SynapseSafeRow>()
        for (obj in rowsJson) {
            val row = SynapseSafeRow(
                synapseId = (getField(obj, "synapse_id") ?: "0").toInt(),
                producerLang = getField(obj, "producer_lang") ?: "",
                producerRelpath = getField(obj, "producer_relpath") ?: "",
                consumerLang = getField(obj, "consumer_lang") ?: "",
                consumerRelpath = getField(obj, "consumer_relpath") ?: "",
                synapseClass = getField(obj, "synapse_class") ?: "",
                transportKind = getField(obj, "transport_kind") ?: "",
                laneDefault = getField(obj, "lane_default") ?: "",
                primaryPlane = getField(obj, "primary_plane") ?: "",
                nonActuating = (getField(obj, "non_actuating") ?: "1") == "1",
                allowsReadonly = (getField(obj, "allows_readonly") ?: "1") == "1",
                allowsActuation = (getField(obj, "allows_actuation") ?: "0") == "1",
                neuroFlag = (getField(obj, "neuro_flag") ?: "0") == "1",
                kerK = (getField(obj, "ker_k") ?: "0.0").toDouble(),
                kerE = (getField(obj, "ker_e") ?: "0.0").toDouble(),
                kerR = (getField(obj, "ker_r") ?: "0.0").toDouble(),
                kerS = (getField(obj, "ker_s") ?: "0.0").toDouble()
            )
            result.add(row)
        }
        return result
    }

    // Governance helper: find hexes needing attention

    fun hexesNeedingStabilityAttention(limit: Int = 50, primaryPlane: String = "HYDRAULICS"): List<HexStabilityCarbonRow> {
        return hexStabilityCarbon(limit, primaryPlane).filter { row ->
            row.violationsDvtGlobal > 0 || row.violationsJointKerDvt > 0
        }
    }

    fun hexesNeedingCarbonAttention(limit: Int = 50, primaryPlane: String = "HYDRAULICS"): List<HexStabilityCarbonRow> {
        return hexStabilityCarbon(limit, primaryPlane).filter { row ->
            row.countRedBand > 0 || row.violationsRedBandKer > 0 || row.violationsProdRedBand > 0
        }
    }
}

fun main(args: Array<String>) {
    if (args.size != 2) {
        println("Usage: McpKerSynapseClient <server_path> <sqlite_db_path>")
        exitProcess(1)
    }
    val client = McpKerSynapseClient(args[0], args[1])
    try {
        val hexes = client.hexesNeedingStabilityAttention()
        println("Hexes needing stability attention:")
        for (h in hexes) {
            println("${h.hexId} region=${h.regionName} total_delta_v_t=${h.totalDeltaVt} violations_dvt=${h.violationsDvtGlobal}")
        }
    } finally {
        client.close()
    }
}
