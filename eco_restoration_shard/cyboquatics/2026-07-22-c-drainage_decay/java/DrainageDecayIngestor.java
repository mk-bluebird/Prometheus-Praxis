// eco_restoration_shard/cyboquatics/2026-07-22-c-drainage_decay/java/DrainageDecayIngestor.java

package cyboquatics.drainagedecay;

import java.time.Instant;
import java.util.Objects;

/**
 * Java ingestion and rule engine for drainage decay frames.
 *
 * This class is designed to interface with SQL telemetry backends and Kotlin models.
 */
public final class DrainageDecayIngestor {

    public static final class Frame {
        public final String frameId;
        public final String canalNodeId;
        public final Instant timestamp;
        public final double bodMgL;
        public final double tssMgL;
        public final double cecCmolPerKg;
        public final double frameEnergyJ;
        public final double deltaVtMps;
        public final double kKnowledgeFactor;
        public final double eEcoImpact;
        public final double rRiskFactor;
        public final double kerScore;
        public final String fogRegionId;
        public final String fogChannelId;
        public final String governanceParticleHex;

        public Frame(
                String frameId,
                String canalNodeId,
                Instant timestamp,
                double bodMgL,
                double tssMgL,
                double cecCmolPerKg,
                double frameEnergyJ,
                double deltaVtMps,
                double kKnowledgeFactor,
                double eEcoImpact,
                double rRiskFactor,
                double kerScore,
                String fogRegionId,
                String fogChannelId,
                String governanceParticleHex
        ) {
            this.frameId = Objects.requireNonNull(frameId, "frameId");
            this.canalNodeId = Objects.requireNonNull(canalNodeId, "canalNodeId");
            this.timestamp = Objects.requireNonNull(timestamp, "timestamp");
            this.bodMgL = bodMgL;
            this.tssMgL = tssMgL;
            this.cecCmolPerKg = cecCmolPerKg;
            this.frameEnergyJ = frameEnergyJ;
            this.deltaVtMps = deltaVtMps;
            this.kKnowledgeFactor = kKnowledgeFactor;
            this.eEcoImpact = eEcoImpact;
            this.rRiskFactor = rRiskFactor;
            this.kerScore = kerScore;
            this.fogRegionId = Objects.requireNonNull(fogRegionId, "fogRegionId");
            this.fogChannelId = Objects.requireNonNull(fogChannelId, "fogChannelId");
            this.governanceParticleHex = Objects.requireNonNull(governanceParticleHex, "governanceParticleHex");
        }
    }

    public interface FrameSink {
        void persist(Frame frame) throws Exception;
    }

    public static final class IngestionPolicy {
        public final double maxFrameEnergyJ;
        public final double minKerScore;
        public final double maxBodMgL;
        public final double maxTssMgL;

        public IngestionPolicy(double maxFrameEnergyJ, double minKerScore, double maxBodMgL, double maxTssMgL) {
            this.maxFrameEnergyJ = maxFrameEnergyJ;
            this.minKerScore = minKerScore;
            this.maxBodMgL = maxBodMgL;
            this.maxTssMgL = maxTssMgL;
        }
    }

    private final FrameSink sink;
    private final IngestionPolicy policy;

    public DrainageDecayIngestor(FrameSink sink, IngestionPolicy policy) {
        this.sink = Objects.requireNonNull(sink, "sink");
        this.policy = Objects.requireNonNull(policy, "policy");
    }

    /**
     * Validates and persists a frame according to KER and energy constraints.
     */
    public boolean ingest(Frame frame) throws Exception {
        Objects.requireNonNull(frame, "frame");

        if (frame.frameEnergyJ > policy.maxFrameEnergyJ) {
            return false;
        }
        if (frame.kerScore < policy.minKerScore) {
            return false;
        }
        if (frame.bodMgL > policy.maxBodMgL) {
            return false;
        }
        if (frame.tssMgL > policy.maxTssMgL) {
            return false;
        }

        sink.persist(frame);
        return true;
    }
}
