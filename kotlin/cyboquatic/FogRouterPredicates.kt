// File: kotlin/cyboquatic/FogRouterPredicates.kt
package cyboquatic

data class MediaSample(
    val mediumType: String,      // e.g. "sediment", "water", "foam"
    val temperatureC: Double,
    val pfasConcentrationUgL: Double,
    val dissolvedO2MgL: Double,
    val turbidityNTU: Double
)

object FogRouterPredicates {

    fun isColdSurvivalCorridor(media: MediaSample): Boolean {
        val isCold = media.temperatureC <= 12.0
        val highPFAS = media.pfasConcentrationUgL >= 0.1
        return isCold && highPFAS
    }

    fun isEcoRestorationReady(media: MediaSample): Boolean {
        val o2Safe = media.dissolvedO2MgL >= 5.0
        val turbiditySafe = media.turbidityNTU <= 50.0
        val pfasLow = media.pfasConcentrationUgL < 0.05
        return o2Safe && turbiditySafe && pfasLow
    }

    fun route(media: MediaSample): String {
        return when {
            isColdSurvivalCorridor(media) ->
                "FOG:COLD_SURVIVAL_MONITOR"
            isEcoRestorationReady(media) ->
                "FOG:RESTORATION_PREFERRED"
            else ->
                "FOG:NEEDS_DIAGNOSTIC"
        }
    }
}

fun main() {
    val sample = MediaSample(
        mediumType = "water",
        temperatureC = 10.0,
        pfasConcentrationUgL = 0.2,
        dissolvedO2MgL = 6.0,
        turbidityNTU = 30.0
    )

    val route = FogRouterPredicates.route(sample)
    println("Media route: $route")
}
