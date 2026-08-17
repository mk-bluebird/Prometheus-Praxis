data class ValidationSample(
    val foldId: Int,
    val predictedFog: Boolean,
    val referenceHazard: Boolean
)

data class ConfusionMatrix(
    val truePositive: Long,
    val falsePositive: Long,
    val falseNegative: Long,
    val trueNegative: Long
) {
    fun precision(): Double? {
        val denominator = truePositive + falsePositive
        return if (denominator == 0L) null else truePositive.toDouble() / denominator
    }

    fun recall(): Double? {
        val denominator = truePositive + falseNegative
        return if (denominator == 0L) null else truePositive.toDouble() / denominator
    }

    fun f1(): Double? {
        val denominator = 2L * truePositive + falsePositive + falseNegative
        return if (denominator == 0L) null else (2.0 * truePositive) / denominator
    }
}

fun metrics(samples: Iterable<ValidationSample>): ConfusionMatrix {
    var tp = 0L
    var fp = 0L
    var fn = 0L
    var tn = 0L

    for (sample in samples) {
        when {
            sample.predictedFog && sample.referenceHazard -> tp += 1
            sample.predictedFog && !sample.referenceHazard -> fp += 1
            !sample.predictedFog && sample.referenceHazard -> fn += 1
            else -> tn += 1
        }
    }

    return ConfusionMatrix(tp, fp, fn, tn)
}

fun main(args: Array<String>) {
    require(args.isNotEmpty() && args.size % 3 == 0) {
        "usage: FogCrossValidationKt <fold_id> <predicted_fog_0_or_1> <reference_hazard_0_or_1> [...]"
    }

    val samples = buildList {
        var index = 0
        while (index < args.size) {
            val predicted = args[index + 1].toInt()
            val reference = args[index + 2].toInt()
            require(predicted == 0 || predicted == 1) { "predicted_fog must be 0 or 1" }
            require(reference == 0 || reference == 1) { "reference_hazard must be 0 or 1" }

            add(
                ValidationSample(
                    foldId = args[index].toInt(),
                    predictedFog = predicted == 1,
                    referenceHazard = reference == 1
                )
            )
            index += 3
        }
    }

    val overall = metrics(samples)
    println("overall_tp=${overall.truePositive}")
    println("overall_fp=${overall.falsePositive}")
    println("overall_fn=${overall.falseNegative}")
    println("overall_tn=${overall.trueNegative}")
    println("overall_precision=${overall.precision()?.let { "%.8f".format(it) } ?: "UNDEFINED"}")
    println("overall_recall=${overall.recall()?.let { "%.8f".format(it) } ?: "UNDEFINED"}")
    println("overall_f1=${overall.f1()?.let { "%.8f".format(it) } ?: "UNDEFINED"}")

    samples.groupBy { it.foldId }
        .toSortedMap()
        .forEach { (foldId, foldSamples) ->
            val fold = metrics(foldSamples)
            println(
                "fold=$foldId tp=${fold.truePositive} fp=${fold.falsePositive} fn=${fold.falseNegative} " +
                    "tn=${fold.trueNegative} f1=${fold.f1()?.let { "%.8f".format(it) } ?: "UNDEFINED"}"
            )
        }
}
