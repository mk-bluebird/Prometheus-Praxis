import kotlin.math.round

data class RadiusResult(val zone: String, val scaledRadius: Long)

fun parseFinite(text: String, label: String): Double {
    val value = text.toDouble()
    require(value.isFinite()) { "$label must be finite" }
    return value
}

fun classify(q: Double, t: Double, sensitivity: Double, distance: Double, coefficient: Double): RadiusResult {
    require(q > 0.0 && t > 0.0 && coefficient > 0.0) { "Q, T, and c must be positive" }
    require(sensitivity in 0.0..1.0 && distance >= 0.0) { "S_b must be in [0, 1] and distance must be non-negative" }

    val radius = coefficient * kotlin.math.sqrt(q * t) * (1.0 + 1.5 * sensitivity)
    require(radius.isFinite() && radius >= 0.0) { "conservative radius must be finite" }

    val zone = when {
        distance > radius -> "SAFE"
        distance > radius / 2.0 -> "CAUTION"
        else -> "EXCLUDE"
    }

    return RadiusResult(zone, round(radius * 1_000_000.0).toLong())
}

fun main(args: Array<String>) {
    require(args.size == 5) {
        "usage: BlastRadiusConformanceKt <Q_m3_s> <T_s> <S_b_0_to_1> <distance_m> <c>"
    }

    val result = classify(
        parseFinite(args[0], "Q"),
        parseFinite(args[1], "T"),
        parseFinite(args[2], "S_b"),
        parseFinite(args[3], "distance"),
        parseFinite(args[4], "c")
    )

    println("${result.zone}|${result.scaledRadius}")
}
