// File: bee-corridor-evidence/src/main/kotlin/org/prometheuspraxis/beecorridor/BeeCorridorEvidenceBundleLib.kt
// Destination: Prometheus-Praxis/bee-corridor-evidence/src/main/kotlin/org/prometheuspraxis/beecorridor/BeeCorridorEvidenceBundleLib.kt
// License: MIT OR Apache-2.0

package org.prometheuspraxis.beecorridor

import java.io.BufferedReader
import java.io.InputStream
import java.io.InputStreamReader
import java.time.Instant
import java.util.Properties
import org.apache.kafka.clients.producer.KafkaProducer
import org.apache.kafka.clients.producer.ProducerRecord
import org.apache.kafka.clients.producer.RecordMetadata

/**
 * Data model for BeeCorridorEvidenceBundle as described by eco.beecorridor.evidencebundle.v1.aln.
 *
 * Expected CSV header (example):
 *   corridor_id,timestamp_utc,region,metric_type,metric_value,evidence_source,steward_did
 */
data class BeeCorridorEvidenceBundle(
    val corridorId: String,
    val timestampUtc: String,
    val region: String,
    val metricType: String,
    val metricValue: Double,
    val evidenceSource: String,
    val stewardDid: String
)

/**
 * Validation errors when checking against eco.beecorridor.evidencebundle.v1.aln.
 */
class EvidenceValidationException(message: String) : Exception(message)

/**
 * ALN validation logic for BeeCorridorEvidenceBundle.
 *
 * This follows the eco.beecorridor.evidencebundle.v1.aln constraints:
 *  - corridorId must be non-empty and reference a known corridor namespace.
 *  - timestampUtc must be a valid ISO-8601 string.
 *  - metricType must be one of allowed evidence metric categories.
 *  - metricValue must be non-negative.
 *  - stewardDid must be a valid DID (bostrom/aln/zeta/0x patterns).
 */
object BeeCorridorEvidenceValidator {

    private val allowedMetricTypes = setOf(
        "flower_density_index",
        "pollinator_count",
        "pesticide_residue_ppb",
        "nectar_quality_index",
        "corridor_width_m",
        "corridor_maintenance_event"
    )

    fun validate(bundle: BeeCorridorEvidenceBundle) {
        if (bundle.corridorId.isBlank()) {
            throw EvidenceValidationException("corridor_id must not be blank")
        }
        if (!bundle.corridorId.startsWith("bee_corridor_")) {
            throw EvidenceValidationException("corridor_id must start with 'bee_corridor_' namespace")
        }

        try {
            Instant.parse(bundle.timestampUtc)
        } catch (ex: Exception) {
            throw EvidenceValidationException("timestamp_utc must be ISO-8601; found '${bundle.timestampUtc}'")
        }

        if (!allowedMetricTypes.contains(bundle.metricType)) {
            throw EvidenceValidationException("metric_type '${bundle.metricType}' is not allowed by eco.beecorridor.evidencebundle.v1.aln")
        }

        if (bundle.metricValue < 0.0) {
            throw EvidenceValidationException("metric_value must be non-negative")
        }

        if (!validateDidFormat(bundle.stewardDid)) {
            throw EvidenceValidationException("steward_did '${bundle.stewardDid}' is not a valid DID for eco.beecorridor.evidencebundle.v1.aln")
        }
    }

    private fun validateDidFormat(did: String): Boolean {
        if (did.startsWith("bostrom") || did.startsWith("aln")) {
            return did.length in 40..80
        }
        if (did.startsWith("0x")) {
            return did.length == 42
        }
        if (did.startsWith("zeta")) {
            return did.length >= 40
        }
        return false
    }
}

/**
 * Bridge to Rust/ALN signing enclave.
 *
 * Native library must provide:
 *
 *   JNIEXPORT jstring JNICALL
 *   Java_org_prometheuspraxis_beecorridor_EvidenceSigningBridge_signEvidenceBundle(
 *       JNIEnv* env,
 *       jclass,
 *       jstring j_did,
 *       jstring j_payload
 *   );
 *
 * The returned string is a hex-encoded signature (evidence_hex).
 */
object EvidenceSigningBridge {

    init {
        // Native library name must match build output, e.g., libeco_bee_sign_enclave.so
        System.loadLibrary("eco_bee_sign_enclave")
    }

    @JvmStatic
    external fun signEvidenceBundle(did: String, payload: String): String
}

/**
 * Simple Kafka producer wrapper that publishes signed evidence messages.
 *
 * Records are published as JSON blobs containing the bundle fields and evidence_hex.
 */
