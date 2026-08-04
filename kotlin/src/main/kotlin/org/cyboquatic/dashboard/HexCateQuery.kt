// File: kotlin/src/main/kotlin/org/cyboquatic/dashboard/HexCateQuery.kt
package org.cyboquatic.dashboard

import java.sql.DriverManager

data class HexCateValue(
    val h3Index: String,
    val cateLstDropK: Double,
    val costUnit: Double,
    val lstDropPerCost: Double
)

/**
 * Kotlin helper to query CATE-based hex ranking for canopy interventions.
 */
object HexCateQuery {

    private const val JDBC_URL = "jdbc:sqlite:./data/hex_cate.db"

    fun topKCanopyHexes(k: Int, confidenceMin: Double = 0.8): List<HexCateValue> {
        DriverManager.getConnection(JDBC_URL).use { conn ->
            val sql = """
                SELECT h3_index, cate_lst_drop_k, cost_unit, lst_drop_per_cost
                FROM hex_cate_canopy_value
                WHERE cate_confidence >= ?
                ORDER BY lst_drop_per_cost DESC
                LIMIT ?
            """.trimIndent()
            conn.prepareStatement(sql).use { ps ->
                ps.setDouble(1, confidenceMin)
                ps.setInt(2, k)
                ps.executeQuery().use { rs ->
                    val result = mutableListOf<HexCateValue>()
                    while (rs.next()) {
                        result.add(
                            HexCateValue(
                                h3Index = rs.getString("h3_index"),
                                cateLstDropK = rs.getDouble("cate_lst_drop_k"),
                                costUnit = rs.getDouble("cost_unit"),
                                lstDropPerCost = rs.getDouble("lst_drop_per_cost")
                            )
                        )
                    }
                    return result
                }
            }
        }
    }
}
