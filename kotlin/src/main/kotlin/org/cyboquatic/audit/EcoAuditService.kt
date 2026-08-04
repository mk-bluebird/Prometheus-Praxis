// File: kotlin/src/main/kotlin/org/cyboquatic/audit/EcoAuditService.kt
package org.cyboquatic.audit

import java.sql.Connection
import java.sql.DriverManager

/**
 * Kotlin eco-audit micro-service that logs KER violations against the DID
 * governance particle. This is invoked from Java via a bridge class.
 */
object EcoAuditService {

    private const val JDBC_URL = "jdbc:sqlite:./data/cyboquatic_workload.db"

    fun logKerEViolation(did: String,
                         basinId: String,
                         timestampS: Double,
                         kerE: Double,
                         message: String) {
        DriverManager.getConnection(JDBC_URL).use { conn ->
            conn.autoCommit = false
            val sql = """
                INSERT INTO eco_audit_log (did, telemetry_id, timestamp_s, basin_id, ker_e, message)
                VALUES (?, -1, ?, ?, ?, ?)
            """.trimIndent()
            conn.prepareStatement(sql).use { ps ->
                ps.setString(1, did)
                ps.setDouble(2, timestampS)
                ps.setString(3, basinId)
                ps.setDouble(4, kerE)
                ps.setString(5, message)
                ps.executeUpdate()
            }
            conn.commit()
        }
    }
}
