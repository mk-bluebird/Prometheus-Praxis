// Filename: src/main/kotlin/AlnParticleProcessor.kt

import kotlinx.coroutines.*
import kotlinx.coroutines.flow.*
import org.sqlite.SQLiteDataSource

data class AlnParticleUpdate(
    val particleId: String,
    val did: String,
    val k: Double,
    val e: Double,
    val r: Double,
    val kerScore: Double,
    val vt: Double,
    val lane: String,
    val evidenceHex: String,
    val timestampUtc: String
)

enum class GovernanceVerdictType { ACCEPT, REJECT, AUTOBAN, PROBATION }

data class GovernanceVerdict(
    val particleId: String,
    val verdict: GovernanceVerdictType,
    val reason: String,
    val timestampUtc: String
)

class SqliteParticleCache(dbPath: String) {
    private val ds = SQLiteDataSource().apply { url = "jdbc:sqlite:$dbPath" }

    fun loadPreviousState(particleId: String): Pair<Double?, Int> {
        ds.connection.use { conn ->
            conn.prepareStatement(
                """
                SELECT vt, high_r_count 
                FROM governance_particle_state 
                WHERE particle_id = ?
                """.trimIndent()
            ).use { ps ->
                ps.setString(1, particleId)
                ps.executeQuery().use { rs ->
                    return if (rs.next()) {
                        rs.getDouble("vt") to rs.getInt("high_r_count")
                    } else {
                        null to 0
                    }
                }
            }
        }
    }

    fun persistVerdict(update: AlnParticleUpdate, verdict: GovernanceVerdict, highRCount: Int) {
        ds.connection.use { conn ->
            conn.autoCommit = false
            try {
                // Upsert particle state.
                conn.prepareStatement(
                    """
                    INSERT INTO governance_particle_state (particle_id, vt, high_r_count, state)
                    VALUES (?, ?, ?, ?)
                    ON CONFLICT(particle_id) DO UPDATE SET
                        vt = excluded.vt,
                        high_r_count = excluded.high_r_count,
                        state = excluded.state
                    """.trimIndent()
                ).use { ps ->
                    ps.setString(1, update.particleId)
                    ps.setDouble(2, update.vt)
                    ps.setInt(3, highRCount)
                    val newState = when (verdict.verdict) {
                        GovernanceVerdictType.AUTOBAN -> "BANNED"
                        GovernanceVerdictType.PROBATION -> "PROBATION"
                        else -> "ACTIVE"
                    }
                    ps.setString(4, newState)
                    ps.executeUpdate()
                }

                // Insert verdict row.
                conn.prepareStatement(
                    """
                    INSERT INTO governance_verdicts 
                    (particle_id, verdict, reason, evidence_hex, timestamp_utc)
                    VALUES (?, ?, ?, ?, ?)
                    """.trimIndent()
                ).use { ps ->
                    ps.setString(1, verdict.particleId)
                    ps.setString(2, verdict.verdict.name)
                    ps.setString(3, verdict.reason)
                    ps.setString(4, update.evidenceHex)
                    ps.setString(5, verdict.timestampUtc)
                    ps.executeUpdate()
                }

                conn.commit()
            } catch (ex: Exception) {
                conn.rollback()
                throw ex
            }
        }
    }
}

fun mqttParticleFlow(): Flow<AlnParticleUpdate> {
    // Implementation-specific MQTT client, here represented as a placeholder.
    // It should use callbackFlow to emit AlnParticleUpdate objects.
    TODO("Implement MQTT client to emit AlnParticleUpdate")
}

fun governanceVerdictFlow(
    cache: SqliteParticleCache,
    highRiskThreshold: Double = 0.5,
    consecutiveHighRiskForAutoban: Int = 3
): Flow<GovernanceVerdict> {
    return mqttParticleFlow()
        .flatMapMerge(concurrency = 32) { update ->
            flow {
                val (prevVt, prevHighRCount) = cache.loadPreviousState(update.particleId)

                // Basic range checks.
                if (update.k !in 0.0..1.0 ||
                    update.e !in 0.0..1.0 ||
                    update.r !in 0.0..1.0 ||
                    update.kerScore !in 0.0..1.0
                ) {
                    val verdict = GovernanceVerdict(
                        particleId = update.particleId,
                        verdict = GovernanceVerdictType.REJECT,
                        reason = "Out-of-range K/E/R/kerScore",
                        timestampUtc = update.timestampUtc
                    )
                    cache.persistVerdict(update, verdict, prevHighRCount)
                    emit(verdict)
                    return@flow
                }

                // Check kerScore ≈ k * e - r (tolerant).
                val expectedS = update.k * update.e - update.r
                if (kotlin.math.abs(expectedS - update.kerScore) > 1e-6) {
                    val verdict = GovernanceVerdict(
                        particleId = update.particleId,
                        verdict = GovernanceVerdictType.REJECT,
                        reason = "Inconsistent kerScore with k*e-r",
                        timestampUtc = update.timestampUtc
                    )
                    cache.persistVerdict(update, verdict, prevHighRCount)
                    emit(verdict)
                    return@flow
                }

                // Lyapunov non-increase for production lane.
                if (update.lane == "PRODUCTION" && prevVt != null && update.vt > prevVt + 1e-9) {
                    val verdict = GovernanceVerdict(
                        particleId = update.particleId,
                        verdict = GovernanceVerdictType.REJECT,
                        reason = "Lyapunov residual increased in PRODUCTION lane",
                        timestampUtc = update.timestampUtc
                    )
                    cache.persistVerdict(update, verdict, prevHighRCount)
                    emit(verdict)
                    return@flow
                }

                // High-risk counter and autoban.
                val newHighRCount = if (update.r > highRiskThreshold) {
                    prevHighRCount + 1
                } else {
                    0
                }

                val verdictType = when {
                    newHighRCount >= consecutiveHighRiskForAutoban ->
                        GovernanceVerdictType.AUTOBAN
                    update.lane == "RESEARCH" || update.lane == "PILOT" ->
                        GovernanceVerdictType.PROBATION
                    else ->
                        GovernanceVerdictType.ACCEPT
                }

                val reason = when (verdictType) {
                    GovernanceVerdictType.AUTOBAN ->
                        "Risk r>${highRiskThreshold} for $newHighRCount consecutive events"
                    GovernanceVerdictType.PROBATION ->
                        "Non-production lane, particle under probation"
                    GovernanceVerdictType.ACCEPT ->
                        "Invariants satisfied; particle remains active"
                    GovernanceVerdictType.REJECT ->
                        "Rejected by invariants"
                }

                val verdict = GovernanceVerdict(
                    particleId = update.particleId,
                    verdict = verdictType,
                    reason = reason,
                    timestampUtc = update.timestampUtc
                )

                cache.persistVerdict(update, verdict, newHighRCount)
                emit(verdict)
            }.catch { ex ->
                // Emit a REJECT verdict on any processing failure without cancelling the stream.
                emit(
                    GovernanceVerdict(
                        particleId = update.particleId,
                        verdict = GovernanceVerdictType.REJECT,
                        reason = "Processing error: ${ex.message}",
                        timestampUtc = update.timestampUtc
                    )
                )
            }
        }
}

fun main() = runBlocking {
    val cache = SqliteParticleCache("aln_governance.db")
    governanceVerdictFlow(cache)
        .collect { verdict ->
            println("Particle ${verdict.particleId} verdict: ${verdict.verdict} (${verdict.reason})")
        }
}
