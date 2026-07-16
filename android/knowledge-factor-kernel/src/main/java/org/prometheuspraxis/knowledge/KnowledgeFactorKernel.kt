package org.prometheuspraxis.knowledge

import org.json.JSONObject
import kotlin.math.max
import kotlin.math.min

/**
 * KnowledgeFactorKernel
 *
 * Exposes all kernel fields (weights, normalisation ranges) needed to
 * compute a K score from JSON evidence on Android clients.
 *
 * All dimensions are normalized to [0,1] before weighting.
 */
data class KnowledgeFactorKernel(
    val weightRecency: Double,
    val weightResolution: Double,
    val weightCoverage: Double,
    val weightReliability: Double,
    val recencyMinDays: Double,
    val recencyMaxDays: Double,
    val resolutionMinMeters: Double,
    val resolutionMaxMeters: Double,
    val coverageMinPercent: Double,
    val coverageMaxPercent: Double,
    val reliabilityMin: Double,
    val reliabilityMax: Double
) {

    /**
     * Compute K for a single evidence shard.
     *
     * Expected JSON structure:
     * {
     *   "recency_days": <number>,
     *   "resolution_meters": <number>,
     *   "coverage_percent": <number>,
     *   "reliability_score": <number in [0,1]>
     * }
     */
    fun computeKFromJson(json: JSONObject): Double {
        val recencyDays = json.optDouble("recency_days", recencyMaxDays)
        val resolutionMeters = json.optDouble("resolution_meters", resolutionMaxMeters)
        val coveragePercent = json.optDouble("coverage_percent", coverageMinPercent)
        val reliabilityScore = json.optDouble("reliability_score", reliabilityMin)

        // Normalize recency: newer (smaller days) should give higher K component.
        val recencyNormRaw = (recencyMaxDays - clamp(recencyDays, recencyMinDays, recencyMaxDays)) /
            (recencyMaxDays - recencyMinDays)
        val recencyNorm = clamp(recencyNormRaw, 0.0, 1.0)

        // Normalize resolution: finer (smaller meters) yields higher K.
        val resolutionNormRaw = (resolutionMaxMeters - clamp(resolutionMeters, resolutionMinMeters, resolutionMaxMeters)) /
            (resolutionMaxMeters - resolutionMinMeters)
        val resolutionNorm = clamp(resolutionNormRaw, 0.0, 1.0)

        // Normalize coverage: higher coveragePercent yields higher K.
        val coverageNormRaw = (clamp(coveragePercent, coverageMinPercent, coverageMaxPercent) - coverageMinPercent) /
            (coverageMaxPercent - coverageMinPercent)
        val coverageNorm = clamp(coverageNormRaw, 0.0, 1.0)

        // Normalize reliability: already in corridor [reliabilityMin, reliabilityMax].
        val reliabilityNormRaw = (clamp(reliabilityScore, reliabilityMin, reliabilityMax) - reliabilityMin) /
            (reliabilityMax - reliabilityMin)
        val reliabilityNorm = clamp(reliabilityNormRaw, 0.0, 1.0)

        // Weighted sum, then clamp to [0,1].
        val weighted =
            weightRecency * recencyNorm +
            weightResolution * resolutionNorm +
            weightCoverage * coverageNorm +
            weightReliability * reliabilityNorm

        return clamp(weighted, 0.0, 1.0)
    }

    companion object {

        /**
         * Default kernel consistent with KER corridor semantics.
         *
         * Weights sum to 1.0 so that K stays bounded in [0,1].
         */
        fun defaultKernel(): KnowledgeFactorKernel {
            return KnowledgeFactorKernel(
                weightRecency = 0.25,
                weightResolution = 0.25,
                weightCoverage = 0.25,
                weightReliability = 0.25,
                recencyMinDays = 0.0,
                recencyMaxDays = 365.0,
                resolutionMinMeters = 1.0,
                resolutionMaxMeters = 1000.0,
                coverageMinPercent = 0.0,
                coverageMaxPercent = 100.0,
                reliabilityMin = 0.0,
                reliabilityMax = 1.0
            )
        }

        /**
         * Compute K directly from JSON evidence string using the default kernel.
         */
        fun fromJsonEvidence(jsonEvidence: String): Double {
            val kernel = defaultKernel()
            val json = JSONObject(jsonEvidence)
            return kernel.computeKFromJson(json)
        }

        private fun clamp(value: Double, minValue: Double, maxValue: Double): Double {
            return min(max(value, minValue), maxValue)
        }
    }
}
