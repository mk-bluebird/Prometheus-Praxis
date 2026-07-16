package org.prometheuspraxis.eco

import java.io.File
import java.time.Instant
import java.time.format.DateTimeParseException

data class QuarantineRecord(
    val assetId: String,
    val timestamp: Instant,
    val lane: String,
    val ecoWealth: Double,
    val quarantineFlag: Boolean
)

data class QuarantineViolation(
    val assetId: String,
    val timestamp: Instant,
    val previousEcoWealth: Double,
    val currentEcoWealth: Double
)

object CsvUtil {
    fun readRecords(csvFile: File): List<QuarantineRecord> {
        val records = mutableListOf<QuarantineRecord>()
        csvFile.useLines { lines ->
            val iterator = lines.iterator()
            if (!iterator.hasNext()) return@useLines
            val header = iterator.next()
            val headerColumns = header.split(",").map { it.trim() }

            val idxAsset = headerColumns.indexOf("asset_id")
            val idxTs = headerColumns.indexOf("timestamp_utc")
            val idxLane = headerColumns.indexOf("lane")
            val idxEco = headerColumns.indexOf("eco_wealth")
            val idxQuar = headerColumns.indexOf("quarantine_flag")

            require(idxAsset >= 0 && idxTs >= 0 && idxLane >= 0 && idxEco >= 0 && idxQuar >= 0) {
                "CSV file must contain asset_id,timestamp_utc,lane,eco_wealth,quarantine_flag columns"
            }

            iterator.forEachRemaining { line ->
                if (line.isBlank()) return@forEachRemaining
                val cols = line.split(",")
                if (cols.size < headerColumns.size) return@forEachRemaining

                val assetId = cols[idxAsset].trim()
                val tsStr = cols[idxTs].trim()
                val lane = cols[idxLane].trim()
                val ecoStr = cols[idxEco].trim()
                val qStr = cols[idxQuar].trim()

                val ts = try {
                    Instant.parse(tsStr)
                } catch (e: DateTimeParseException) {
                    return@forEachRemaining
                }

                val eco = ecoStr.toDoubleOrNull() ?: return@forEachRemaining
                val qFlag = qStr == "1" || qStr.equals("true", ignoreCase = true)

                records.add(
                    QuarantineRecord(
                        assetId = assetId,
                        timestamp = ts,
                        lane = lane,
                        ecoWealth = eco,
                        quarantineFlag = qFlag
                    )
                )
            }
        }
        return records
    }
}

object QuarantinePolicy {

    /**
     * Verify that once an asset is quarantined, eco-wealth does not increase.
     * Returns a list of violations.
     */
    fun verifyInvariant(records: List<QuarantineRecord>): List<QuarantineViolation> {
        val byAsset = records.groupBy { it.assetId }
        val violations = mutableListOf<QuarantineViolation>()

        for ((assetId, recs) in byAsset) {
            val sorted = recs.sortedBy { it.timestamp }
            var inQuarantine = false
            var lastEco = 0.0

            for (rec in sorted) {
                val isQuarantineLane = rec.lane.equals("QUARANTINE", ignoreCase = true)
                val q = rec.quarantineFlag || isQuarantineLane

                if (!inQuarantine && q) {
                    inQuarantine = true
                    lastEco = rec.ecoWealth
                } else if (inQuarantine) {
                    if (rec.ecoWealth > lastEco + 1e-6) {
                        violations.add(
                            QuarantineViolation(
                                assetId = assetId,
                                timestamp = rec.timestamp,
                                previousEcoWealth = lastEco,
                                currentEcoWealth = rec.ecoWealth
                            )
                        )
                    }
                    if (rec.ecoWealth < lastEco) {
                        lastEco = rec.ecoWealth
                    }
                }
            }
        }

        return violations
    }
}

object GoldenFile {

    fun writeGolden(
        records: List<QuarantineRecord>,
        violations: List<QuarantineViolation>,
        outFile: File
    ) {
        val sb = StringBuilder()
        sb.append("{\"assets\":[")
        val byAsset = records.groupBy { it.assetId }
        var firstAsset = true
        for ((assetId, recs) in byAsset) {
            if (!firstAsset) {
                sb.append(",")
            }
            firstAsset = false
            sb.append("\n  {\"asset_id\":\"")
            sb.append(assetId)
            sb.append("\",\"timeline\":[")
            var firstRec = true
            for (r in recs.sortedBy { it.timestamp }) {
                if (!firstRec) sb.append(",")
                firstRec = false
                sb.append("{\"timestamp\":\"")
                sb.append(r.timestamp.toString())
                sb.append("\",\"lane\":\"")
                sb.append(r.lane)
                sb.append("\",\"eco_wealth\":")
                sb.append(r.ecoWealth)
                sb.append(",\"quarantine\":")
                sb.append(if (r.quarantineFlag) "true" else "false")
                sb.append("}")
            }
            sb.append("]}")
        }
        sb.append("],\"violations\":[")

        var firstV = true
        for (v in violations) {
            if (!firstV) sb.append(",")
            firstV = false
            sb.append("{\"asset_id\":\"")
            sb.append(v.assetId)
            sb.append("\",\"timestamp\":\"")
            sb.append(v.timestamp.toString())
            sb.append("\",\"prev\":")
            sb.append(v.previousEcoWealth)
            sb.append(",\"current\":")
            sb.append(v.currentEcoWealth)
            sb.append("}")
        }
        sb.append("]}")
        outFile.writeText(sb.toString())
    }
}

object QuarantineCli {

    @JvmStatic
    fun main(args: Array<String>) {
        if (args.size < 2) {
            System.err.println("Usage: quarantine-cli <input.csv> <golden.json>")
            return
        }

        val inputCsv = File(args[0])
        val goldenOut = File(args[1])

        if (!inputCsv.exists()) {
            System.err.println("Input CSV not found: ${inputCsv.absolutePath}")
            return
        }

        val records = CsvUtil.readRecords(inputCsv)
        val violations = QuarantinePolicy.verifyInvariant(records)

        if (violations.isEmpty()) {
            println("No quarantine invariant violations detected.")
        } else {
            println("Quarantine invariant violations:")
            for (v in violations) {
                println(
                    "Asset ${v.assetId} at ${v.timestamp}: " +
                    "eco_wealth increased from ${v.previousEcoWealth} to ${v.currentEcoWealth}"
                )
            }
        }

        GoldenFile.writeGolden(records, violations, goldenOut)
        println("Golden file written to: ${goldenOut.absolutePath}")
    }
}
