// File: kotlin/cyboquatic/EcoRestorationDashboard.kt
package cyboquatic

import java.sql.Connection
import java.sql.DriverManager
import java.sql.ResultSet

data class CanalNodeSummary(
    val nodeCode: String,
    val description: String,
    val kerBand: String,
    val fogBand: String,
    val canalPlane: String,
    val avgDeltaVt: Double,
    val avgEnergyInputJ: Double,
    val avgPfasUgL: Double,
    val minKerScore: Double
)

data class PfasCorridorSummary(
    val nodeCode: String,
    val massKg: Double,
    val sorbedFraction: Double,
    val coldSurvivalFactor: Double
)

data class KerHexSummary(
    val hexId: String,
    val rHydraulics: Double,
    val rEnergy: Double,
    val rTopology: Double,
    val rBiodiversity: Double,
    val Vt: Double
)

object EcoRestorationDashboard {

    private fun connect(dbPath: String): Connection {
        val url = "jdbc:sqlite:$dbPath"
        return DriverManager.getConnection(url)
    }

    private fun queryCanalNodes(conn: Connection): List<CanalNodeSummary> {
        val sql = """
            SELECT cn.node_code,
                   cn.description,
                   cn.ker_band,
                   cn.fog_band,
                   cn.canal_plane,
                   IFNULL(v.avg_deltaVt, 0.0) AS avg_deltaVt,
                   IFNULL(v.avg_energy_input_J, 0.0) AS avg_energy_input_J,
                   IFNULL(v.avg_pfas_ugL, 0.0) AS avg_pfas_ugL,
                   IFNULL(v.min_ker_score, 0.0) AS min_ker_score
            FROM canal_node cn
            LEFT JOIN v_cyboquatic_workload_ker_summary v
              ON v.node_code = cn.node_code;
        """.trimIndent()

        conn.createStatement().use { st ->
            st.executeQuery(sql).use { rs ->
                val out = mutableListOf<CanalNodeSummary>()
                while (rs.next()) {
                    out.add(
                        CanalNodeSummary(
                            nodeCode = rs.getString("node_code"),
                            description = rs.getString("description"),
                            kerBand = rs.getString("ker_band"),
                            fogBand = rs.getString("fog_band"),
                            canalPlane = rs.getString("canal_plane"),
                            avgDeltaVt = rs.getDouble("avg_deltaVt"),
                            avgEnergyInputJ = rs.getDouble("avg_energy_input_J"),
                            avgPfasUgL = rs.getDouble("avg_pfas_ugL"),
                            minKerScore = rs.getDouble("min_ker_score")
                        )
                    )
                }
                return out
            }
        }
    }

    private fun queryPfasCorridors(conn: Connection): List<PfasCorridorSummary> {
        val sql = """
            SELECT cn.node_code,
                   ps.mass_kg,
                   ps.sorbed_fraction,
                   ps.cold_survival_factor
            FROM canal_node cn
            JOIN pfas_corridor_state ps
              ON ps.node_id = cn.node_id;
        """.trimIndent()

        conn.createStatement().use { st ->
            val out = mutableListOf<PfasCorridorSummary>()
            st.executeQuery(sql).use { rs ->
                while (rs.next()) {
                    out.add(
                        PfasCorridorSummary(
                            nodeCode = rs.getString("node_code"),
                            massKg = rs.getDouble("mass_kg"),
                            sorbedFraction = rs.getDouble("sorbed_fraction"),
                            coldSurvivalFactor = rs.getDouble("cold_survival_factor")
                        )
                    )
                }
            }
            return out
        }
    }

