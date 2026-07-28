// filename: src/kotlin/org/prometheuspraxis/cyboquatic/CyboquaticDrainageSummary.kt
// license: MIT OR Apache-2.0
// role: Non-actuating drainage-decay summary tool over dbcyboquaticdrainagedecayindex.sql.
// note: Uses only Kotlin stdlib and JDBC; no external dependencies, no device IO, no actuation.

package org.prometheuspraxis.cyboquatic

import java.io.BufferedWriter
import java.io.File
import java.io.FileWriter
import java.nio.charset.StandardCharsets
import java.sql.Connection
import java.sql.DriverManager
import java.sql.ResultSet
import java.util.Locale

data class DrainageFrameSample(
    val canalNodeId: String,
    val timestampUtc: String,
    val bodMgL: Double,
    val tssMgL: Double,
    val cecCmolPerKg: Double,
    val kKnowledge: Double,
    val eEcoImpact: Double,
    val rRisk: Double,
    val kerScore: Double
)

data class DrainageSummary(
    val samples: List<DrainageFrameSample>,
    val totalFrames: Int,
    val uniqueNodes: Int,
    val avgKerScore: Double,
    val worstKerScore: Double,
    val worstSample: DrainageFrameSample?
)

object CyboquaticDrainageSummary {

    @JvmStatic
    fun main(args: Array<String>) {
        if (args.size != 2) {
            System.err.println("Usage: kotlin CyboquaticDrainageSummary <db_path> <output_md>")
            System.exit(1)
        }
        val dbPath = args[0]
        val outputMd = File(args[1])
        val summary = loadSummary(dbPath)
        writeMarkdown(summary, outputMd)
        println("Drainage summary written to ${outputMd.absolutePath}")
    }

    private fun loadSummary(dbPath: String): DrainageSummary {
        val url = "jdbc:sqlite:$dbPath"
        val samples = mutableListOf<DrainageFrameSample>()
        val nodes = mutableSetOf<String>()

        var kerSum = 0.0
        var worstKer = Double.MAX_VALUE
        var worstSample: DrainageFrameSample? = null

        DriverManager.getConnection(url).use { conn ->
            val sql = """
                SELECT 
                    canalnodeid,
                    timestamputc,
                    bodmgl,
                    tssmgl,
                    ceccmolperkg,
                    kknowledgefactor AS k,
                    eecoimpact AS e,
                    rriskfactor AS r,
                    kerscore
                FROM drainagedecayframe
            """.trimIndent()
            conn.prepareStatement(sql).use { ps ->
                ps.executeQuery().use { rs ->
                    while (rs.next()) {
                        val sample = mapRow(rs)
                        samples.add(sample)
                        nodes.add(sample.canalNodeId)
                        kerSum += sample.kerScore
                        if (sample.kerScore < worstKer) {
                            worstKer = sample.kerScore
                            worstSample = sample
                        }
                    }
                }
            }
        }

        val total = samples.size
        val avgKer = if (total > 0) kerSum / total else 0.0
        return DrainageSummary(
            samples = samples,
            totalFrames = total,
            uniqueNodes = nodes.size,
            avgKerScore = avgKer,
            worstKerScore = if (worstKer == Double.MAX_VALUE) 0.0 else worstKer,
            worstSample = worstSample
        )
    }

    private fun mapRow(rs: ResultSet): DrainageFrameSample {
        val nodeId = rs.getString("canalnodeid")
        val ts = rs.getString("timestamputc")
        val bod = rs.getDouble("bodmgl")
        val tss = rs.getDouble("tssmgl")
        val cec = rs.getDouble("ceccmolperkg")
        val k = rs.getDouble("k")
        val e = rs.getDouble("e")
        val r = rs.getDouble("r")
        val ker = rs.getDouble("kerscore")
        return DrainageFrameSample(
            canalNodeId = nodeId,
            timestampUtc = ts,
            bodMgL = bod,
            tssMgL = tss,
            cecCmolPerKg = cec,
            kKnowledge = k,
            eEcoImpact = e,
            rRisk = r,
            kerScore = ker
        )
    }

    private fun writeMarkdown(summary: DrainageSummary, outputFile: File) {
        BufferedWriter(FileWriter(outputFile, StandardCharsets.UTF_8)).use { writer ->
            writer.write("# Cyboquatic Drainage-Decay Summary\n\n")
            writer.write("This report summarizes BOD/TSS/CEC frames and KER metrics from `drainagedecayframe` in the cyboquatic drainage-decay index.\n\n")

            writer.write("## Summary\n")
            writer.write("- Total frames: ${summary.totalFrames}\n")
            writer.write("- Unique canal nodes: ${summary.uniqueNodes}\n")
            writer.write("- Average KER score: ${formatDouble(summary.avgKerScore)}\n")
            writer.write("- Worst KER score: ${formatDouble(summary.worstKerScore)}\n")
            summary.worstSample?.let { ws ->
                writer.write("- Worst frame: node `${ws.canalNodeId}`, timestamp `${ws.timestampUtc}`\n")
            }
            writer.write("\n")

            writer.write("## Frames\n")
            writer.write("| Canal Node | Timestamp (UTC) | BOD (mg/L) | TSS (mg/L) | CEC (cmol/kg) | K | E | R | KER |\n")
            writer.write("|-----------|-----------------|-----------:|-----------:|-------------:|---:|---:|---:|----:|\n")
            for (s in summary.samples) {
                writer.write(
                    "|${escapeMarkdown(s.canalNodeId)}" +
                        "|${escapeMarkdown(s.timestampUtc)}" +
                        "|${formatDouble(s.bodMgL)}" +
                        "|${formatDouble(s.tssMgL)}" +
                        "|${formatDouble(s.cecCmolPerKg)}" +
                        "|${formatDouble(s.kKnowledge)}" +
                        "|${formatDouble(s.eEcoImpact)}" +
                        "|${formatDouble(s.rRisk)}" +
                        "|${formatDouble(s.kerScore)}|\n"
                )
            }
        }
    }

    private fun formatDouble(d: Double): String {
        return String.format(Locale.US, "%.3f", d)
    }

    private fun escapeMarkdown(s: String?): String {
        if (s == null) return ""
        return s
            .replace("|", "\\|")
            .replace("`", "\\`")
    }
}
