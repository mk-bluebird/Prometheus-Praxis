// file: eco_restoration_shard/cyboquatic_progress/20260712/java/CyboquaticGovernanceParticle.java
package eco_restoration_shard.cyboquatic_progress.y20260712.java;

public final class CyboquaticGovernanceParticle {
    public final String did;
    public final String crateId;
    public final String domain;
    public final String subtaskId;
    public final String evidenceHex;
    public final double kScore;
    public final double eScore;
    public final double rScore;
    public final double lyapunovVt;

    public CyboquaticGovernanceParticle(String did, String crateId, String domain, String subtaskId, String evidenceHex, double kScore, double eScore, double rScore, double lyapunovVt) {
        this.did = did;
        this.crateId = crateId;
        this.domain = domain;
        this.subtaskId = subtaskId;
        this.evidenceHex = evidenceHex;
        this.kScore = clamp01(kScore);
        this.eScore = clamp01(eScore);
        this.rScore = clamp01(rScore);
        this.lyapunovVt = Math.max(0.0, lyapunovVt);
    }

    public static double clamp01(double x) {
        if (x < 0.0) return 0.0;
        if (x > 1.0) return 1.0;
        return x;
    }

    public String insertSql() {
        return "INSERT INTO daily_progress " +
                "(yyyymmdd, crateid, domain, subtaskid, nodeid, sampleid, timestamputc, evidencehex, kfactor, efactor, rfactor, priorcrateid, didbound, vtafter) VALUES " +
                "('20260712','" + crateId + "','" + domain + "','" + subtaskId + "','PHX-GOV-NODE-01','PHX-GOV-SAMPLE-0001','2026-07-12T23:31:00Z','" + evidenceHex + "'," +
                kScore + "," + eScore + "," + rScore + ",'cyboquatic_governance_particle_20260711','" + did + "'," + lyapunovVt + ");";
    }
}