    private fun queryKerHexes(conn: Connection): List<KerHexSummary> {
        val sql = """
            SELECT hex_id,
                   r_hydraulics,
                   r_energy,
                   r_topology,
                   r_biodiversity,
                   w_h, w_e, w_t, w_b
            FROM phoenix_hex_registry;
        """.trimIndent()

        conn.createStatement().use { st ->
            val out = mutableListOf<KerHexSummary>()
            st.executeQuery(sql).use { rs ->
                while (rs.next()) {
                    val rH = rs.getDouble("r_hydraulics")
                    val rE = rs.getDouble("r_energy")
                    val rT = rs.getDouble("r_topology")
                    val rB = rs.getDouble("r_biodiversity")
                    val wH = rs.getDouble("w_h")
                    val wE = rs.getDouble("w_e")
                    val wT = rs.getDouble("w_t")
                    val wB = rs.getDouble("w_b")

                    val Vt = wH * rH * rH +
                             wE * rE * rE +
                             wT * rT * rT +
                             wB * rB * rB

                    out.add(
                        KerHexSummary(
                            hexId = rs.getString("hex_id"),
                            rHydraulics = rH,
                            rEnergy = rE,
                            rTopology = rT,
                            rBiodiversity = rB,
                            Vt = Vt
                        )
                    )
                }
            }
            return out
        }
    }

    private fun printCanalNodeSummary(nodes: List<CanalNodeSummary>) {
        println("=== Canal Nodes ===")
        for (n in nodes) {
            println("Node ${n.nodeCode} [${n.canalPlane}]")
            println("  Description   : ${n.description}")
            println("  KER band      : ${n.kerBand}")
            println("  FOG band      : ${n.fogBand}")
            println("  avgDeltaVt    : ${"%.4f".format(n.avgDeltaVt)}")
            println("  avgEnergyIn J : ${"%.2f".format(n.avgEnergyInputJ)}")
            println("  avgPFAS ug/L  : ${"%.4f".format(n.avgPfasUgL)}")
            println("  minKerScore   : ${"%.4f".format(n.minKerScore)}")
            println()
        }
    }

    private fun printPfasSummary(pfas: List<PfasCorridorSummary>) {
        println("=== PFAS Corridors ===")
        for (p in pfas) {
            println("Node ${p.nodeCode}")
            println("  mass_kg             : ${"%.6f".format(p.massKg)}")
            println("  sorbed_fraction     : ${"%.3f".format(p.sorbedFraction)}")
            println("  cold_survival_factor: ${"%.3f".format(p.coldSurvivalFactor)}")
            println()
        }
    }

    private fun printKerHexSummary(hexes: List<KerHexSummary>) {
        println("=== KER Hex Anchors ===")
        for (h in hexes) {
            val rMax = listOf(h.rHydraulics, h.rEnergy, h.rTopology, h.rBiodiversity).maxOrNull() ?: 0.0
            val e = (1.0 - rMax).coerceAtLeast(0.0)
            val k = 0.9 // example band, consistent with KER docs.[59]
            val s = k * e - rMax
            println("Hex ${h.hexId}")
            println("  r_hydraulics  : ${"%.3f".format(h.rHydraulics)}")
            println("  r_energy      : ${"%.3f".format(h.rEnergy)}")
            println("  r_topology    : ${"%.3f".format(h.rTopology)}")
            println("  r_biodiversity: ${"%.3f".format(h.rBiodiversity)}")
            println("  Vt            : ${"%.4f".format(h.Vt)}")
            println("  r_max         : ${"%.3f".format(rMax)}")
            println("  k,e,ker_score : k=$k e=${"%.3f".format(e)} ker=${"%.4f".format(s)}")
            println()
        }
    }

    @JvmStatic
    fun main(args: Array<String>) {
        if (args.isEmpty()) {
            println("Usage: EcoRestorationDashboard <sqlite-db-path>")
            return
        }
        val dbPath = args[0]
        connect(dbPath).use { conn ->
            val nodes = queryCanalNodes(conn)
            val pfas = queryPfasCorridors(conn)
            val hexes = queryKerHexes(conn)

            printCanalNodeSummary(nodes)
            printPfasSummary(pfas)
            printKerHexSummary(hexes)
        }
    }
}
