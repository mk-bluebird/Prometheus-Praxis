// File: java/src/main/java/eco/actuation/ActuationGateJNI.java
package eco.actuation;

import java.io.FileWriter;
import java.io.IOException;

/**
 * Java binding for the C++ ActuationGateSynapse via JNI.
 * Allows Java-based controllers to call evaluate_series and
 * serialise results to CSV.
 *
 * The native library should be built from actuation_gate_synapse.cpp
 * and loaded via System.loadLibrary("actuation_gate_synapse").
 */
public class ActuationGateJNI {

    static {
        System.loadLibrary("actuation_gate_synapse");
    }

    // JNI signature mapping to C++ function:
    // void ActuationGateSynapse_evaluate_series(...);
    private native void evaluateSeriesNative(
            double[] k,
            double[] e,
            double[] r,
            double[] ecoEnergy,
            double[] ecoEff,
            double[] ci,
            double[] maxCarbon,
            int len,
            double alpha,
            double beta,
            double gamma,
            double delta,
            double deltaVMax,
            double[] outKerS,
            double[] outCarbonCorridor,
            double[] outDeltaVt,
            int[] outAllowed
    );

    public ActuationResults evaluateSeries(
            double[] k,
            double[] e,
            double[] r,
            double[] ecoEnergy,
            double[] ecoEff,
            double[] ci,
            double[] maxCarbon,
            double alpha,
            double beta,
            double gamma,
            double delta,
            double deltaVMax
    ) {
        int len = k.length;
        double[] outKerS = new double[len];
        double[] outCarbonCorridor = new double[len];
        double[] outDeltaVt = new double[len];
        int[] outAllowed = new int[len];

        evaluateSeriesNative(
                k, e, r, ecoEnergy, ecoEff, ci, maxCarbon, len,
                alpha, beta, gamma, delta, deltaVMax,
                outKerS, outCarbonCorridor, outDeltaVt, outAllowed
        );

        return new ActuationResults(outKerS, outCarbonCorridor, outDeltaVt, outAllowed);
    }

    public static class ActuationResults {
        public final double[] kerS;
        public final double[] carbonCorridor;
        public final double[] deltaVt;
        public final int[] allowed;

        public ActuationResults(double[] kerS, double[] carbonCorridor,
                                double[] deltaVt, int[] allowed) {
            this.kerS = kerS;
            this.carbonCorridor = carbonCorridor;
            this.deltaVt = deltaVt;
            this.allowed = allowed;
        }

        public void toCsv(String path) throws IOException {
            try (FileWriter fw = new FileWriter(path)) {
                fw.write("index,ker_s,carbon_corridor,delta_v_t,allowed\n");
                for (int i = 0; i < kerS.length; ++i) {
                    fw.write(String.format(
                            "%d,%.6f,%.6f,%.6f,%d\n",
                            i, kerS[i], carbonCorridor[i], deltaVt[i], allowed[i]
                    ));
                }
            }
        }
    }

    // Example usage
    public static void main(String[] args) throws Exception {
        ActuationGateJNI gate = new ActuationGateJNI();

        double[] k = {0.8, 0.7, 0.9};
        double[] e = {0.85, 0.75, 0.9};
        double[] r = {0.2, 0.3, 0.15};
        double[] ecoEnergy = {1.2, 1.0, 1.5};
        double[] ecoEff = {0.9, 0.85, 0.88};
        double[] ci = {0.4, 0.5, 0.35};
        double[] maxCarbon = {1.0, 1.0, 1.0};

        double alpha = 0.5;
        double beta = 0.01;
        double gamma = 0.1;
        double delta = 0.1;
        double deltaVMax = 0.05;

        ActuationResults res = gate.evaluateSeries(
                k, e, r, ecoEnergy, ecoEff, ci, maxCarbon,
                alpha, beta, gamma, delta, deltaVMax
        );

        res.toCsv("actuation_gate_results.csv");
        System.out.println("Actuation gate evaluation completed; CSV written.");
    }
}
