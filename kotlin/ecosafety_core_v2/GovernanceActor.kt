// File: kotlin/ecosafety_core_v2/GovernanceActor.kt

package ecosafety_core_v2

import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
import java.sql.Connection
import java.sql.DriverManager
import java.time.Instant

// Event types for the actor.
sealed class GovernanceEvent {
    data class WindowEvaluated(
        val particleId: String,
        val priorHex: String?,
        val currentHex: String,
        val kPrev: Double,
        val ePrev: Double,
        val rPrev: Double,
        val vtPrev: Double,
        val kNext: Double,
        val eNext: Double,
        val rNext: Double,
        val vtNext: Double
    ) : GovernanceEvent()

    data class LanePromoted(
        val particleId: String,
        val lanePrev: String,
        val laneNext: String,
        val evidenceHex: String,
        val kNext: Double,
        val eNext: Double,
        val rNext: Double,
        val vtNext: Double
    ) : GovernanceEvent()
}

// Simple in-memory state for a governance particle.
data class GovernanceState(
    var lane: String,
    var k: Double,
    var e: Double,
    var r: Double,
    var vt: Double,
    var nextEventIndex: Int
)

// Governance particle coroutine actor.
class GovernanceActor(
    private val particleId: String,
    private val dbUrl: String
) {
    private val scope = CoroutineScope(Dispatchers.Default + SupervisorJob())
    private val channel = Channel<GovernanceEvent>(Channel.UNLIMITED)
    private var state = GovernanceState(
        lane = "RESEARCH",
        k = 0.0,
        e = 0.0,
        r = 0.0,
        vt = 0.0,
        nextEventIndex = 0
    )

    init {
        scope.launch {
            val conn = DriverManager.getConnection(dbUrl)
            try {
                // Replay existing journal for this particle to rebuild state.
                replayJournal(conn)
                // Process new events serially.
                for (event in channel) {
                    handleEvent(conn, event)
                }
            } finally {
                conn.close()
            }
        }
    }

    // Public API: send an event to the actor.
    suspend fun send(event: GovernanceEvent) {
        channel.send(event)
    }

    // Rebuild state from governance_journal.
    private fun replayJournal(conn: Connection) {
        val sql = """
            SELECT event_type, event_payload, event_index
            FROM governance_journal
            WHERE particle_id = ?
            ORDER BY event_index ASC
        """
        conn.prepareStatement(sql).use { ps ->
            ps.setString(1, particleId)
            ps.executeQuery().use { rs ->
                while (rs.next()) {
                    val type = rs.getString("event_type")
                    val payload = rs.getString("event_payload")
                    val idx = rs.getInt("event_index")
                    state.nextEventIndex = idx + 1
                    when (type) {
                        "WindowEvaluated" -> {
                            // Parse payload (e.g. JSON) and update state.
                            // For brevity, omitted here; assume state update.
                        }
                        "LanePromoted" -> {
                            // Parse payload and update lane and K,E,R,vt.
                        }
                    }
                }
            }
        }
    }

    // Handle a single event: update state and append to journal.
    private fun handleEvent(conn: Connection, event: GovernanceEvent) {
        when (event) {
            is GovernanceEvent.WindowEvaluated -> {
                // Apply ALN state machine logic (AlwaysImprove, etc.).
                // Here we simply overwrite with kNext,eNext,rNext,vtNext.
                state.k = event.kNext
                state.e = event.eNext
                state.r = event.rNext
                state.vt = event.vtNext

                appendJournalRow(conn, "WindowEvaluated", event)
            }
            is GovernanceEvent.LanePromoted -> {
                state.lane = event.laneNext
                state.k = event.kNext
                state.e = event.eNext
                state.r = event.rNext
                state.vt = event.vtNext

                appendJournalRow(conn, "LanePromoted", event)
            }
        }
    }

    private fun appendJournalRow(conn: Connection, type: String, event: GovernanceEvent) {
        val eventIndex = state.nextEventIndex++
        val payload = when (event) {
            is GovernanceEvent.WindowEvaluated -> {
                // Minimal JSON payload; in practice use a proper JSON library.
                """{"priorHex":"${event.priorHex ?: ""}","currentHex":"${event.currentHex}","kPrev":${event.kPrev},"ePrev":${event.ePrev},"rPrev":${event.rPrev},"vtPrev":${event.vtPrev},"kNext":${event.kNext},"eNext":${event.eNext},"rNext":${event.rNext},"vtNext":${event.vtNext}}"""
            }
            is GovernanceEvent.LanePromoted -> {
                """{"lanePrev":"${event.lanePrev}","laneNext":"${event.laneNext}","evidenceHex":"${event.evidenceHex}","kNext":${event.kNext},"eNext":${event.eNext},"rNext":${event.rNext},"vtNext":${event.vtNext}}"""
            }
        }
        val evidenceHex = when (event) {
            is GovernanceEvent.WindowEvaluated -> event.currentHex
            is GovernanceEvent.LanePromoted -> event.evidenceHex
        }
        val createdUtc = Instant.now().toString()

        val sql = """
            INSERT INTO governance_journal
              (particle_id, event_index, event_type, event_payload, evidence_hex, created_utc)
            VALUES (?, ?, ?, ?, ?, ?)
        """
        conn.prepareStatement(sql).use { ps ->
            ps.setString(1, particleId)
            ps.setInt(2, eventIndex)
            ps.setString(3, type)
            ps.setString(4, payload)
            ps.setString(5, evidenceHex)
            ps.setString(6, createdUtc)
            ps.executeUpdate()
        }
    }
}
