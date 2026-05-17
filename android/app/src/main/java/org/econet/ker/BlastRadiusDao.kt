// filename: android/app/src/main/java/org/econet/ker/BlastRadiusDao.kt
// destination: mk-bluebird/eco_restoration_shard/android/.../BlastRadiusDao.kt
// role: Android‑side readonly visualizer DAO (no actuation)

package org.econet.ker

import android.database.Cursor
import android.database.sqlite.SQLiteDatabase

data class NodeEnergyCarbon(
    val nodeId: String,
    val nEvents: Long,
    val eReqAcceptJ: Double,
    val eSurplusAcceptJ: Double,
    val rCarbonAvg: Double?,
    val rBiodivAvg: Double?,
    val dvAvg: Double?
)

data class CandidateEcorestorative(
    val sourceType: String,
    val sourceId: String,
    val impactCarbon: Double,
    val impactBiodiv: Double,
    val vtSensitivityAvg: Double,
    val dvAvg: Double
)

object BlastRadiusDao {

    fun queryBestNodes(db: SQLiteDatabase, limit: Int = 32): List<NodeEnergyCarbon> {
        val list = mutableListOf<NodeEnergyCarbon>()
        val cursor: Cursor = db.rawQuery(
            """
            SELECT node_id, n_events, e_req_accept_j, e_surplus_accept_j,
                   r_carbon_avg, r_biodiv_avg, dv_avg
            FROM v_node_energy_carbon
            WHERE n_events >= 5
              AND dv_avg <= 0.0
              AND e_surplus_accept_j >= e_req_accept_j
            ORDER BY r_carbon_avg ASC, e_req_accept_j ASC
            LIMIT ?
            """.trimIndent(),
            arrayOf(limit.toString())
        )
        cursor.use {
            while (it.moveToNext()) {
                val nodeId = it.getString(0)
                val nEvents = it.getLong(1)
                val eReq = it.getDouble(2)
                val eSurplus = it.getDouble(3)
                val rCarbon = if (it.isNull(4)) null else it.getDouble(4)
                val rBiodiv = if (it.isNull(5)) null else it.getDouble(5)
                val dvAvg = if (it.isNull(6)) null else it.getDouble(6)
                list.add(
                    NodeEnergyCarbon(
                        nodeId,
                        nEvents,
                        eReq,
                        eSurplus,
                        rCarbon,
                        rBiodiv,
                        dvAvg
                    )
                )
            }
        }
        return list
    }

    fun queryCandidates(db: SQLiteDatabase, limit: Int = 32): List<CandidateEcorestorative> {
        val list = mutableListOf<CandidateEcorestorative>()
        val cursor: Cursor = db.rawQuery(
            """
            SELECT source_type, source_id, impact_carbon, impact_biodiv,
                   vt_sensitivity_avg, dv_avg
            FROM v_candidate_ecorestorative
            ORDER BY impact_carbon DESC, impact_biodiv DESC
            LIMIT ?
            """.trimIndent(),
            arrayOf(limit.toString())
        )
        cursor.use {
            while (it.moveToNext()) {
                val sourceType = it.getString(0)
                val sourceId = it.getString(1)
                val impactCarbon = it.getDouble(2)
                val impactBiodiv = it.getDouble(3)
                val vtSens = it.getDouble(4)
                val dvAvg = it.getDouble(5)
                list.add(
                    CandidateEcorestorative(
                        sourceType,
                        sourceId,
                        impactCarbon,
                        impactBiodiv,
                        vtSens,
                        dvAvg
                    )
                )
            }
        }
        return list
    }
}
