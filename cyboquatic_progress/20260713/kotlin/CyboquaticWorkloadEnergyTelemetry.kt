// File: eco_restoration_shard/cyboquatic_progress/20260713/kotlin/CyboquaticWorkloadEnergyTelemetry.kt
// Domain: (d) Cyboquatic workload in Kotlin + SQL telemetry.
// Purpose: Kotlin helper to compute K,E,R metrics and emit workload samples into SQLite.

package org.cyboquatic.progress

import java.sql.Connection
import java.sql.DriverManager
import java.sql.PreparedStatement

data class WorkloadRiskVector(
    val renergy: Double,
    val rhydraulic: Double,
    val runcertainty: Double
) {
    fun residual(): Double {
        val WENERGY = 0.8
        val WHYDRAULIC = 1.0
        val WUNCERTAINTY = 0.6
        return WENERGY * renergy * renergy +
                WHYDRAULIC * rhydraulic * rhydraulic +
                WUNCERTAINTY * runcertainty * runcertainty
    }

    companion object {
        private fun clamp01(x: Double): Double =
            when {
                x < 0.0 -> 0.0
                x > 1.0 -> 1.0
                else -> x
            }

        fun normalized(
            energyReqJ: Double,
            energySurplusJ: Double,
            hydraulicRisk: Double,
            uncertaintyRisk: Double
        ): WorkloadRiskVector {
            val ENERGY_TAILWIND_SAFE_RATIO = 1.2
            val ENERGY_MIN_RATIO = 0.0
            val ENERGY_MAX_RATIO = 2.5

            val ratio = if (energyReqJ <= 0.0) {
                ENERGY_MAX_RATIO
            } else {
                energySurplusJ / energyReqJ
            }

            val renergyRaw = when {
                ratio >= ENERGY_TAILWIND_SAFE_RATIO -> 0.0
                ratio <= ENERGY_MIN_RATIO -> 1.0
                else -> {
                    var bounded = ratio
                    if (bounded > ENERGY_MAX_RATIO) bounded = ENERGY_MAX_RATIO
                    val span = ENERGY_TAILWIND_SAFE_RATIO - ENERGY_MIN_RATIO
                    val rel = (bounded - ENERGY_MIN_RATIO) / span
                    var valRisk = 1.0 - rel
                    if (valRisk < 0.0) valRisk = 0.0
                    if (valRisk > 1.0) valRisk = 1.0
                    valRisk
                }
            }

            return WorkloadRiskVector(
                clamp01(renergyRaw),
                clamp01(hydraulicRisk),
                clamp01(uncertaintyRisk)
            )
        }
    }
}

data class WorkloadKer(
    val vt: Double,
    val deltaVt: Double,
    val k: Double,
    val e: Double,
    val r: Double
)

data class WorkloadSampleRow(
    val yyyymmdd: String,
    val domain: String,
    val subtaskId: String,
    val nodeId: String,
    val sampleId: String,
    val timestampUtc: String,
    val energyReqJ: Double,
    val energySurplusJ: Double,
    val hydraulicRisk: Double,
    val uncertaintyRisk: Double,
    val risk: WorkloadRiskVector,
    val vtBefore: Double,
    val vtAfter: Double,
    val deltaVt: Double,
    val k: Double,
    val e: Double,
    val r: Double,
    val phoenixHex: String,
    val priorPointer: String
)

object CyboquaticWorkloadEnergyTelemetry {

    fun computeKer(risk: WorkloadRiskVector, vtBefore: Double): WorkloadKer {
        val vtBeforeClamped = if (vtBefore < 0.0) 0.0 else vtBefore
        val vt = risk.residual()
        val deltaVt = vt - vtBeforeClamped

        var maxR = risk.renergy
        if (risk.rhydraulic > maxR) maxR = risk.rhydraulic
        if (risk.runcertainty > maxR) maxR = risk.runcertainty

        var k = 0.95 - 0.4 * maxR
        if (deltaVt > 0.0) {
            k -= 0.25
        }
        if (k < 0.0) k = 0.0
        if (k > 1.0) k = 1.0

        var e = 0.95 - vt
        if (deltaVt > 0.0) {
            e -= 0.3
        }
        if (e < 0.0) e = 0.0
        if (e > 1.0) e = 1.0

        var r = vt
        if (deltaVt > 0.0) {
            r += deltaVt
        }
        if (r < 0.0) r = 0.0
        if (r > 1.0) r = 1.0

        return WorkloadKer(vt, deltaVt, k, e, r)
    }

