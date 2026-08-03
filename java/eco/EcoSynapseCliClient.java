// File: java/eco/EcoSynapseCliClient.java
package eco;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.Objects;

/**
 * EcoSynapseCliClient
 *
 * Java-side synapse module that calls the C++ CLI bridge
 * (`eco_synapse_cpp_bridge`) and parses its CSV output into
 * a simple POJO for downstream use in dashboards or governance tools.
 *
 * This uses only standard Java APIs and treats C++ as a pure analytics engine.
 */
public class EcoSynapseCliClient {

    public static final class KerScore {
        public final double k;
        public final double e;
        public final double r;
        public final double s;

        public KerScore(double k, double e, double r, double s) {
            this.k = k;
            this.e = e;
            this.r = r;
            this.s = s;
        }
    }

    public KerScore computeKerScore(double k, double e, double r) throws Exception {
        ProcessBuilder pb = new ProcessBuilder(
                "./build/eco_synapse_cpp_bridge",
                Double.toString(k),
                Double.toString(e),
                Double.toString(r)
        );
        pb.redirectErrorStream(true);
        Process proc = pb.start();

        try (BufferedReader reader = new BufferedReader(new InputStreamReader(proc.getInputStream()))) {
            String header = reader.readLine(); // "K,E,R,s"
            String row = reader.readLine();
            if (row == null) {
                throw new IllegalStateException("No data row from eco_synapse_cpp_bridge");
            }
            String[] parts = row.split(",");
            if (parts.length != 4) {
                throw new IllegalStateException("Unexpected CSV format: " + row);
            }
            double kVal = Double.parseDouble(parts[0]);
            double eVal = Double.parseDouble(parts[1]);
            double rVal = Double.parseDouble(parts[2]);
            double sVal = Double.parseDouble(parts[3]);
            return new KerScore(kVal, eVal, rVal, sVal);
        } finally {
            proc.waitFor();
        }
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 3) {
            System.out.println("Usage: java eco.EcoSynapseCliClient <K> <E> <R>");
            return;
        }
        double k = Double.parseDouble(args[0]);
        double e = Double.parseDouble(args[1]);
        double r = Double.parseDouble(args[2]);

        EcoSynapseCliClient client = new EcoSynapseCliClient();
        KerScore score = client.computeKerScore(k, e, r);
        System.out.println("KER score:");
        System.out.println("K=" + score.k + " E=" + score.e + " R=" + score.r + " s=" + score.s);
    }
}
