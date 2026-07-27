// filename: ecorestorationshard/cyboquaticprogress/20260725/java/BlastSurchargeTelemetry.java
// destination: ecorestorationshard/cyboquaticprogress/20260725/java/BlastSurchargeTelemetry.java
// repo-target: https://github.com/mk-bluebird/Prometheus-Praxis

package org.prometheuspraxis.cyboquatic.g20260725;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.SQLException;
import java.time.Instant;
import java.time.format.DateTimeFormatter;

/**
 * Non-actuating telemetry helper for blast-radius surcharge diagnostics.
 * - Bridges Java telemetry to SQLite (blast_surcharge_radius).
 * - Calls a native C++ kernel via JNI to get blast radius and risk coordinates.
 * - Computes K,E,R and kerscore locally, without any actuator interaction. [file:2][file:7]
 */
public final class BlastSurchargeTelemetry {

    static {
        // The native library must be built from blast_surcharge_kernel.cpp.
        // This load is non-actuating; it only enables diagnostic computation. [file:7]
        System.loadLibrary("blast_surcharge_kernel");
    }

    public static final class Input {
        public final String canalNodeId;
        public final String fogRegionId;
        public final String fogChannelId;

        public final double surchargeIndex;
        public final double flowM3s;
        public final double headLossM;
        public final double bulkDensityKgM3;
        public final double canalWidthM;
        public final double corridorRadiusM;

        public Input(
                String canalNodeId,
                String fogRegionId,
                String fogChannelId,
                double surchargeIndex,
                double flowM3s,
                double headLossM,
                double bulkDensityKgM3,
                double canalWidthM,
                double corridorRadiusM
        ) {
            this.canalNodeId = canalNodeId;
            this.fogRegionId = fogRegionId;
            this.fogChannelId = fogChannelId;
            this.surchargeIndex = surchargeIndex;
            this.flowM3s = flowM3s;
            this.headLossM = headLossM;
            this.bulkDensityKgM3 = bulkDensityKgM3;
            this.canalWidthM = canalWidthM;
            this.corridorRadiusM = corridorRadiusM;
        }
    }

    public static final class Output {
        public final double blastRadiusM;
        public final double vtResidual;
        public final double rEnergy;
        public final double rHydraulics;
        public final double rBio;
        public final double rTox;
        public final double rUncertainty;
        public final double rTopology;

        public Output(
                double blastRadiusM,
                double vtResidual,
                double rEnergy,
                double rHydraulics,
                double rBio,
                double rTox,
                double rUncertainty,
                double rTopology
        ) {
            this.blastRadiusM = blastRadiusM;
            this.vtResidual = vtResidual;
            this.rEnergy = rEnergy;
            this.rHydraulics = rHydraulics;
            this.rBio = rBio;
            this.rTox = rTox;
            this.rUncertainty = rUncertainty;
            this.rTopology = rTopology;
        }
    }

    // Native binding: must be implemented to marshal Input to BlastSurchargeInput in C++ and back. [file:7]
    private static native Output computeNative(Input in);

    // Simple K,E,R scoring consistent with existing drainage/workload grammar. [file:2]
    private static double clamp01(double x) {
        if (x < 0.0) return 0.0;
        if (x > 1.0) return 1.0;
        return x;
    }

    private static double computeK(double vtResidual) {
        // Knowledge high when residual is low. [file:2]
        return clamp01(1.0 - vtResidual);
    }

    private static double computeE(double vtResidual, double rEnergy, double rCarbonPlane) {
        // Ecoimpact high when residual is low and energy/carbon planes are benign. [file:2]
        double e = 1.0 - 0.6 * vtResidual - 0.3 * rEnergy - 0.1 * rCarbonPlane;
        return clamp01(e);
    }

    private static double computeR(double vtResidual, double rEnergy, double rHydraulics, double rBio, double rTox, double rUncertainty, double rTopology) {
        // Risk accumulates from residual and mandatory planes. [file:2]
        double r = 0.4 * vtResidual
                + 0.2 * rHydraulics
                + 0.1 * rEnergy
                + 0.1 * rBio
                + 0.1 * rTox
                + 0.05 * rUncertainty
                + 0.05 * rTopology;
        return clamp01(r);
    }

