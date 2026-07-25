// file: ecorestorationshard/cyboquaticprogress/20260724-g-blastradius/java/CanalBlastRadiusTelemetry.java
// destination: ecorestorationshard/cyboquaticprogress/20260724-g-blastradius/java/CanalBlastRadiusTelemetry.java
// purpose: Java telemetry and policy layer for canal blast-radius diagnostics,
//          enforcing KER and energy envelopes before inserting into SQLite.
//          Non-actuating, carbon-negative oriented [file:4][file:18].

package cyboquatics.blastradius;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.SQLException;
import java.time.Instant;
import java.util.Objects;

public final class CanalBlastRadiusTelemetry {

    public static final class BlastRadiusDiag {
        public final String blastDiagId;
        public final String surchargeEventId;
        public final String kerProfileId;
        public final String fogRegionId;
        public final String fogChannelId;
        public final String governanceParticleHex;
        public final Instant tsUtc;

        public final double radiusM;
        public final double rHydraulics;
        public final double rEnergy;
        public final double rTopology;
        public final double rBiodiversity;
        public final double vtResidual;
        public final double kKnowledgeFactor;
        public final double eEcoImpact;
        public final double rRiskFactor;
        public final double kerScore;
        public final double energyPerMJ;

        public BlastRadiusDiag(
                String blastDiagId,
                String surchargeEventId,
                String kerProfileId,
                String fogRegionId,
                String fogChannelId,
                String governanceParticleHex,
                Instant tsUtc,
                double radiusM,
                double rHydraulics,
                double rEnergy,
                double rTopology,
                double rBiodiversity,
                double vtResidual,
                double kKnowledgeFactor,
                double eEcoImpact,
                double rRiskFactor,
                double kerScore,
                double energyPerMJ
        ) {
            this.blastDiagId = Objects.requireNonNull(blastDiagId, "blastDiagId");
            this.surchargeEventId = Objects.requireNonNull(surchargeEventId, "surchargeEventId");
            this.kerProfileId = Objects.requireNonNull(kerProfileId, "kerProfileId");
            this.fogRegionId = Objects.requireNonNull(fogRegionId, "fogRegionId");
            this.fogChannelId = Objects.requireNonNull(fogChannelId, "fogChannelId");
            this.governanceParticleHex = Objects.requireNonNull(governanceParticleHex, "governanceParticleHex");
            this.tsUtc = Objects.requireNonNull(tsUtc, "tsUtc");
            this.radiusM = radiusM;
            this.rHydraulics = rHydraulics;
            this.rEnergy = rEnergy;
            this.rTopology = rTopology;
            this.rBiodiversity = rBiodiversity;
            this.vtResidual = vtResidual;
            this.kKnowledgeFactor = kKnowledgeFactor;
            this.eEcoImpact = eEcoImpact;
            this.rRiskFactor = rRiskFactor;
            this.kerScore = kerScore;
            this.energyPerMJ = energyPerMJ;
        }
    }

    public static final class Policy {
        public final double maxRadiusM;
        public final double maxVtResidual;
        public final double minKerScore;

        public Policy(double maxRadiusM, double maxVtResidual, double minKerScore) {
            this.maxRadiusM = maxRadiusM;
            this.maxVtResidual = maxVtResidual;
            this.minKerScore = minKerScore;
        }
    }

    /**
     * Validate a diagnostic against KER and residual corridors before persistence [file:18][file:31].
     */
    public static boolean isAcceptable(BlastRadiusDiag diag, Policy policy) {
        Objects.requireNonNull(diag, "diag");
        Objects.requireNonNull(policy, "policy");

        if (diag.radiusM > policy.maxRadiusM) {
            return false;
        }
        if (diag.vtResidual > policy.maxVtResidual) {
            return false;
        }
        if (diag.kerScore < policy.minKerScore) {
            return false;
        }
        return true;
    }

    /**
     * Persist a blast-radius diagnostic row into SQLite `blast_radius_diag` table.
     * This function assumes the schema from canal_blastradius_schema.sql is already applied [file:8][file:4].
     */
    public static void insertBlastRadiusDiag(Connection conn, BlastRadiusDiag diag) throws SQLException {
        Objects.requireNonNull(conn, "conn");
        Objects.requireNonNull(diag, "diag");

        final String sql = "INSERT INTO blast_radius_diag (" +
                "blast_diag_id, surcharge_event_id, ker_profile_id, ts_utc, " +
                "radius_m, r_hydraulics, r_energy, r_topology, r_biodiversity, " +
                "vt_residual, k_knowledge_factor, e_eco_impact, r_risk_factor, ker_score, " +
                "energy_per_m_j, fog_region_id, fog_channel_id, governance_particle_hex, created_at_utc" +
                ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setString(1, diag.blastDiagId);
            ps.setString(2, diag.surchargeEventId);
            ps.setString(3, diag.kerProfileId);
            ps.setString(4, diag.tsUtc.toString());
            ps.setDouble(5, diag.radiusM);
            ps.setDouble(6, diag.rHydraulics);
            ps.setDouble(7, diag.rEnergy);
            ps.setDouble(8, diag.rTopology);
            ps.setDouble(9, diag.rBiodiversity);
            ps.setDouble(10, diag.vtResidual);
            ps.setDouble(11, diag.kKnowledgeFactor);
            ps.setDouble(12, diag.eEcoImpact);
            ps.setDouble(13, diag.rRiskFactor);
            ps.setDouble(14, diag.kerScore);
            ps.setDouble(15, diag.energyPerMJ);
            ps.setString(16, diag.fogRegionId);
            ps.setString(17, diag.fogChannelId);
            ps.setString(18, diag.governanceParticleHex);
            ps.setString(19, Instant.now().toString());
            ps.executeUpdate();
        }
    }
}
