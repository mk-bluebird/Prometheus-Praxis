// File: cpp/simulation/java/CyboquaticWorkloadMonitor.java
// Destination: mk-bluebird/Prometheus-Praxis/cpp/simulation/java/CyboquaticWorkloadMonitor.java

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.SQLException;
import java.sql.ResultSet;
import java.time.Instant;

public class CyboquaticWorkloadMonitor {

    public static class CanalNodeTelemetry {
        public String nodeId;
        public double flowRateM3s;
        public double headLossM;
        public double pumpPowerKw;
        public double liftHeightM;
        public double waterDensityKgm3;
        public double gravityMs2;
        public double ecoEfficiency;
        public double deltaVt;
        public double timestampSeconds;
    }

    public static class WorkloadResult {
        public double energyReqJ;
        public double ecoWeightedEnergyJ;
        public double deltaVt;
    }

    private double alpha;
    private double beta;
    private double maxAllowedDeltaV;
    private Double lastTimestamp = null;
    private double cumulativeEnergyJ = 0.0;
    private double cumulativeEcoEnergyJ = 0.0;
    private double cumulativeDeltaVt = 0.0;

    public CyboquaticWorkloadMonitor(double alpha, double beta, double maxAllowedDeltaV) {
        this.alpha = alpha;
        this.beta = beta;
        this.maxAllowedDeltaV = maxAllowedDeltaV;
    }

    public WorkloadResult step(CanalNodeTelemetry telemetry) {
        validateTelemetry(telemetry);

        double dt = computeDt(telemetry.timestampSeconds);
        double hydraulicEnergyJ = telemetry.waterDensityKgm3 *
                                  telemetry.gravityMs2 *
                                  telemetry.flowRateM3s *
                                  telemetry.liftHeightM *
                                  dt;

        double electricalEnergyJ = telemetry.pumpPowerKw * 1000.0 * dt;

        double energyReqJ = hydraulicEnergyJ + electricalEnergyJ;

        double ecoFactor = 1.0 + alpha * (1.0 - clamp01(telemetry.ecoEfficiency));
        double ecoWeightedEnergyJ = energyReqJ * ecoFactor;

        double deltaVt = beta * ecoWeightedEnergyJ;
        if (deltaVt > maxAllowedDeltaV) {
            deltaVt = maxAllowedDeltaV;
        }

        cumulativeEnergyJ += energyReqJ;
        cumulativeEcoEnergyJ += ecoWeightedEnergyJ;
        cumulativeDeltaVt += deltaVt;
        lastTimestamp = telemetry.timestampSeconds;

        WorkloadResult result = new WorkloadResult();
        result.energyReqJ = energyReqJ;
        result.ecoWeightedEnergyJ = ecoWeightedEnergyJ;
        result.deltaVt = deltaVt;
        return result;
    }

    public double getCumulativeEnergyJ() {
        return cumulativeEnergyJ;
    }

    public double getCumulativeEcoEnergyJ() {
        return cumulativeEcoEnergyJ;
    }

    public double getCumulativeDeltaVt() {
        return cumulativeDeltaVt;
    }

    public void reset() {
        lastTimestamp = null;
        cumulativeEnergyJ = 0.0;
        cumulativeEcoEnergyJ = 0.0;
        cumulativeDeltaVt = 0.0;
    }

    private static double clamp01(double v) {
        if (v < 0.0) return 0.0;
        if (v > 1.0) return 1.0;
        return v;
    }

    private static void validateTelemetry(CanalNodeTelemetry t) {
        if (t.flowRateM3s < 0.0) {
            throw new IllegalArgumentException("flowRateM3s must be non-negative");
        }
        if (t.liftHeightM < 0.0) {
            throw new IllegalArgumentException("liftHeightM must be non-negative");
        }
        if (t.pumpPowerKw < 0.0) {
            throw new IllegalArgumentException("pumpPowerKw must be non-negative");
        }
        if (t.waterDensityKgm3 <= 0.0) {
            throw new IllegalArgumentException("waterDensityKgm3 must be positive");
        }
        if (t.gravityMs2 <= 0.0) {
            throw new IllegalArgumentException("gravityMs2 must be positive");
        }
        if (t.ecoEfficiency < 0.0 || t.ecoEfficiency > 1.0) {
            throw new IllegalArgumentException("ecoEfficiency must be in [0,1]");
        }
    }

