// File: java/cyboquatic_workload_20260807/CyboquaticWorkload.java
import java.util.Locale;

public final class CyboquaticWorkload {
    private record Frame(String nodeId, double energyReqJ, double deltaVt,
                         double renewableFraction, double recoveredEnergyJ,
                         double waterQualityGain, double knowledgeFactor,
                         double ecoImpactValue, String decision) { }

    private static double clamp01(double value) {
        return Math.max(0.0, Math.min(1.0, value));
    }

    private static Frame evaluate(String nodeId, double energyReqJ, double deltaVt,
                                  double renewableFraction, double recoveredEnergyJ,
                                  double waterQualityGain) {
        double netEnergy = Math.max(0.0, energyReqJ - recoveredEnergyJ);
        double renewable = clamp01(renewableFraction);
        double waterGain = clamp01(waterQualityGain);
        double stability = Math.exp(-Math.max(0.0, deltaVt));
        double energyQuality = Math.exp(-netEnergy / 50000.0);
        double knowledge = clamp01(0.35 * stability + 0.30 * renewable
                + 0.20 * energyQuality + 0.15 * waterGain);
        double ecoImpact = clamp01(0.40 * stability + 0.30 * renewable
                + 0.20 * waterGain + 0.10 * energyQuality);
        boolean accepted = deltaVt <= 0.0 && renewable >= 0.70
                && netEnergy <= 50000.0 && waterGain >= 0.20;
        return new Frame(nodeId, energyReqJ, deltaVt, renewable, recoveredEnergyJ,
                waterGain, knowledge, ecoImpact, accepted ? "ACCEPT" : "REJECT");
    }

    public static void main(String[] args) {
        if (args.length != 6) {
            System.err.println("usage: CyboquaticWorkload NODE ENERGYREQJ DELTAVT "
                    + "RENEWABLE RECOVEREDJ WATERQUALITYGAIN");
            System.exit(1);
        }
        try {
            Frame f = evaluate(args[0], Double.parseDouble(args[1]), Double.parseDouble(args[2]),
                    Double.parseDouble(args[3]), Double.parseDouble(args[4]),
                    Double.parseDouble(args[5]));
            System.out.println("node_id,energyreq_j,delta_vt,renewable_fraction,"
                    + "recovered_energy_j,water_quality_gain,knowledge_factor,"
                    + "eco_impact_value,decision");
            System.out.printf(Locale.ROOT, "%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%s%n",
                    f.nodeId(), f.energyReqJ(), f.deltaVt(), f.renewableFraction(),
                    f.recoveredEnergyJ(), f.waterQualityGain(), f.knowledgeFactor(),
                    f.ecoImpactValue(), f.decision());
            System.exit("ACCEPT".equals(f.decision()) ? 0 : 2);
        } catch (NumberFormatException error) {
            System.err.println("all numeric inputs must be finite decimal values");
            System.exit(1);
        }
    }
}
