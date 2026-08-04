// File: kotlin/src/main/kotlin/org/cyboquatic/fog/EcostressIngestService.kt
package org.cyboquatic.fog

import org.cyboquatic.hex.H3
import org.cyboquatic.sql.SqlClient

data class EcostressPixel(
    val lat: Double,
    val lon: Double,
    val lstK: Double,
    val timeOfDay: TimeOfDay,
    val quality: String
)

enum class TimeOfDay {
    MORNING,
    AFTERNOON
}

data class HexLST(
    val h3Index: String,
    val morningLstK: Double,
    val afternoonLstK: Double,
    val lstDropC: Double,
    val qualityFlag: String
)

interface CorridorPlannerNotifier {
    suspend fun notifyUpdate()
}

class EcostressIngestService(
    private val h3Resolution: Int,
    private val sqlClient: SqlClient,
    private val corridorPlannerNotifier: CorridorPlannerNotifier
) {

    suspend fun runOnce() {
        val pixels = fetchEcostressTiles()
        if (pixels.isEmpty()) return
        val hexAggregates = aggregateToHex(pixels)
        upsertHexThermalRecovery(hexAggregates)
        corridorPlannerNotifier.notifyUpdate()
    }

    private fun fetchEcostressTiles(): List<EcostressPixel> {
        // Implementation must call NASA ECOSTRESS API and return parsed pixels.
        return emptyList()
    }

    private fun aggregateToHex(pixels: List<EcostressPixel>): List<HexLST> {
        val hexMap = mutableMapOf<String, MutableList<EcostressPixel>>()
        for (p in pixels) {
            val h3Index = H3.geoToH3(p.lat, p.lon, h3Resolution)
            hexMap.getOrPut(h3Index) { mutableListOf() }.add(p)
        }

        return hexMap.map { (h3, pixList) ->
            val morning = pixList.filter {
                it.timeOfDay == TimeOfDay.MORNING && it.quality == "CLEAR"
            }
            val afternoon = pixList.filter {
                it.timeOfDay == TimeOfDay.AFTERNOON && it.quality == "CLEAR"
            }

            val morningLstK = if (morning.isNotEmpty()) {
                morning.map { it.lstK }.average()
            } else {
                Double.NaN
            }

            val afternoonLstK = if (afternoon.isNotEmpty()) {
                afternoon.map { it.lstK }.average()
            } else {
                Double.NaN
            }

            val lstDropC = if (!morningLstK.isNaN() && !afternoonLstK.isNaN()) {
                (afternoonLstK - morningLstK)
            } else {
                Double.NaN
            }

            val qualityFlag =
                if (morning.isEmpty() || afternoon.isEmpty()) "LOW_QUAL" else "CLEAR"

            HexLST(
                h3Index = h3,
                morningLstK = morningLstK,
                afternoonLstK = afternoonLstK,
                lstDropC = lstDropC,
                qualityFlag = qualityFlag
            )
        }
    }

    private suspend fun upsertHexThermalRecovery(hexLSTs: List<HexLST>) {
        if (hexLSTs.isEmpty()) return
        sqlClient.withTransaction {
            hexLSTs.forEach { h ->
                if (h.qualityFlag == "CLOUD") return@forEach
                if (h.morningLstK.isNaN() || h.afternoonLstK.isNaN() || h.lstDropC.isNaN()) {
                    return@forEach
                }

                sqlClient.execute(
                    """
                    INSERT INTO hex_thermal_recovery
                      (h3_index,
                       morning_lst_k,
                       afternoon_lst_k,
                       lst_drop_c,
                       baseline_lst_c,
                       period_start_utc,
                       period_end_utc,
                       quality_flag)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                    ON CONFLICT(h3_index) DO UPDATE SET
                      morning_lst_k   = excluded.morning_lst_k,
                      afternoon_lst_k = excluded.afternoon_lst_k,
                      lst_drop_c      = excluded.lst_drop_c,
                      quality_flag    = excluded.quality_flag;
                    """.trimIndent(),
                    h.h3Index,
                    h.morningLstK,
                    h.afternoonLstK,
                    h.lstDropC,
                    h.afternoonLstK,
                    currentStartUtc(),
                    currentEndUtc(),
                    h.qualityFlag
                )
            }
        }
    }

    private fun currentStartUtc(): Long {
        return System.currentTimeMillis() / 1000L
    }

    private fun currentEndUtc(): Long {
        return System.currentTimeMillis() / 1000L
    }
}
