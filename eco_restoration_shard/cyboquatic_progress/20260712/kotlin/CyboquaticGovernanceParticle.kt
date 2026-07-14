// file: eco_restoration_shard/cyboquatic_progress/20260712/kotlin/CyboquaticGovernanceParticle.kt
package eco_restoration_shard.cyboquatic_progress.y20260712.kotlin

data class CyboquaticGovernanceParticle(
    val did: String,
    val crateId: String,
    val domain: String,
    val subtaskId: String,
    val evidenceHex: String,
    val kScore: Double,
    val eScore: Double,
    val rScore: Double,
    val lyapunovVt: Double
) {
    init {
        require(kScore in 0.0..1.0)
        require(eScore in 0.0..1.0)
        require(rScore in 0.0..1.0)
        require(lyapunovVt >= 0.0)
    }

    fun sqlInsert(): String =
        "INSERT INTO daily_progress (yyyymmdd, crateid, domain, subtaskid, nodeid, sampleid, timestamputc, evidencehex, kfactor, efactor, rfactor, priorcrateid, didbound, vtafter) VALUES " +
        "('20260712','$crateId','$domain','$subtaskId','PHX-GOV-NODE-01','PHX-GOV-SAMPLE-0001','2026-07-12T23:31:00Z','$evidenceHex',$kScore,$eScore,$rScore,'cyboquatic_governance_particle_20260711','$did',$lyapunovVt);"
}
