import kotlin.math.abs
import kotlin.math.max
import kotlin.math.roundToLong
import kotlin.math.sqrt

data class SpatialObservation(
    val xMeters: Double,
    val yMeters: Double,
    val value: Double
)

data class ResolutionResult(
    val sideLengthMeters: Double,
    val rmse: Double,
    val sparseFraction: Double,
    val objective: Double,
    val occupiedBins: Int,
    val status: String
)

private data class Axial(val q: Long, val r: Long)

private fun roundHalfAwayFromZero(value: Double): Long {
    return if (value >= 0.0) kotlin.math.floor(value + 0.5).toLong()
    else kotlin.math.ceil(value - 0.5).toLong()
}

private fun axialFor(xMeters: Double, yMeters: Double, sideLengthMeters: Double): Axial {
    val qFloat = (sqrt(3.0) / 3.0 * xMeters - yMeters / 3.0) / sideLengthMeters
    val rFloat = (2.0 / 3.0 * yMeters) / sideLengthMeters
    var q = roundHalfAwayFromZero(qFloat)
    var r = roundHalfAwayFromZero(rFloat)
    var s = roundHalfAwayFromZero(-qFloat - rFloat)

    val qError = abs(q - qFloat)
    val rError = abs(r - rFloat)
    val sError = abs(s + qFloat + rFloat)

    if (qError > rError && qError > sError) {
        q = -r - s
    } else if (rError > sError) {
        r = -q - s
    }

    return Axial(q, r)
}

fun selectResolution(
    observations: List<SpatialObservation>,
    candidatesMeters: List<Double>,
    minimumSamplesPerBin: Int,
    sparsePenaltyWeight: Double
): List<ResolutionResult> {
    require(observations.size >= 3) { "at least three observations are required" }
    require(candidatesMeters.isNotEmpty()) { "at least one candidate size is required" }
    require(minimumSamplesPerBin >= 2) { "minimumSamplesPerBin must be at least 2" }
    require(sparsePenaltyWeight >= 0.0) { "sparsePenaltyWeight must be non-negative" }

    observations.forEach {
        require(it.xMeters.isFinite() && it.yMeters.isFinite() && it.value.isFinite()) {
            "observation values must be finite"
        }
    }

    return candidatesMeters.sorted().map { sideLength ->
        require(sideLength > 0.0 && sideLength.isFinite()) { "candidate side length must be finite and positive" }

        val bins = observations.groupBy { axialFor(it.xMeters, it.yMeters, sideLength) }
        var squaredError = 0.0
        var evaluated = 0
        var sparseBins = 0

        for ((_, members) in bins) {
            if (members.size < minimumSamplesPerBin) {
                sparseBins += 1
                continue
            }

            val sum = members.sumOf { it.value }
            for (member in members) {
                val leaveOneOutMean = (sum - member.value) / (members.size - 1).toDouble()
                val residual = member.value - leaveOneOutMean
                squaredError += residual * residual
                evaluated += 1
            }
        }

        val rmse = if (evaluated == 0) Double.POSITIVE_INFINITY else sqrt(squaredError / evaluated)
        val sparseFraction = sparseBins.toDouble() / max(1, bins.size).toDouble()
        val objective = rmse + sparsePenaltyWeight * sparseFraction
        val status = if (evaluated == 0) "INSUFFICIENT_DENSITY" else "EVALUATED"

        ResolutionResult(
            sideLengthMeters = sideLength,
            rmse = rmse,
            sparseFraction = sparseFraction,
            objective = objective,
            occupiedBins = bins.size,
            status = status
        )
    }
}

fun main(args: Array<String>) {
    require(args.size >= 8 && (args.size - 3) % 3 == 0) {
        "usage: HexBinResolutionSelectorKt <minimum_samples_per_bin> <sparse_penalty_weight> " +
            "<candidate_side_m> [<candidate_side_m> ...] -- <x_m> <y_m> <value> [...]"
    }

    val separator = args.indexOf("--")
    require(separator >= 3) { "use -- to separate candidate sizes from observations" }
    require((args.size - separator - 1) >= 3 && (args.size - separator - 1) % 3 == 0) {
        "observations must be x_m y_m value triples"
    }

    val minimumSamples = args[0].toInt()
    val penalty = args[1].toDouble()
    val candidates = args.slice(2 until separator).map { it.toDouble() }
    val observations = buildList {
        var index = separator + 1
        while (index < args.size) {
            add(SpatialObservation(args[index].toDouble(), args[index + 1].toDouble(), args[index + 2].toDouble()))
            index += 3
        }
    }

    val results = selectResolution(observations, candidates, minimumSamples, penalty)
    results.forEach {
        println(
            "side_m=%.8f rmse=%.8f sparse_fraction=%.8f objective=%.8f occupied_bins=%d status=%s"
                .format(
                    it.sideLengthMeters,
                    it.rmse,
                    it.sparseFraction,
                    it.objective,
                    it.occupiedBins,
                    it.status
                )
        )
    }

    val best = results
        .filter { it.status == "EVALUATED" }
        .minWithOrNull(compareBy<ResolutionResult> { it.objective }.thenBy { it.sideLengthMeters })

    println(
        if (best == null) "selected_side_m=NONE_INSUFFICIENT_DENSITY"
        else "selected_side_m=%.8f".format(best.sideLengthMeters)
    )
}
