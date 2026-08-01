// File: kotlin/cyboquatic/FogRouter.kt
package cyboquatic

import java.sql.Connection
import java.sql.DriverManager
import java.sql.ResultSet

data class FogDecision(
    val hexId: String,
    val canalNodeId: String,
    val route: String,
    val reason: String
)

class FogRouter(private val dbPath: String) {

    private fun connect(): Connection {
        val url = "jdbc:sqlite:$dbPath"
        return DriverManager.getConnection(url)
    }

    fun evaluateDecisions(): List<FogDecision> {
        val decisions = mutableListOf<FogDecision>()
        val sql = """
            SELECT f.hex_id, f.canal_node_id,
                   f.fog_concentration_mgL,
                   f.unmodeled_media_flag,
                   w.R, w.energyreqJ
            FROM fog_flow f
            LEFT JOIN workload_cycle w
              ON f.hex_id = w.hex_id
             AND f.canal_node_id = w.canal_node_id
        """.trimIndent()

        connect().use { conn ->
            conn.createStatement().use { stmt ->
                val rs: ResultSet = stmt.executeQuery(sql)
                while (rs.next()) {
                    val hexId = rs.getString("hex_id")
                    val canalNodeId = rs.getString("canal_node_id")
                    val fogConc = rs.getDouble("fog_concentration_mgL")
                    val unmodeled = rs.getDouble("unmodeled_media_flag")
                    val R = rs.getDouble("R")
                    val energyreqJ = rs.getDouble("energyreqJ")

                    val decision = decideRoute(
                        hexId,
                        canalNodeId,
                        fogConc,
                        unmodeled,
                        R,
                        energyreqJ
                    )
                    decisions.add(decision)
                }
            }
        }
        return decisions
    }

    private fun decideRoute(
        hexId: String,
        canalNodeId: String,
        fogConc: Double,
        unmodeledFlag: Double,
        residualRisk: Double,
        energyreqJ: Double
    ): FogDecision {
        val highFog = fogConc > 100.0
        val highUnmodeled = unmodeledFlag > 0.5
        val highRisk = residualRisk > 0.7
        val highEnergy = energyreqJ > 50000.0

        val route: String
        val reason: String

        if (highFog || highUnmodeled || highRisk) {
            route = "SAFE_TREATMENT_BASIN"
            reason = "High FOG/unmodeled media or residual risk; route to treatment basin."
        } else if (highEnergy) {
            route = "EFFICIENCY_RETUNING"
            reason = "High energy requirement; schedule retuning to reduce energyreqJ."
        } else {
            route = "STANDARD_CANAL_FLOW"
            reason = "FOG, risk, and energy within safe corridors."
        }

        return FogDecision(hexId, canalNodeId, route, reason)
    }
}

fun main(args: Array<String>) {
    if (args.isEmpty()) {
        println("Usage: FogRouter <db_path>")
        return
    }
    val router = FogRouter(args[0])
    val decisions = router.evaluateDecisions()
    decisions.forEach {
        println("hex=${it.hexId} node=${it.canalNodeId} route=${it.route} reason=${it.reason}")
    }
}
