import kotlin.math.max
import kotlin.math.min

data class ExposurePoint(val timeHours: Double, val concentrationMgL: Double)
data class GrowthResult(
    val integratedGrowth: Double,
    val controlGrowth: Double,
    val inhibition: Double,
    val monotonicDecreasing: Boolean
)

fun clamp(value: Double, low: Double, high: Double): Double = max(low, min(high, value))

fun concentrationAt(points: List<ExposurePoint>, timeHours: Double): Double {
    if (timeHours <= points.first().timeHours) return points.first().concentrationMgL

    for (index in 1 until points.size) {
        val left = points[index - 1]
        val right = points[index]
        if (timeHours <= right.timeHours) {
            val fraction = (timeHours - left.timeHours) / (right.timeHours - left.timeHours)
            return left.concentrationMgL + fraction * (right.concentrationMgL - left.concentrationMgL)
        }
    }
    return points.last().concentrationMgL
}

fun growthRate(
    concentrationMgL: Double,
    muMaxPerHour: Double,
    substrateMgL: Double,
    ksMgL: Double,
    ec50MgL: Double
): Double {
    val substrateTerm = substrateMgL / (ksMgL + substrateMgL)
    val toxicityTerm = clamp(1.0 - concentrationMgL / ec50MgL, 0.0, 1.0)
    return muMaxPerHour * substrateTerm * toxicityTerm
}

fun integrateGrowth(
    rawPoints: List<ExposurePoint>,
    muMaxPerHour: Double,
    substrateMgL: Double,
    ksMgL: Double,
    ec50MgL: Double,
    stepHours: Double
): GrowthResult {
    require(rawPoints.size >= 2) { "at least two exposure points are required" }
    require(muMaxPerHour >= 0.0 && substrateMgL >= 0.0 && ksMgL >= 0.0 && ec50MgL > 0.0 && stepHours > 0.0) {
        "invalid kinetic parameters"
    }

    val points = rawPoints.sortedBy { it.timeHours }
    var monotonicDecreasing = true
    for (index in 1 until points.size) {
        require(points[index].timeHours > points[index - 1].timeHours) { "time values must strictly increase" }
        require(points[index].concentrationMgL >= 0.0) { "concentrations must be non-negative" }
        monotonicDecreasing = monotonicDecreasing &&
            points[index].concentrationMgL <= points[index - 1].concentrationMgL
    }

    val start = points.first().timeHours
    val end = points.last().timeHours
    var current = start
    var integral = 0.0

    while (current < end) {
        val next = min(end, current + stepHours)
        val leftRate = growthRate(concentrationAt(points, current), muMaxPerHour, substrateMgL, ksMgL, ec50MgL)
        val rightRate = growthRate(concentrationAt(points, next), muMaxPerHour, substrateMgL, ksMgL, ec50MgL)
        integral += 0.5 * (leftRate + rightRate) * (next - current)
        current = next
    }

    val controlRate = muMaxPerHour * substrateMgL / (ksMgL + substrateMgL)
    val controlGrowth = controlRate * (end - start)
    val inhibition = if (controlGrowth <= 0.0) 0.0 else clamp(1.0 - integral / controlGrowth, 0.0, 1.0)

    return GrowthResult(integral, controlGrowth, inhibition, monotonicDecreasing)
}

fun main(args: Array<String>) {
    require(args.size >= 9 && (args.size - 5) % 2 == 0) {
        "usage: TimeVaryingAlgalGrowthKt <muMax_per_h> <S_mg_L> <Ks_mg_L> <EC50_mg_L> <step_h> " +
            "<time_h> <C_mg_L> [<time_h> <C_mg_L> ...]"
    }

    val muMax = args[0].toDouble()
    val substrate = args[1].toDouble()
    val ks = args[2].toDouble()
    val ec50 = args[3].toDouble()
    val step = args[4].toDouble()

    val points = buildList {
        var index = 5
        while (index < args.size) {
            add(ExposurePoint(args[index].toDouble(), args[index + 1].toDouble()))
            index += 2
        }
    }

    val result = integrateGrowth(points, muMax, substrate, ks, ec50, step)
    println("integrated_growth=%.8f".format(result.integratedGrowth))
    println("control_growth=%.8f".format(result.controlGrowth))
    println("growth_inhibition=%.8f".format(result.inhibition))
    println("exposure_monotonic_decreasing=${result.monotonicDecreasing}")
    println(
        "ec50_interpretation=" +
            if (result.monotonicDecreasing) "TRAJECTORY_DEPENDENT_NOT_CONSTANT_EXPOSURE_EC50"
            else "NONMONOTONIC_TRAJECTORY_NOT_CONSTANT_EXPOSURE_EC50"
    )
}
