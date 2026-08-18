data class ExposureZone(
    val canalNodeId: String,
    val phoenixHexAnchor: String,
    val distanceM: Double,
    val conservativeRadiusM: Double,
    val zone: String,
    val action: String
)

fun classifyExposure(
    canalNodeId: String,
    phoenixHexAnchor: String,
    distanceM: Double,
    conservativeRadiusM: Double
): ExposureZone {
    require(canalNodeId.isNotBlank()) { "canalNodeId must be non-empty" }
    require(phoenixHexAnchor.isNotBlank()) { "phoenixHexAnchor must be non-empty" }
    require(distanceM.isFinite() && distanceM >= 0.0) { "distanceM must be finite and non-negative" }
    require(conservativeRadiusM.isFinite() && conservativeRadiusM > 0.0) {
        "conservativeRadiusM must be finite and positive"
    }

    return when {
        distanceM > conservativeRadiusM -> ExposureZone(
            canalNodeId,
            phoenixHexAnchor,
            distanceM,
            conservativeRadiusM,
            "SAFE",
            "OPERATE_ONLY_AFTER_SITE_REVIEW"
        )
        distanceM > conservativeRadiusM / 2.0 -> ExposureZone(
            canalNodeId,
            phoenixHexAnchor,
            distanceM,
            conservativeRadiusM,
            "CAUTION",
            "HOLD_FOR_FIELD_INSPECTION"
        )
        else -> ExposureZone(
            canalNodeId,
            phoenixHexAnchor,
            distanceM,
            conservativeRadiusM,
            "EXCLUDE",
            "NO_ENTRY_OR_OPERATION"
        )
    }
}

fun main(args: Array<String>) {
    require(args.size == 4) {
        "usage: HexExposureZoneKt <canal_node_id> <phoenix_hex_anchor> <distance_m> <conservative_radius_m>"
    }

    val result = classifyExposure(
        canalNodeId = args[0],
        phoenixHexAnchor = args[1],
        distanceM = args[2].toDouble(),
        conservativeRadiusM = args[3].toDouble()
    )

    println("canal_node_id=${result.canalNodeId}")
    println("phoenix_hex_anchor=${result.phoenixHexAnchor}")
    println("distance_m=${"%.8f".format(result.distanceM)}")
    println("conservative_radius_m=${"%.8f".format(result.conservativeRadiusM)}")
    println("zone=${result.zone}")
    println("action=${result.action}")
}
