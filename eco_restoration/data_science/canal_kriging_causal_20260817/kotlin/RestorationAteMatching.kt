import kotlin.math.abs
import kotlin.math.sqrt

data class InterventionRecord(
    val id: String,
    val treated: Boolean,
    val stratum: String,
    val covariates: List<Double>,
    val outcome: Double
)

data class Match(
    val treatedId: String,
    val controlId: String,
    val treatmentEffect: Double,
    val distance: Double
)

data class MatchingResult(
    val matchedAverageTreatmentEffectOnTreated: Double?,
    val matchedCount: Int,
    val unmatchedTreatedCount: Int,
    val maximumMatchedDistance: Double,
    val status: String,
    val matches: List<Match>
)

fun standardizedDistance(left: List<Double>, right: List<Double>, scales: List<Double>): Double {
    require(left.size == right.size && left.size == scales.size) { "covariate dimensions must match" }
    var sum = 0.0
    for (index in left.indices) {
        require(scales[index] > 0.0 && scales[index].isFinite()) { "covariate scales must be finite and positive" }
        val standardizedDifference = (left[index] - right[index]) / scales[index]
        sum += standardizedDifference * standardizedDifference
    }
    return sqrt(sum)
}

fun matchEstimate(
    records: List<InterventionRecord>,
    covariateScales: List<Double>,
    caliper: Double
): MatchingResult {
    require(records.isNotEmpty()) { "records must not be empty" }
    require(caliper >= 0.0 && caliper.isFinite()) { "caliper must be finite and non-negative" }
    require(records.map { it.id }.toSet().size == records.size) { "record IDs must be unique" }

    records.forEach {
        require(it.stratum.isNotBlank()) { "stratum must be non-empty" }
        require(it.covariates.size == covariateScales.size) { "covariate dimensions must match scales" }
        require(it.covariates.all { value -> value.isFinite() } && it.outcome.isFinite()) {
            "covariates and outcome must be finite"
        }
    }

    val treated = records.filter { it.treated }.sortedBy { it.id }
    val availableControls = records.filterNot { it.treated }.associateBy { it.id }.toMutableMap()
    val matches = mutableListOf<Match>()
    var unmatched = 0

    for (treatedRecord in treated) {
        val candidate = availableControls.values
            .asSequence()
            .filter { it.stratum == treatedRecord.stratum }
            .map { control ->
                control to standardizedDistance(
                    treatedRecord.covariates,
                    control.covariates,
                    covariateScales
                )
            }
            .filter { (_, distance) -> distance <= caliper }
            .sortedWith(compareBy<Pair<InterventionRecord, Double>> { it.second }.thenBy { it.first.id })
            .firstOrNull()

        if (candidate == null) {
            unmatched += 1
        } else {
            val (control, distance) = candidate
            matches += Match(
                treatedId = treatedRecord.id,
                controlId = control.id,
                treatmentEffect = treatedRecord.outcome - control.outcome,
                distance = distance
            )
            availableControls.remove(control.id)
        }
    }

    val effect = matches.takeIf { it.isNotEmpty() }?.map { it.treatmentEffect }?.average()
    val maxDistance = matches.maxOfOrNull { it.distance } ?: 0.0
    val status = when {
        matches.isEmpty() -> "NO_MATCHED_COMPARISON"
        unmatched > 0 -> "PARTIAL_MATCHED_ATT_ESTIMATE"
        else -> "MATCHED_ATT_ESTIMATE"
    }

    return MatchingResult(effect, matches.size, unmatched, maxDistance, status, matches)
}

fun main(args: Array<String>) {
    require(args.size >= 8) {
        "usage: RestorationAteMatchingKt <caliper> <scale_1> [<scale_n> ...] -- " +
            "<id> <treated_0_or_1> <stratum> <outcome> <covariate_1> [<covariate_n> ...] ..."
    }

    val separator = args.indexOf("--")
    require(separator >= 3) { "use -- to separate scales from records" }

    val caliper = args[0].toDouble()
    val scales = args.slice(1 until separator).map { it.toDouble() }
    val width = 4 + scales.size
    require((args.size - separator - 1) >= width && (args.size - separator - 1) % width == 0) {
        "each record must provide id treated stratum outcome and all covariates"
    }

    val records = buildList {
        var index = separator + 1
        while (index < args.size) {
            val treatedFlag = args[index + 1].toInt()
            require(treatedFlag == 0 || treatedFlag == 1) { "treated flag must be 0 or 1" }
            add(
                InterventionRecord(
                    id = args[index],
                    treated = treatedFlag == 1,
                    stratum = args[index + 2],
                    outcome = args[index + 3].toDouble(),
                    covariates = args.slice(index + 4 until index + 4 + scales.size).map { it.toDouble() }
                )
            )
            index += width
        }
    }

    val result = matchEstimate(records, scales, caliper)
    println("status=${result.status}")
    println("matched_count=${result.matchedCount}")
    println("unmatched_treated_count=${result.unmatchedTreatedCount}")
    println("maximum_matched_distance=${"%.10f".format(result.maximumMatchedDistance)}")
    println(
        "matched_ATT=${result.matchedAverageTreatmentEffectOnTreated?.let { "%.10f".format(it) } ?: "UNAVAILABLE"}"
    )
    result.matches.forEach {
        println(
            "match treated=${it.treatedId} control=${it.controlId} " +
                "effect=${"%.10f".format(it.treatmentEffect)} distance=${"%.10f".format(it.distance)}"
        )
    }
}