    private static String decideLane(double k, double e, double r) {
        // Simple lane logic; full governance uses Rust always-improve kernel. [file:7]
        double score = k + e - r;
        if (score >= 0.8 && r <= 0.2) {
            return "PRODUCTION";
        } else if (score >= 0.6 && r <= 0.3) {
            return "PILOT";
        } else {
            return "RESEARCH";
        }
    }

    /**
     * Compute blast-radius diagnostics and insert a row into blast_surcharge_radius.
     *
     * @param conn           SQLite connection (non-actuating).
     * @param input          physical metrics at breach segment.
     * @param governanceParticle ALN particle name for binding.
     * @param evidenceHex    Phoenix evidence hex for this diagnostic.
     * @param signingDid     Bostrom DID (provenance).
     */
    public static void recordBlastRadius(
            Connection conn,
            Input input,
            String governanceParticle,
            String evidenceHex,
            String signingDid
    ) throws SQLException {

        Output out = computeNative(input);

        double vt = clamp01(out.vtResidual);
        double rEnergy = clamp01(out.rEnergy);
        double rHydraulics = clamp01(out.rHydraulics);
        double rBio = clamp01(out.rBio);
        double rTox = clamp01(out.rTox);
        double rUncertainty = clamp01(out.rUncertainty);
        double rTopology = clamp01(out.rTopology);

        double k = computeK(vt);
        // Here rCarbonPlane is approximated by rTox; dedicated carbon planes exist in other shards. [file:2]
        double e = computeE(vt, rEnergy, rTox);
        double r = computeR(vt, rEnergy, rHydraulics, rBio, rTox, rUncertainty, rTopology);
        double kerscore = clamp01(k + e - r);

        String lane = decideLane(k, e, r);
        String tsUtc = DateTimeFormatter.ISO_INSTANT.format(Instant.now());

        String sql = "INSERT INTO blast_surcharge_radius (" +
                "canal_node_id, fog_region_id, fog_channel_id, timestamputc, " +
                "surcharge_index, flow_m3s, head_loss_m, bulk_density_kg_m3, energy_j, " +
                "blast_radius_m, vt_residual, r_energy, r_hydraulics, r_bio, r_tox, r_uncertainty, r_topology, " +
                "k_knowledge, e_ecoimpact, r_risk, kerscore, lane, governance_particle, evidence_hex, signing_did" +
                ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setString(1, input.canalNodeId);
            ps.setString(2, input.fogRegionId);
            ps.setString(3, input.fogChannelId);
            ps.setString(4, tsUtc);

            ps.setDouble(5, input.surchargeIndex);
            ps.setDouble(6, input.flowM3s);
            ps.setDouble(7, input.headLossM);
            ps.setDouble(8, input.bulkDensityKgM3);

            // energy_j here uses the same proxy as the C++ kernel; can be refined with real energetics. [file:2]
            double rho = input.bulkDensityKgM3;
            double Q = input.flowM3s;
            double W = (input.canalWidthM > 0.0) ? input.canalWidthM : 1.0;
            double H = (input.headLossM > 0.0) ? input.headLossM : 0.0;
            double energyProxy = 0.5 * rho * Q * Q / W * (1.0 + H);
            ps.setDouble(9, energyProxy);

            ps.setDouble(10, out.blastRadiusM);
            ps.setDouble(11, vt);
            ps.setDouble(12, rEnergy);
            ps.setDouble(13, rHydraulics);
            ps.setDouble(14, rBio);
            ps.setDouble(15, rTox);
            ps.setDouble(16, rUncertainty);
            ps.setDouble(17, rTopology);

            ps.setDouble(18, k);
            ps.setDouble(19, e);
            ps.setDouble(20, r);
            ps.setDouble(21, kerscore);
            ps.setString(22, lane);
            ps.setString(23, governanceParticle);
            ps.setString(24, evidenceHex);
            ps.setString(25, signingDid);

            ps.executeUpdate();
        }
    }
}
