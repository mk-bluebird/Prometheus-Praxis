// Filename: android/app/src/main/java/org/prometheuspraxis/fog/FogRoutingDecisionDao.kt
// Room DAO bound to the shared SQLite blast-radius / routing DB.

package org.prometheuspraxis.fog

import androidx.room.Dao
import androidx.room.Query

@Dao
interface FogRoutingDecisionDao {

    @Query(
        """
        SELECT timestamp_utc AS timestampUtc,
               node_id      AS nodeId,
               previous_v   AS previousV,
               current_v    AS currentV,
               verdict      AS verdict,
               diagnostic_only AS diagnosticOnly,
               evidence_hex AS evidenceHex
        FROM fog_routing_decisions
        ORDER BY timestamp_utc DESC
        LIMIT 256
        """
    )
    suspend fun getRecentDecisions(): List<FogRoutingDecisionRow>
}
