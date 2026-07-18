// filename: ecorestoration_shard/cyboquatic_progress/20260717/kotlin/FogRouterPredicates20260717.kt
// destination: ecorestoration_shard/cyboquatic_progress/20260717/kotlin/FogRouterPredicates20260717.kt
// repo-target: https://github.com/mk-bluebird/Prometheus-Praxis

package org.mkbluebird.cyboquatic.fogrouter

import kotlin.math.max
import kotlin.math.min

/**
 * Cyboquatic FOG-router predicates for unmodeled media (Phoenix, 2026-07-17).
 * Non-actuating: provides diagnostics and K,E,R banding only. [file:2][file:12]
 */
data class FogSegment(
    val bodMgL: Double,
    val tssMgL: Double,
    val cecIndex: Double,
    val pfasNgL: Double,
    val rCalib: Double,
    val rSigma: Double,
    val energyReqJ: Double,
    val vtPrev: Double = 0.0
)

data class FogClassification(
    val rBod: Double,
    val rTss: Double,
    val rCec: Double,
    val rPfas: Double,
    val rDataQuality: Double,
    val vtResidual: Double,
    val energyReqJ: Double,
    val category: String,
    val kBand: Double,
    val eBand: Double,
    val rBand: Double
)

private fun clamp01(x: Double): Double = max(0.0, min(1.0, x))

private fun normalizeBod(bodMgL: Double): Double {
    return when {
        bodMgL <= 150.0 ->
            0.1 * (bodMgL / 150.0)
        bodMgL <= 300.0 ->
            0.1 + 0.4 * ((bodMgL - 150.0) / 150.0)
        else -> {
            val excess = bodMgL - 300.0
            clamp01(0.5 + 0.5 * (excess / (excess + 300.0)))
        }
    }
}

private fun normalizeTss(tssMgL: Double): Double {
    return when {
        tssMgL <= 100.0 ->
            0.1 * (tssMgL / 100.0)
        tssMgL <= 500.0 ->
            0.1 + 0.5 * ((tssMgL - 100.0) / 400.0)
        else -> {
            val excess = tssMgL - 500.0
            clamp01(0.6 + 0.4 * (excess / (excess + 400.0)))
        }
    }
}

private fun normalizeCec(cecIndex: Double): Double {
    return when {
        cecIndex <= 1.0 ->
            0.05 * cecIndex
        cecIndex <= 5.0 ->
            0.05 + 0.45 * ((cecIndex - 1.0) / 4.0)
        else -> {
            val excess = cecIndex - 5.0
            clamp01(0.5 + 0.5 * (excess / (excess + 5.0)))
        }
    }
}

private fun normalizePfas(pfasNgL: Double): Double {
    return when {
        pfasNgL <= 4.0 ->
            0.1 * (pfasNgL / 4.0)
        pfasNgL <= 20.0 ->
            0.1 + 0.6 * ((pfasNgL - 4.0) / 16.0)
        else -> {
            val excess = pfasNgL - 20.0
            clamp01(0.7 + 0.3 * (excess / (excess + 20.0)))
        }
    }
}

private fun normalizeDataQuality(rCalib: Double, rSigma: Double): Double {
    val rc = clamp01(rCalib)
    val rs = clamp01(rSigma)
    return clamp01(0.5 * rc + 0.5 * rs)
}

private fun vtResidual(
    rBod: Double,
    rTss: Double,
    rCec: Double,
    rPfas: Double,
    rDataQ: Double
): Double {
    val wBod = 0.15
    val wTss = 0.15
    val wCec = 0.25
    val wPfas = 0.30
    val wDq = 0.15
    return wBod * rBod * rBod +
            wTss * rTss * rTss +
            wCec * rCec * rCec +
            wPfas * rPfas * rPfas +
            wDq * rDataQ * rDataQ
}

/**
 * Classify a FOG segment into safe/monitored/unsafe router corridors
 * and derive a K,E,R band consistent with existing KER grammar. [file:2][file:21]
 */
fun classifyFogSegment(seg: FogSegment): FogClassification {
    val rBod = normalizeBod(seg.bodMgL)
    val rTss = normalizeTss(seg.tssMgL)
    val rCec = normalizeCec(seg.cecIndex)
    val rPfas = normalizePfas(seg.pfasNgL)
    val rDataQ = normalizeDataQuality(seg.rCalib, seg.rSigma)

    val vt = vtResidual(rBod, rTss, rCec, rPfas, rDataQ)
    val energyReqJ = seg.energyReqJ

    val category: String
    val kBand: Double
    val eBand: Double
    val rBand: Double

    when {
        vt <= 0.10 && energyReqJ <= 1.0e5 -> {
            category = "FOG_SAFE_CORRIDOR"
            kBand = 0.95
            eBand = 0.90
            rBand = 0.10
        }
        vt <= 0.25 && energyReqJ <= 5.0e5 -> {
            category = "FOG_MONITORED"
            kBand = 0.90
            eBand = 0.85
            rBand = 0.18
        }
        else -> {
            category = "FOG_UNSAFE_DIAGNOSTIC_ONLY"
            kBand = 0.88
            eBand = 0.75
            rBand = 0.25
        }
    }

    return FogClassification(
        rBod = rBod,
        rTss = rTss,
        rCec = rCec,
        rPfas = rPfas,
        rDataQuality = rDataQ,
        vtResidual = vt,
        energyReqJ = energyReqJ,
        category = category,
        kBand = kBand,
        eBand = eBand,
        rBand = rBand
    )
}

/**
 * CLI entrypoint: reads seven arguments and prints one-line JSON diagnostics.
 */
fun main(args: Array<String>) {
    if (args.size < 7) {
        System.err.println(
            "Usage: FogRouterPredicates20260717 <bod_mg_l> <tss_mg_l> <cec_index> <pfas_ng_l> <rcalib> <rsigma> <energy_req_j>"
        )
        return
    }

    fun parseDouble(s: String): Double =
        s.toDoubleOrNull() ?: 0.0

    val seg = FogSegment(
        bodMgL = parseDouble(args[0]),
        tssMgL = parseDouble(args[1]),
        cecIndex = parseDouble(args[2]),
        pfasNgL = parseDouble(args[3]),
        rCalib = clamp01(parseDouble(args[4])),
        rSigma = clamp01(parseDouble(args[5])),
        energyReqJ = parseDouble(args[6])
    )

    val c = classifyFogSegment(seg)

    val json = """
        {
          "r_bod": ${"%.4f".format(c.rBod)},
          "r_tss": ${"%.4f".format(c.rTss)},
          "r_cec": ${"%.4f".format(c.rCec)},
          "r_pfas": ${"%.4f".format(c.rPfas)},
          "r_data_quality": ${"%.4f".format(c.rDataQuality)},
          "vt_residual": ${"%.6f".format(c.vtResidual)},
          "energy_req_j": ${"%.1f".format(c.energyReqJ)},
          "category": "${c.category}",
          "k_band": ${"%.2f".format(c.kBand)},
          "e_band": ${"%.2f".format(c.eBand)},
          "r_band": ${"%.2f".format(c.rBand)}
        }
    """.trimIndent()

    println(json)
}
