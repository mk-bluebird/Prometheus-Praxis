// ecorestorationshard/cyboquaticprogress/20260723-d-cyboquaticworkload/java/CyboWorkloadTelemetry.java
// Non-actuating telemetry helper for cyboquatic workloads (energyreqJ, ΔVt).
// Designed to work with SQL schema cyboquatic_workload_schema.sql.

package cyboquatics.workload;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.SQLException;
import java.time.Instant;
import java.util.Objects;

public final class CyboWorkloadTelemetry {

    public static final class WorkloadFrame {
        public final String frameId;
        public final String nodeId;
        public final Instant timestampUtc;
        public final double energyReqJ;
        public final double energyCorridorMaxJ;
        public final double hydraulicLoad;
        public final double hydraulicCorridorMax;
        public final double carbonIntensity;
        public final double carbonCorridorMax;
        public final double uncertaintyRaw;
        public final double rEnergy;
        public final double rHydraulics;
        public final double rCarbon;
        public final double rUncertainty;
        public final double vtBefore;
        public final double vtAfter;
        public final double deltaVt;
        public final double k;
        public final double e;
        public final double r;
        public final double kerScore;
        public final String fogRegionId;
        public final String fogChannelId;
        public final String governanceParticleHex;

        public WorkloadFrame(
                String frameId,
                String nodeId,
                Instant timestampUtc,
                double energyReqJ,
                double energyCorridorMaxJ,
                double hydraulicLoad,
                double hydraulicCorridorMax,
                double carbonIntensity,
                double carbonCorridorMax,
                double uncertaintyRaw,
                double rEnergy,
                double rHydraulics,
                double rCarbon,
                double rUncertainty,
                double vtBefore,
                double vtAfter,
                double deltaVt,
                double k,
                double e,
                double r,
                double kerScore,
                String fogRegionId,
                String fogChannelId,
                String governanceParticleHex
        ) {
            this.frameId = Objects.requireNonNull(frameId, "frameId");
            this.nodeId = Objects.requireNonNull(nodeId, "nodeId");
            this.timestampUtc = Objects.requireNonNull(timestampUtc, "timestampUtc");
            this.energyReqJ = energyReqJ;
            this.energyCorridorMaxJ = energyCorridorMaxJ;
            this.hydraulicLoad = hydraulicLoad;
            this.hydraulicCorridorMax = hydraulicCorridorMax;
            this.carbonIntensity = carbonIntensity;
            this.carbonCorridorMax = carbonCorridorMax;
            this.uncertaintyRaw = uncertaintyRaw;
            this.rEnergy = rEnergy;
            this.rHydraulics = rHydraulics;
            this.rCarbon = rCarbon;
            this.rUncertainty = rUncertainty;
            this.vtBefore = vtBefore;
            this.vtAfter = vtAfter;
            this.deltaVt = deltaVt;
            this.k = k;
            this.e = e;
            this.r = r;
            this.kerScore = kerScore;
            this.fogRegionId = Objects.requireNonNull(fogRegionId, "fogRegionId");
            this.fogChannelId = Objects.requireNonNull(fogChannelId, "fogChannelId");
            this.governanceParticleHex = Objects.requireNonNull(governanceParticleHex, "governanceParticleHex");
        }
    }

    public interface WorkloadFrameSink {
        void persistFrame(WorkloadFrame frame) throws SQLException;
    }

    public static final class SqlWorkloadFrameSink implements WorkloadFrameSink {
        private final Connection connection;

        public SqlWorkloadFrameSink(Connection connection) {
            this.connection = Objects.requireNonNull(connection, "connection");
        }

        @Override
        public void persistFrame(WorkloadFrame frame) throws SQLException {
            String sql = """
                    INSERT INTO cybo_workload_frame (
                      frameid,
                      nodeid,
                      timestamputc,
                      energyreqj,
                      energycorridormaxj,
                      hydraulicload,
                      hydrauliccorridormax,
                      carbonintensity,
                      carboncorridormax,
                      uncertaintyraw,
                      renergy,
                      rhydraulics,
                      rcarbon,
                      runcertainty,
                      vtbefore,
                      vtafter,
                      deltavt,
                      k,
                      e,
                      r,
                      kerscore,
                      fogregionid,
                      fogchannelid,
                      governanceparticlehex
                    ) VALUES (
                      ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?
                    )
                    """;
            try (PreparedStatement ps = connection.prepareStatement(sql)) {
                ps.setString(1, frame.frameId);
                ps.setString(2, frame.nodeId);
                ps.setString(3, frame.timestampUtc.toString());
                ps.setDouble(4, frame.energyReqJ);
                ps.setDouble(5, frame.energyCorridorMaxJ);
                ps.setDouble(6, frame.hydraulicLoad);
                ps.setDouble(7, frame.hydraulicCorridorMax);
                ps.setDouble(8, frame.carbonIntensity);
                ps.setDouble(9, frame.carbonCorridorMax);
                ps.setDouble(10, frame.uncertaintyRaw);
                ps.setDouble(11, frame.rEnergy);
                ps.setDouble(12, frame.rHydraulics);
                ps.setDouble(13, frame.rCarbon);
                ps.setDouble(14, frame.rUncertainty);
                ps.setDouble(15, frame.vtBefore);
                ps.setDouble(16, frame.vtAfter);
                ps.setDouble(17, frame.deltaVt);
                ps.setDouble(18, frame.k);
                ps.setDouble(19, frame.e);
                ps.setDouble(20, frame.r);
                ps.setDouble(21, frame.kerScore);
                ps.setString(22, frame.fogRegionId);
                ps.setString(23, frame.fogChannelId);
                ps.setString(24, frame.governanceParticleHex);
                ps.executeUpdate();
            }
        }
    }

    public static final class IngestionPolicy {
        public final double maxEnergyReqJ;
        public final double maxDeltaVt;
        public final double minKerScore;

        public IngestionPolicy(double maxEnergyReqJ, double maxDeltaVt, double minKerScore) {
            this.maxEnergyReqJ = maxEnergyReqJ;
            this.maxDeltaVt = maxDeltaVt;
            this.minKerScore = minKerScore;
        }
    }

    private final WorkloadFrameSink sink;
    private final IngestionPolicy policy;

    public CyboWorkloadTelemetry(WorkloadFrameSink sink, IngestionPolicy policy) {
        this.sink = Objects.requireNonNull(sink, "sink");
        this.policy = Objects.requireNonNull(policy, "policy");
    }

    public boolean ingestFrame(WorkloadFrame frame) throws SQLException {
        Objects.requireNonNull(frame, "frame");

        if (frame.energyReqJ > policy.maxEnergyReqJ) {
            return false;
        }
        if (Math.abs(frame.deltaVt) > policy.maxDeltaVt) {
            return false;
        }
        if (frame.kerScore < policy.minKerScore) {
            return false;
        }

        sink.persistFrame(frame);
        return true;
    }
}