class BeeCorridorEvidencePublisher(
    bootstrapServers: String,
    private val topic: String
) : AutoCloseable {

    private val producer: KafkaProducer<String, String>

    init {
        val props = Properties()
        props["bootstrap.servers"] = bootstrapServers
        props["key.serializer"] = "org.apache.kafka.common.serialization.StringSerializer"
        props["value.serializer"] = "org.apache.kafka.common.serialization.StringSerializer"
        props["acks"] = "all"
        props["enable.idempotence"] = "true"
        producer = KafkaProducer<String, String>(props)
    }

    fun publish(bundle: BeeCorridorEvidenceBundle, evidenceHex: String): RecordMetadata {
        val key = bundle.corridorId
        val value = buildEvidenceJson(bundle, evidenceHex)

        val record = ProducerRecord<String, String>(topic, key, value)
        val future = producer.send(record)
        return future.get()
    }

    private fun buildEvidenceJson(bundle: BeeCorridorEvidenceBundle, evidenceHex: String): String {
        // Small hand-rolled JSON to avoid extra dependencies.
        return buildString {
            append("{")
            append("\"corridor_id\":\"").append(escapeJson(bundle.corridorId)).append("\",")
            append("\"timestamp_utc\":\"").append(escapeJson(bundle.timestampUtc)).append("\",")
            append("\"region\":\"").append(escapeJson(bundle.region)).append("\",")
            append("\"metric_type\":\"").append(escapeJson(bundle.metricType)).append("\",")
            append("\"metric_value\":").append(bundle.metricValue).append(",")
            append("\"evidence_source\":\"").append(escapeJson(bundle.evidenceSource)).append("\",")
            append("\"steward_did\":\"").append(escapeJson(bundle.stewardDid)).append("\",")
            append("\"evidence_hex\":\"").append(escapeJson(evidenceHex)).append("\"")
            append("}")
        }
    }

    private fun escapeJson(value: String): String {
        return value
            .replace("\\", "\\\\")
            .replace("\"", "\\\"")
            .replace("\n", "\\n")
            .replace("\r", "\\r")
    }

    override fun close() {
        producer.close()
    }
}

/**
 * CSV parsing utilities for BeeCorridorEvidenceBundle.
 *
 * This assumes a header row and comma-separated values, with no embedded commas.
 * For more complex CSV files, a dedicated CSV parser can be wired in.
 */
object BeeCorridorEvidenceCsvParser {

    fun parseStream(input: InputStream): Sequence<BeeCorridorEvidenceBundle> {
        val reader = BufferedReader(InputStreamReader(input, Charsets.UTF_8))
        return sequence {
            val headerLine = reader.readLine() ?: return@sequence
            val header = headerLine.split(",").map { it.trim() }

            val expected = listOf(
                "corridor_id",
                "timestamp_utc",
                "region",
                "metric_type",
                "metric_value",
                "evidence_source",
                "steward_did"
            )

            if (header != expected) {
                throw IllegalArgumentException("Unexpected CSV header. Expected $expected but found $header")
            }

            var line: String? = reader.readLine()
            while (line != null) {
                if (line.isBlank()) {
                    line = reader.readLine()
                    continue
                }
                val columns = line.split(",")
                if (columns.size < expected.size) {
                    throw IllegalArgumentException("Invalid CSV row (expected ${expected.size} columns): $line")
                }

                val bundle = BeeCorridorEvidenceBundle(
                    corridorId = columns[0].trim(),
                    timestampUtc = columns[1].trim(),
                    region = columns[2].trim(),
                    metricType = columns[3].trim(),
                    metricValue = columns[4].trim().toDoubleOrNull()
                        ?: throw IllegalArgumentException("metric_value must be numeric in row: $line"),
                    evidenceSource = columns[5].trim(),
                    stewardDid = columns[6].trim()
                )

                yield(bundle)

                line = reader.readLine()
            }
        }
    }
}

/**
 * High-level orchestrator: parse, validate, sign, and publish.
 *
 * This function can be called from an IoT ingestion pipeline or a CLI wrapper.
 */
class BeeCorridorEvidenceIngestor(
    bootstrapServers: String,
    topic: String
) {

    private val publisher = BeeCorridorEvidencePublisher(bootstrapServers, topic)

    fun ingestCsvStream(input: InputStream) {
        val bundles = BeeCorridorEvidenceCsvParser.parseStream(input)
        for (bundle in bundles) {
            BeeCorridorEvidenceValidator.validate(bundle)

            val payload = buildSigningPayload(bundle)
            val evidenceHex = EvidenceSigningBridge.signEvidenceBundle(bundle.stewardDid, payload)

            publisher.publish(bundle, evidenceHex)
        }
    }

    private fun buildSigningPayload(bundle: BeeCorridorEvidenceBundle): String {
        // Deterministic payload concatenation for signing.
        return buildString {
            append("BeeCorridorEvidenceBundle|")
            append("corridor_id=").append(bundle.corridorId).append("|")
            append("timestamp_utc=").append(bundle.timestampUtc).append("|")
            append("region=").append(bundle.region).append("|")
            append("metric_type=").append(bundle.metricType).append("|")
            append("metric_value=").append(bundle.metricValue).append("|")
            append("evidence_source=").append(bundle.evidenceSource).append("|")
            append("steward_did=").append(bundle.stewardDid)
        }
    }
}
