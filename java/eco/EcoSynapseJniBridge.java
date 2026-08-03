// File: java/eco/EcoSynapseJniBridge.java
package eco;

/**
 * EcoSynapseJniBridge
 *
 * Optional Java module that demonstrates how a shared-library synapse
 * can be wired using JNI to call C++ `extern "C"` functions directly.
 *
 * This remains non-actuating; the native call is purely analytic.
 */
public class EcoSynapseJniBridge {

    static {
        // Load the shared library built from cpp/tools/eco_synapse_cpp_bridge.cpp
        // e.g., libeco_synapse.so or eco_synapse.dll.
        System.loadLibrary("eco_synapse");
    }

    // Native declaration matching extern "C" eco_compute_simple_score.
    private native double eco_compute_simple_score(double k, double e, double r);

    public double computeKerScore(double k, double e, double r) {
        return eco_compute_simple_score(k, e, r);
    }

    public static void main(String[] args) {
        if (args.length != 3) {
            System.out.println("Usage: java eco.EcoSynapseJniBridge <K> <E> <R>");
            return;
        }
        double k = Double.parseDouble(args[0]);
        double e = Double.parseDouble(args[1]);
        double r = Double.parseDouble(args[2]);

        EcoSynapseJniBridge bridge = new EcoSynapseJniBridge();
        double s = bridge.computeKerScore(k, e, r);
        System.out.println("KER scalar (JNI) s=" + s);
    }
}
