// File: kotlin/eco/MaterialImpactCli.kt
package eco

import java.sql.Connection
import java.sql.DriverManager
import java.sql.ResultSet

data class MaterialRow(
    val materialName: String,
    val isoO2Percent: Double,
    val isoCo2Percent: Double,
    val oecdBodPercent: Double,
    val oecdDocPercent: Double,
    val daysToPassWindow: Double,
    val toxicityScore: Double,
    val pfasPresence: Double,
    val biodegradabilityScore: Double,
    val ecoImpactBand: String
)

object MaterialImpactCli {

    private fun connect(dbPath: String): Connection {
        val url = "jdbc:sqlite:$dbPath"
        return DriverManager.getConnection(url)
    }

    private fun loadMaterials(conn: Connection, bandFilter: String?): List<MaterialRow> {
        val sql = if (bandFilter != null) {
            "SELECT material_name, iso_14851_o2_percent, iso_14855_co2_percent," +
            "       oecd_301_bod_percent, oecd_301_doc_percent, days_to_pass_window," +
            "       toxicity_score, pfas_presence, biodegradability_score, eco_impact_band" +
            "  FROM eco_material_catalog WHERE eco_impact_band = ?;"
        } else {
            "SELECT material_name, iso_14851_o2_percent, iso_14855_co2_percent," +
            "       oecd_301_bod_percent, oecd_301_doc_percent, days_to_pass_window," +
            "       toxicity_score, pfas_presence, biodegradability_score, eco_impact_band" +
            "  FROM eco_material_catalog;"
        }

        conn.prepareStatement(sql).use { ps ->
            if (bandFilter != null) {
                ps.setString(1, bandFilter)
            }
            ps.executeQuery().use { rs ->
                val out = mutableListOf<MaterialRow>()
                while (rs.next()) {
                    out.add(
                        MaterialRow(
                            materialName = rs.getString("material_name"),
                            isoO2Percent = rs.getDouble("iso_14851_o2_percent"),
                            isoCo2Percent = rs.getDouble("iso_14855_co2_percent"),
                            oecdBodPercent = rs.getDouble("oecd_301_bod_percent"),
                            oecdDocPercent = rs.getDouble("oecd_301_doc_percent"),
                            daysToPassWindow = rs.getDouble("days_to_pass_window"),
                            toxicityScore = rs.getDouble("toxicity_score"),
                            pfasPresence = rs.getDouble("pfas_presence"),
                            biodegradabilityScore = rs.getDouble("biodegradability_score"),
                            ecoImpactBand = rs.getString("eco_impact_band")
                        )
                    )
                }
                return out
            }
        }
    }

    private fun callCppMaterialScore(material: MaterialRow, cppBinaryPath: String): String {
        // Call C++ eco-impact engine via command-line (no new crates).
        // The C++ CLI expects: material-score O2 CO2 BOD DOC days toxicity pfas.
        val args = listOf(
            cppBinaryPath, "material-score",
            material.isoO2Percent.toString(),
            material.isoCo2Percent.toString(),
            material.oecdBodPercent.toString(),
            material.oecdDocPercent.toString(),
            material.daysToPassWindow.toString(),
            material.toxicityScore.toString(),
            material.pfasPresence.toString()
        )
        val pb = ProcessBuilder(args)
        pb.redirectErrorStream(true)
        val proc = pb.start()
        val out = proc.inputStream.bufferedReader().readText()
        proc.waitFor()
        return out
    }

    private fun printComparativeScores(materials: List<MaterialRow>, cppBinaryPath: String) {
        println("=== Eco Material Impact Comparison ===")
        for (m in materials) {
            println("Material: ${m.materialName} [band=${m.ecoImpactBand}]")
            println("  ISO 14851 O2%   : ${"%.1f".format(m.isoO2Percent)}")
            println("  ISO 14855 CO2%  : ${"%.1f".format(m.isoCo2Percent)}")
            println("  OECD 301 BOD%   : ${"%.1f".format(m.oecdBodPercent)}")
            println("  OECD 301 DOC%   : ${"%.1f".format(m.oecdDocPercent)}")
            println("  days_to_pass    : ${"%.1f".format(m.daysToPassWindow)}")
            println("  toxicity_score  : ${"%.2f".format(m.toxicityScore)}")
            println("  pfas_presence   : ${"%.2f".format(m.pfasPresence)}")
            println("  biodegradability: ${"%.2f".format(m.biodegradabilityScore)}")
            println("  C++ eco-impact CLI output:")
            val cliOut = callCppMaterialScore(m, cppBinaryPath)
            println(cliOut.trim())
            println()
        }
    }

    @JvmStatic
    fun main(args: Array<String>) {
        if (args.size < 2) {
            println("Usage: MaterialImpactCli <sqlite-db-path> <cpp-cli-path> [eco_band]")
            println("  eco_band: optional eco_impact_band filter (LOW|MEDIUM|HIGH)")
            return
        }
        val dbPath = args[0]
        val cppCliPath = args[1]
        val bandFilter = if (args.size >= 3) args[2] else null

        connect(dbPath).use { conn ->
            val materials = loadMaterials(conn, bandFilter)
            if (materials.isEmpty()) {
                println("No materials found in eco_material_catalog.")
                return
            }
            printComparativeScores(materials, cppCliPath)
        }
    }
}
