import kotlin.math.exp
import kotlin.math.pow

data class BodObservation(
    val elapsedDays: Double,
    val temperatureC: Double,
    val bodMgL: Double,
    val flowM3PerS: Double
)

data class BodFit(
    val bod0MgL: Double,
    val k20PerDay: Double,
    val theta: Double,
    val sumSquaredError: Double,
    val effectiveRatesPerDay: List<Double>,
    val knowledgeFactor: Double,
    val ecoImpactValue: Double,
    val harmRisk: Double
)

fun predictBod(bod0MgL: Double, k20PerDay: Double, theta: Double, elapsedDays: Double, temperatureC: Double): Double {
    return bod0MgL * exp(-k20PerDay * theta.pow(temperatureC - 20.0) * elapsedDays)
}

fun fitTemperatureCorrectedBod(
    bod0MgL: Double,
    observations: List<BodObservation>,
    k20Minimum: Double,
    k20Maximum: Double,
    k20Steps: Int,
    thetaMinimum: Double,
    thetaMaximum: Double,
    thetaSteps: Int
): BodFit {
    require(bod0MgL > 0.0 && bod0MgL.isFinite()) { "bod0MgL must be positive and finite" }
    require(observations.size >= 3) { "at least three observations are required" }
    require(k20Minimum > 0.0 && k20Maximum >= k20Minimum && k20Steps >= 2) { "invalid k20 search bounds" }
    require(thetaMinimum > 0.0 && thetaMaximum >= thetaMinimum && thetaSteps >= 2) { "invalid theta search bounds" }

    observations.forEach {
        require(it.elapsedDays >= 0.0 && it.temperatureC in -5.0..60.0 && it.bodMgL >= 0.0 && it.flowM3PerS >= 0.0) {
            "observation values are outside accepted screening limits"
        }
    }

    val uniqueTemperatures = observations.map { it.temperatureC }.toSet().size
    var bestK20 = k20Minimum
    var bestTheta = thetaMinimum
    var bestSse = Double.POSITIVE_INFINITY

    for (kStep in 0 until k20Steps) {
        val k20 = k20Minimum + (k20Maximum - k20Minimum) * kStep / (k20Steps - 1).toDouble()
        for (thetaStep in 0 until thetaSteps) {
            val theta = thetaMinimum + (thetaMaximum - thetaMinimum) * thetaStep / (thetaSteps - 1).toDouble()
            val sse = observations.sumOf { observation ->
                val residual = observation.bodMgL - predictBod(
                    bod0MgL, k20, theta, observation.elapsedDays, observation.temperatureC
                )
                residual * residual
            }

            if (sse < bestSse) {
                bestSse = sse
                bestK20 = k20
                bestTheta = theta
            }
        }
    }

    val rates = observations.map { bestK20 * bestTheta.pow(it.temperatureC - 20.0) }
    val knowledgeFactor = when {
        uniqueTemperatures >= 3 && observations.size >= 6 -> 0.85
        uniqueTemperatures >= 2 && observations.size >= 4 -> 0.65
        else -> 0.35
    }
    val harmRisk = if (uniqueTemperatures < 2) 0.65 else 0.30
    val ecoImpactValue = knowledgeFactor * (1.0 - harmRisk)

    return BodFit(
        bod0MgL = bod0MgL,
        k20PerDay = bestK20,
        theta = bestTheta,
        sumSquaredError = bestSse,
        effectiveRatesPerDay = rates,
        knowledgeFactor = knowledgeFactor,
        ecoImpactValue = ecoImpactValue,
        harmRisk = harmRisk
    )
}

fun main(args: Array<String>) {
    require(args.size >= 15 && (args.size - 9) % 4 == 0) {
        "usage: TemperatureCorrectedBodFitKt <BOD0_mg_L> <k20_min> <k20_max> <k20_steps> " +
            "<theta_min> <theta_max> <theta_steps> <reserved_1> <reserved_2> " +
            "<time_days> <temperature_C> <BOD_mg_L> <flow_m3_s> [...]"
    }

    val bod0 = args[0].toDouble()
    val k20Min = args[1].toDouble()
    val k20Max = args[2].toDouble()
    val k20Steps = args[3].toInt()
    val thetaMin = args[4].toDouble()
    val thetaMax = args[5].toDouble()
    val thetaSteps = args[6].toInt()

    val observations = buildList {
        var index = 9
        while (index < args.size) {
            add(
                BodObservation(
                    elapsedDays = args[index].toDouble(),
                    temperatureC = args[index + 1].toDouble(),
                    bodMgL = args[index + 2].toDouble(),
                    flowM3PerS = args[index + 3].toDouble()
                )
            )
            index += 4
        }
    }

    val result = fitTemperatureCorrectedBod(
        bod0, observations, k20Min, k20Max, k20Steps, thetaMin, thetaMax, thetaSteps
    )

    println("BOD0_mg_L=${"%.8f".format(result.bod0MgL)}")
    println("k20_per_day=${"%.8f".format(result.k20PerDay)}")
    println("theta=${"%.8f".format(result.theta)}")
    println("sum_squared_error=${"%.8f".format(result.sumSquaredError)}")
    println("temperature_levels=${observations.map { it.temperatureC }.toSet().size}")
    result.effectiveRatesPerDay.forEachIndexed { index, rate ->
        println("effective_rate_per_day_sample_$index=${"%.8f".format(rate)}")
    }
    println("knowledge_factor=${"%.8f".format(result.knowledgeFactor)}")
    println("eco_impact_value=${"%.8f".format(result.ecoImpactValue)}")
    println("harm_risk=${"%.8f".format(result.harmRisk)}")
}
