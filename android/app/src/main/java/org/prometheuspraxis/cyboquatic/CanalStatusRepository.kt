// File: android/app/src/main/java/org/prometheuspraxis/cyboquatic/CanalStatusRepository.kt
package org.prometheuspraxis.cyboquatic

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import java.io.File

class CanalStatusRepository(context: Context) {
    private val databaseFile = File(context.filesDir, "cyboquatic.sqlite")

    fun latestFrames(): List<WorkloadFrame> {
        if (!databaseFile.exists()) return emptyList()

        return SQLiteDatabase.openDatabase(
            databaseFile.path,
            null,
            SQLiteDatabase.OPEN_READONLY
        ).use { database ->
            database.rawQuery(
                """
                SELECT canal_node, observed_utc, energyreq_j, delta_vt,
                       eco_impact_value, accepted
                FROM cyboquatic_workload_frame
                WHERE frame_id IN (
                    SELECT MAX(frame_id)
                    FROM cyboquatic_workload_frame
                    GROUP BY canal_node
                )
                ORDER BY canal_node
                """.trimIndent(),
                null
            ).use { cursor ->
                buildList {
                    while (cursor.moveToNext()) add(WorkloadBridge.toWorkloadFrame(cursor))
                }
            }
        }
    }
}