    private double computeDt(double currentTimestamp) {
        if (lastTimestamp == null) {
            return 1.0;
        }
        double dt = currentTimestamp - lastTimestamp;
        if (dt <= 0.0) {
            return 1.0;
        }
        return dt;
    }

    public static void main(String[] args) {
        CyboquaticWorkloadMonitor monitor = new CyboquaticWorkloadMonitor(
            0.5,
            1e-6,
            0.05
        );

        String url = "jdbc:sqlite:eco_restoration_shard.db";

        try (Connection conn = DriverManager.getConnection(url)) {
            conn.setAutoCommit(false);

            CanalNodeTelemetry t = new CanalNodeTelemetry();
            t.nodeId = "node-A";
            t.flowRateM3s = 0.4;
            t.headLossM = 0.2;
            t.pumpPowerKw = 1.2;
            t.liftHeightM = 2.0;
            t.waterDensityKgm3 = 1000.0;
            t.gravityMs2 = 9.81;
            t.ecoEfficiency = 0.9;
            t.deltaVt = 0.0;
            t.timestampSeconds = Instant.now().getEpochSecond();

            WorkloadResult r = monitor.step(t);

            try (PreparedStatement stmt = conn.prepareStatement(
                "INSERT INTO cyboquatic_workload_telemetry (" +
                "node_id, timestamputc, flow_rate_m3s, head_loss_m, pump_power_kw, " +
                "lift_height_m, water_density_kgm3, gravity_ms2, eco_efficiency, " +
                "energyreq_j, eco_energy_j, delta_v_t" +
                ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
            )) {
                stmt.setString(1, t.nodeId);
                stmt.setString(2, Instant.ofEpochSecond((long)t.timestampSeconds).toString());
                stmt.setDouble(3, t.flowRateM3s);
                stmt.setDouble(4, t.headLossM);
                stmt.setDouble(5, t.pumpPowerKw);
                stmt.setDouble(6, t.liftHeightM);
                stmt.setDouble(7, t.waterDensityKgm3);
                stmt.setDouble(8, t.gravityMs2);
                stmt.setDouble(9, t.ecoEfficiency);
                stmt.setDouble(10, r.energyReqJ);
                stmt.setDouble(11, r.ecoWeightedEnergyJ);
                stmt.setDouble(12, r.deltaVt);
                stmt.executeUpdate();
            }

            conn.commit();

            try (PreparedStatement q = conn.prepareStatement(
                "SELECT node_id, timestamputc, energyreq_j, eco_energy_j, delta_v_t " +
                "FROM cyboquatic_workload_telemetry " +
                "ORDER BY timestamputc DESC LIMIT 10"
            )) {
                try (ResultSet rs = q.executeQuery()) {
                    while (rs.next()) {
                        System.out.println(
                            "node=" + rs.getString(1) +
                            " ts=" + rs.getString(2) +
                            " energyreq_j=" + rs.getDouble(3) +
                            " eco_energy_j=" + rs.getDouble(4) +
                            " delta_v_t=" + rs.getDouble(5)
                        );
                    }
                }
            }

            System.out.println(
                "cumulative_energy_j=" + monitor.getCumulativeEnergyJ() +
                " cumulative_eco_energy_j=" + monitor.getCumulativeEcoEnergyJ() +
                " cumulative_delta_v_t=" + monitor.getCumulativeDeltaVt()
            );
        } catch (SQLException e) {
            e.printStackTrace();
        }
    }
}
