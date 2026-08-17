data class FogThresholds(
    val oilMgL: Double,
    val tssMgL: Double,
    val turbidityNtu: Double
)

data class FogDecision(
    val pFog: Int,
    val mediaState: String,
    val route: String,
    val knowledgeFactor: Double,
    val ecoImpactValue: Double,
    val harmRisk: Double
)

fun requireFiniteNonNegative(value: Double, name: String) {
    require(value.isFinite() && value >= 0.0) { "$name must be finite and non-negative" }
}

fun fogRoute(
    oilMgL: Double,
    tssMgL: Double,
    turbidityNtu: Double,
    thresholds: FogThresholds
): FogDecision {
    requireFiniteNonNegative(oilMgL, "oilMgL")
    requireFiniteNonNegative(tssMgL, "tssMgL")
    requireFiniteNonNegative(turbidityNtu, "turbidityNtu")
    require(thresholds.oilMgL > 0.0 && thresholds.tssMgL > 0.0 && thresholds.turbidityNtu > 0.0) {
        "field-calibrated thresholds must be positive"
    }

    val pFog = oilMgL > thresholds.oilMgL &&
        tssMgL > thresholds.tssMgL &&
        turbidityNtu > thresholds.turbidityNtu

    return if (pFog) {
        FogDecision(
            pFog = 1,
            mediaState = "UNMODELED_MIXED_OIL_WATER_SEDIMENT",
            route = "MANUAL_FIELD_REVIEW",
            knowledgeFactor = 0.45,
            ecoImpactValue = 0.35,
            harmRisk = 0.75
        )
    } else {
        FogDecision(
            pFog = 0,
            mediaState = "FIELD_SCREENED_NOT_ALL_FOG_CONDITIONS_MET",
            route = "RETAIN_FOR_SAMPLING_AND_CONTEXT_REVIEW",
            knowledgeFactor = 0.60,
            ecoImpactValue = 0.50,
            harmRisk = 0.35
        )
    }
}

fun main(args: Array<String>) {
    require(args.size == 6) {
        "usage: FogRouterKt <oil_mg_L> <TSS_mg_L> <turbidity_NTU> " +
            "<oil_threshold_mg_L> <TSS_threshold_mg_L> <turbidity_threshold_NTU>"
    }

    val decision = fogRoute(
        oilMgL = args[0].toDouble(),
        tssMgL = args[1].toDouble(),
        turbidityNtu = args[2].toDouble(),
        thresholds = FogThresholds(
            oilMgL = args[3].toDouble(),
            tssMgL = args[4].toDouble(),
            turbidityNtu = args[5].toDouble()
        )
    )

    println("P_fog=${decision.pFog}")
    println("media_state=${decision.mediaState}")
    println("route=${decision.route}")
    println("knowledge_factor=%.2f".format(decision.knowledgeFactor))
    println("eco_impact_value=%.2f".format(decision.ecoImpactValue))
    println("harm_risk=%.2f".format(decision.harmRisk))
}
