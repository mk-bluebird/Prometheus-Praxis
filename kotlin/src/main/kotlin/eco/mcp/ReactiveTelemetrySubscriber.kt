// File: kotlin/src/main/kotlin/eco/mcp/ReactiveTelemetrySubscriber.kt
package eco.mcp

import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStreamWriter

/**
 * Kotlin Coroutine-Based MCP Stream Subscriber
 *
 * Opens a persistent stdio-based connection to the C++ MCP server (toolbox/bridge),
 * listens for telemetry push events (simulated as JSON lines), and reacts
 * to new events in a reactive fashion.
 *
 * The C++ side can be extended to run in "stream" mode and emit JSON objects
 * whenever new telemetry is available. Here we assume each line is a JSON record
 * with fields like hexId, vResidual, carbonIntensity, kerS.
 */

data class TelemetryEvent(
    val hexId: String,
    val vResidual: Double,
    val carbonIntensity: Double,
    val kerS: Double
)

class ReactiveTelemetrySubscriber(
    private val cppBinaryPath: String = "./universal_mcp_governance_toolbox"
) {

    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    private val eventChannel = Channel<TelemetryEvent>(Channel.UNLIMITED)

    /**
     * Start the MCP subscriber: spawn the C++ binary in "stream" mode
     * and route each JSON line into the event channel.
     */
    fun start() {
        scope.launch {
            val process = ProcessBuilder(cppBinaryPath, "--stream-telemetry")
                .redirectErrorStream(true)
                .start()

            // Optionally send an initial command to enable stream mode.
            OutputStreamWriter(process.outputStream).use { writer ->
                writer.write("stream_telemetry\n")
                writer.flush()
            }

            BufferedReader(InputStreamReader(process.inputStream)).use { reader ->
                var line = reader.readLine()
                while (line != null) {
                    val event = parseTelemetryEvent(line)
                    if (event != null) {
                        eventChannel.send(event)
                    }
                    line = reader.readLine()
                }
            }

            process.waitFor()
            eventChannel.close()
        }

        // Consumer: react to events (e.g., log, update UI, trigger governance actions).
        scope.launch {
            for (event in eventChannel) {
                handleEvent(event)
            }
        }
    }

    fun stop() {
        scope.cancel("Stopping MCP stream subscriber")
    }

    private fun parseTelemetryEvent(line: String): TelemetryEvent? {
        // Expect JSON like:
        // { "hexId": "hex_PHX_001", "vResidual": 0.83, "carbonIntensity": 0.41, "kerS": 0.34 }
        if (!line.contains("\"hexId\"")) return null

        fun extractString(field: String): String {
            val key = "\"$field\""
            val idx = line.indexOf(key)
            if (idx == -1) return ""
            val colon = line.indexOf(':', idx)
            val firstQuote = line.indexOf('"', colon + 1)
            val secondQuote = line.indexOf('"', firstQuote + 1)
            if (firstQuote == -1 || secondQuote == -1) return ""
            return line.substring(firstQuote + 1, secondQuote)
        }

        fun extractDouble(field: String): Double {
            val key = "\"$field\""
            val idx = line.indexOf(key)
            if (idx == -1) return 0.0
            val colon = line.indexOf(':', idx)
            if (colon == -1) return 0.0
            val end = line.indexOf(',', colon + 1).let { if (it == -1) line.length else it }
            val raw = line.substring(colon + 1, end).trim().trimEnd('}', ' ')
            return raw.toDoubleOrNull() ?: 0.0
        }

        val hexId = extractString("hexId")
        val vResidual = extractDouble("vResidual")
        val carbonIntensity = extractDouble("carbonIntensity")
        val kerS = extractDouble("kerS")

        if (hexId.isEmpty()) return null
        return TelemetryEvent(hexId, vResidual, carbonIntensity, kerS)
    }

    private fun handleEvent(event: TelemetryEvent) {
        // Simple reactive handling: print and apply basic corridor logic.
        val safe = event.vResidual <= 1.0 && (1.0 - event.carbonIntensity) >= 0.2
        println(
            "TelemetryEvent: hex=${event.hexId} V=${event.vResidual} CI=${event.carbonIntensity} s=${event.kerS} " +
                "safe=${safe}"
        )
        // In a full system, this could trigger UI updates, logging, or downstream scheduling.
    }
}

// Example runnable entrypoint.
fun main() {
    val subscriber = ReactiveTelemetrySubscriber("./universal_mcp_governance_toolbox")
    subscriber.start()

    Runtime.getRuntime().addShutdownHook(Thread {
        subscriber.stop()
    })

    // Keep JVM alive; in real usage, this would be a long-lived service.
    Thread.sleep(60_000L)
}