    fun makeRow(
        nodeId: String,
        sampleId: String,
        timestampUtc: String,
        energyReqJ: Double,
        energySurplusJ: Double,
        hydraulicRisk: Double,
        uncertaintyRisk: Double,
        vtBefore: Double
    ): WorkloadSampleRow {
        val risk = WorkloadRiskVector.normalized(
            energyReqJ,
            energySurplusJ,
            hydraulicRisk,
            uncertaintyRisk
        )
        val ker = computeKer(risk, vtBefore)

        val yyyymmdd = "20260713"
        val domain = "workload_energy_dvt"
        val subtaskId = "PHX-CANAL-WL-2026-07-13"
        val phoenixHex = "0x5078585f574c5f32303236303731335f64445674"
        val priorPointer = "eco_restoration_shard/cyboquatic_progress/20260709/workload_energy_dvt_rust"

        val vtBeforeSafe = if (vtBefore < 0.0) 0.0 else vtBefore

        return WorkloadSampleRow(
            yyyymmdd,
            domain,
            subtaskId,
            nodeId,
            sampleId,
            timestampUtc,
            energyReqJ,
            energySurplusJ,
            hydraulicRisk,
            uncertaintyRisk,
            risk,
            vtBeforeSafe,
            ker.vt,
            ker.deltaVt,
            ker.k,
            ker.e,
            ker.r,
            phoenixHex,
            priorPointer
        )
    }

    fun insertRowSqlite(dbPath: String, row: WorkloadSampleRow) {
        Class.forName("org.sqlite.JDBC")
        val url = "jdbc:sqlite:$dbPath"
        DriverManager.getConnection(url).use { conn ->
            ensureSchema(conn)
            val sql = """
                INSERT INTO workload_daily_progress (
                  yyyymmdd, domain, subtask_id,
                  node_id, sample_id, timestamp_utc,
                  energy_req_j, energy_surplus_j,
                  hydraulic_risk, uncertainty_risk,
                  renergy, rhydraulic, runcertainty,
                  vt_before, vt_after, delta_vt,
                  k_factor, e_factor, r_factor,
                  phoenix_hex, prior_pointer
                ) VALUES (
                  ?, ?, ?,
                  ?, ?, ?,
                  ?, ?,
                  ?, ?,
                  ?, ?, ?,
                  ?, ?, ?,
                  ?, ?, ?,
                  ?, ?
                );
            """.trimIndent()
            conn.prepareStatement(sql).use { st: PreparedStatement ->
                var idx = 1
                st.setString(idx++, row.yyyymmdd)
                st.setString(idx++, row.domain)
                st.setString(idx++, row.subtaskId)

                st.setString(idx++, row.nodeId)
                st.setString(idx++, row.sampleId)
                st.setString(idx++, row.timestampUtc)

                st.setDouble(idx++, row.energyReqJ)
                st.setDouble(idx++, row.energySurplusJ)

                st.setDouble(idx++, row.hydraulicRisk)
                st.setDouble(idx++, row.uncertaintyRisk)

                st.setDouble(idx++, row.risk.renergy)
                st.setDouble(idx++, row.risk.rhydraulic)
                st.setDouble(idx++, row.risk.runcertainty)

                st.setDouble(idx++, row.vtBefore)
                st.setDouble(idx++, row.vtAfter)
                st.setDouble(idx++, row.deltaVt)

                st.setDouble(idx++, row.k)
                st.setDouble(idx++, row.e)
                st.setDouble(idx++, row.r)

                st.setString(idx++, row.phoenixHex)
                st.setString(idx++, row.priorPointer)

                st.executeUpdate()
            }
        }
    }

    private fun ensureSchema(conn: Connection) {
        val sql = """
            PRAGMA foreign_keys=ON;
            CREATE TABLE IF NOT EXISTS workload_daily_progress (
              progress_id       INTEGER PRIMARY KEY AUTOINCREMENT,
              yyyymmdd          TEXT    NOT NULL,
              domain            TEXT    NOT NULL,
              subtask_id        TEXT    NOT NULL,
              node_id           TEXT    NOT NULL,
              sample_id         TEXT    NOT NULL,
              timestamp_utc     TEXT    NOT NULL,
              energy_req_j      REAL    NOT NULL,
              energy_surplus_j  REAL    NOT NULL,
              hydraulic_risk    REAL    NOT NULL,
              uncertainty_risk  REAL    NOT NULL,
              renergy           REAL    NOT NULL,
              rhydraulic        REAL    NOT NULL,
              runcertainty      REAL    NOT NULL,
              vt_before         REAL    NOT NULL,
              vt_after          REAL    NOT NULL,
              delta_vt          REAL    NOT NULL,
              k_factor          REAL    NOT NULL,
              e_factor          REAL    NOT NULL,
              r_factor          REAL    NOT NULL,
              phoenix_hex       TEXT    NOT NULL,
              prior_pointer     TEXT    NOT NULL,
              created_at        TEXT    NOT NULL DEFAULT (datetime('now','localtime'))
            );
            CREATE INDEX IF NOT EXISTS idx_workload_daily_date
              ON workload_daily_progress(yyyymmdd);
            CREATE INDEX IF NOT EXISTS idx_workload_daily_node_time
              ON workload_daily_progress(node_id, timestamp_utc);
            CREATE INDEX IF NOT EXISTS idx_workload_daily_domain_subtask
              ON workload_daily_progress(domain, subtask_id);
        """.trimIndent()
        conn.createStatement().use { st ->
            st.executeUpdate(sql)
        }
    }
}
