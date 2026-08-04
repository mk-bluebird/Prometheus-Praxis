// File: kotlin/src/main/kotlin/org/cyboquatic/fog/FOGRouterPredicates.kt
package org.cyboquatic.fog

/**
 * Kotlin FOG-router predicates aligned with Lua implementation.
 * Provides strongly-typed routing logic for cyboquatic machinery controllers.
 */
object FOGRouterPredicates {

    enum class Decision {
        SAFE, CAUTION, BLOCK
    }

    enum class Route {
        PRIMARY_CANAL, SECONDARY_CANAL, HOLD_TANK
    }

    fun classifyMedia(viscosityCP: Double, turbidityNTU: Double, organicFraction: Double): Decision {
        return if (viscosityCP < 5.0 && turbidityNTU < 50.0 && organicFraction > 0.7) {
            Decision.SAFE
        } else if (viscosityCP < 50.0 && turbidityNTU < 200.0 && organicFraction > 0.3) {
            Decision.CAUTION
        } else {
            Decision.BLOCK
        }
    }

    fun predicateScore(viscosityCP: Double, turbidityNTU: Double, organicFraction: Double): Double {
        val vScore = maxOf(0.0, 1.0 - (viscosityCP / 100.0))
        val tScore = maxOf(0.0, 1.0 - (turbidityNTU / 500.0))
        val oScore = maxOf(0.0, minOf(organicFraction, 1.0))
        return (vScore + tScore + oScore) / 3.0
    }

    fun suggestRoute(predicateScore: Double, canalCapacityM3S: Double): Route {
        return if (predicateScore >= 0.8 && canalCapacityM3S >= 0.1) {
            Route.PRIMARY_CANAL
        } else if (predicateScore >= 0.5 && canalCapacityM3S >= 0.05) {
            Route.SECONDARY_CANAL
        } else {
            Route.HOLD_TANK
        }
    }
}
