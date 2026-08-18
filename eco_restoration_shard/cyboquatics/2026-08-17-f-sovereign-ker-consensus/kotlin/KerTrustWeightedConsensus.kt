import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

data class KerReport(
    val agentId: String,
    val trustWeight: Double,
    val knowledgeFactor: Double,
    val ecoImpactValue: Double,
    val harmRisk: Double
)

data class KerConsensus(
    val knowledgeFactor: Double,
    val ecoImpactValue: Double,
    val harmRisk: Double,
    val normalizedWeightSum: Double,
    val disagreementRange: Double,
    val decisionBand: String,
    val status: String
)

fun clamp01(value: Double): Double = max(0.0, min(1.0, value))

fun weightedKerConsensus(
    reports: List<KerReport>,
    maximumPermittedDisagreement: Double,
    minimumParticipatingAgents: Int
): KerConsensus {
    require(reports.isNotEmpty()) { "at least one KER report is required" }
    require(reports.map { it.agentId }.toSet().size == reports.size) { "agent identifiers must be unique" }
    require(maximumPermittedDisagreement in 0.0..1.0) { "maximum disagreement must be in [0, 1]" }
    require(minimumParticipatingAgents >= 1) { "minimum participating agents must be at least one" }

    reports.forEach {
        require(it.trustWeight >= 0.0 && it.trustWeight.isFinite()) { "trust weights must be finite and non-negative" }
        require(it.knowledgeFactor in 0.0..1.0 && it.ecoImpactValue in 0.0..1.0 && it.harmRisk in 0.0..1.0) {
            "KER values must be in [0, 1]"
        }
    }

    val totalTrust = reports.sumOf { it.trustWeight }
    require(totalTrust > 0.0) { "total trust weight must be positive" }

    val weightedK = reports.sumOf { it.trustWeight * it.knowledgeFactor } / totalTrust
    val weightedE = reports.sumOf { it.trustWeight * it.ecoImpactValue } / totalTrust
    val weightedR = reports.sumOf { it.trustWeight * it.harmRisk } / totalTrust

    val risks = reports.map { it.harmRisk }
    val disagreementRange = risks.max() - risks.min()
    val qualified = reports.count { it.trustWeight > 0.0 }

    val band = when {
        weightedR <= 0.25 -> "SAFE"
        weightedR < 0.60 -> "CAUTION"
        else -> "EXCLUDE"
    }

    val status = when {
        qualified < minimumParticipatingAgents -> "HOLD_INSUFFICIENT_PARTICIPATION"
        disagreementRange > maximumPermittedDisagreement -> "HOLD_CONFLICTING_INPUTS"
        band == "EXCLUDE" -> "EXCLUDE"
        band == "CAUTION" -> "CAUTION_REVIEW"
        else -> "SAFE_SCREENING_ONLY"
    }

    return KerConsensus(
        knowledgeFactor = clamp01(weightedK),
        ecoImpactValue = clamp01(weightedE),
        harmRisk = clamp01(weightedR),
        normalizedWeightSum = 1.0,
        disagreementRange = disagreementRange,
        decisionBand = band,
        status = status
    )
}

fun main(args: Array<String>) {
    require(args.size >= 7 && (args.size - 2) % 5 == 0) {
        "usage: KerTrustWeightedConsensusKt <maximum_disagreement_0_to_1> <minimum_agents> " +
            "<agent_id> <trust_weight> <K> <E> <R> [...]"
    }

    val maximumDisagreement = args[0].toDouble()
    val minimumAgents = args[1].toInt()
    val reports = buildList {
        var index = 2
        while (index < args.size) {
            add(
                KerReport(
                    agentId = args[index],
                    trustWeight = args[index + 1].toDouble(),
                    knowledgeFactor = args[index + 2].toDouble(),
                    ecoImpactValue = args[index + 3].toDouble(),
                    harmRisk = args[index + 4].toDouble()
                )
            )
            index += 5
        }
    }

    val result = weightedKerConsensus(reports, maximumDisagreement, minimumAgents)
    println("knowledge_factor=${"%.8f".format(result.knowledgeFactor)}")
    println("eco_impact_value=${"%.8f".format(result.ecoImpactValue)}")
    println("harm_risk=${"%.8f".format(result.harmRisk)}")
    println("normalized_weight_sum=${"%.8f".format(result.normalizedWeightSum)}")
    println("risk_disagreement_range=${"%.8f".format(result.disagreementRange)}")
    println("decision_band=${result.decisionBand}")
    println("status=${result.status}")
}
