// File: kotlin/src/main/kotlin/org/cyboquatic/hex/HexNeighborCache.kt
package org.cyboquatic.hex

import java.io.File
import java.sql.Connection
import java.sql.DriverManager

/**
 * In-memory hex-cell neighbour cache for Phoenix H3 cells.
 *
 * Assumes:
 *  - phoenix_hex_registry.md contains hex entries with H3 indices.
 *  - A precomputed neighbour mapping (e.g., from H3) is available in a sidecar file
 *    or derivable via H3 bindings; here we assume a simple CSV mapping for clarity.
 *
 * Data structures:
 *  - hexIndexMap: Map<String, Long> mapping H3 string indices to Long IDs.
 *  - neighbourMap: Map<Long, List<Long>> mapping Long cell IDs to neighbour IDs.
 *
 * Functions:
 *  - loadFromRegistry(registryPath, neighboursPath)
 *  - getNeighboursWithLST(cellId): retrieves neighbours and their current LST from SQL.
 *  - localLaplacian(cellId): computes discrete Laplacian of LST for consistency equation.
 */

data class HexCellLST(
    val cellId: Long,
    val h3Index: String,
    val morningLstK: Double,
    val afternoonLstK: Double
)

class HexNeighborCache private constructor(
    private val hexIndexMap: Map<String, Long>,
    private val h3IndexReverse: Map<Long, String>,
    private val neighbourMap: Map<Long, List<Long>>,
    private val jdbcUrl: String
) {

    companion object {

        fun loadFromRegistry(
            registryPath: String,
            neighboursPath: String,
            jdbcUrl: String
        ): HexNeighborCache {
            val hexIndexMap = mutableMapOf<String, Long>()
            val h3IndexReverse = mutableMapOf<Long, String>()

            // Parse phoenix_hex_registry.md for H3 indices.
            // Expect lines like: "**H3 Index:** `8a2a1072bffffff`"
            File(registryPath).forEachLine { line ->
                val trimmed = line.trim()
                if (trimmed.startsWith("**H3 Index:**")) {
                    val idx = trimmed.substringAfter("**H3 Index:**").trim()
                    val h3 = idx.removePrefix("`").removeSuffix("`")
                    if (h3.isNotEmpty()) {
                        val id = h3.hashCode().toLong() // simple stable mapping; real code may use H3 integer form
                        hexIndexMap[h3] = id
                        h3IndexReverse[id] = h3
                    }
                }
            }

            // Load neighbours mapping from CSV: h3_index,neighbor_h3_index
            val neighbourMap = mutableMapOf<Long, MutableList<Long>>()
            File(neighboursPath).forEachLine { line ->
                val parts = line.split(",")
                if (parts.size == 2) {
                    val h3 = parts[0].trim()
                    val nb = parts[1].trim()
                    val id = hexIndexMap[h3] ?: return@forEachLine
                    val nbId = hexIndexMap[nb] ?: return@forEachLine
                    neighbourMap.computeIfAbsent(id) { mutableListOf() }.add(nbId)
                }
            }

            return HexNeighborCache(hexIndexMap, h3IndexReverse, neighbourMap, jdbcUrl)
        }
    }

    private fun getConnection(): Connection = DriverManager.getConnection(jdbcUrl)

    fun getNeighboursWithLST(cellId: Long): List<HexCellLST> {
        val h3Index = h3IndexReverse[cellId] ?: return emptyList()
        val neighbourIds = neighbourMap[cellId] ?: emptyList()
        if (neighbourIds.isEmpty()) return emptyList()

        val neighbourH3 = neighbourIds.mapNotNull { h3IndexReverse[it] }
        val placeholders = neighbourH3.joinToString(",") { "?" }

        val sql = """
            SELECT h3_index, morning_lst_k, afternoon_lst_k
            FROM hex_thermal_recovery
            WHERE h3_index IN ($placeholders)
        """.trimIndent()

        getConnection().use { conn ->
            conn.prepareStatement(sql).use { ps ->
                neighbourH3.forEachIndexed { idx, h3 ->
                    ps.setString(idx + 1, h3)
                }
                ps.executeQuery().use { rs ->
                    val result = mutableListOf<HexCellLST>()
                    while (rs.next()) {
                        val h3 = rs.getString("h3_index")
                        val id = hexIndexMap[h3] ?: continue
                        val morning = rs.getDouble("morning_lst_k")
                        val afternoon = rs.getDouble("afternoon_lst_k")
                        result.add(HexCellLST(id, h3, morning, afternoon))
                    }
                    return result
                }
            }
        }
    }

    /**
     * Compute discrete Laplacian of afternoon LST at cellId:
     *   Δ_LST(i) = deg(i) * LST_i - sum_{j∈N(i)} LST_j
     * where deg(i) = number of neighbours.
     */
    fun localLaplacian(cellId: Long): Double {
        val h3Index = h3IndexReverse[cellId] ?: return 0.0
        val neighbours = neighbourMap[cellId] ?: return 0.0
        if (neighbours.isEmpty()) return 0.0

        val lstMap = mutableMapOf<Long, Double>()

        val sqlSelf = """
            SELECT afternoon_lst_k
            FROM hex_thermal_recovery
            WHERE h3_index = ?
            ORDER BY date_utc DESC
            LIMIT 1
        """.trimIndent()

        val sqlNeighbours = """
            SELECT h3_index, afternoon_lst_k
            FROM hex_thermal_recovery
            WHERE h3_index IN (${neighbours.map { "?" }.joinToString(",")})
            ORDER BY date_utc DESC
        """.trimIndent()

        getConnection().use { conn ->
            // Self LST
            conn.prepareStatement(sqlSelf).use { ps ->
                ps.setString(1, h3Index)
                ps.executeQuery().use { rs ->
                    if (rs.next()) {
                        lstMap[cellId] = rs.getDouble("afternoon_lst_k")
                    } else {
                        lstMap[cellId] = 0.0
                    }
                }
            }

            // Neighbours LST (take latest per neighbour)
            conn.prepareStatement(sqlNeighbours).use { ps ->
                neighbours.forEachIndexed { idx, nbId ->
                    val h3Nb = h3IndexReverse[nbId] ?: ""
                    ps.setString(idx + 1, h3Nb)
                }
                ps.executeQuery().use { rs ->
                    while (rs.next()) {
                        val h3Nb = rs.getString("h3_index")
                        val nbId = hexIndexMap[h3Nb] ?: continue
                        val valLst = rs.getDouble("afternoon_lst_k")
                        lstMap[nbId] = valLst
                    }
                }
            }
        }

        val lstSelf = lstMap[cellId] ?: 0.0
        val deg = neighbours.size.toDouble()
        var sumNb = 0.0
        neighbours.forEach { nb -> sumNb += (lstMap[nb] ?: lstSelf) }

        return deg * lstSelf - sumNb
    }
}
