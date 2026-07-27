// Filename: src/ker_java/KerBenchmark.java

package ker_java;

import java.util.Random;

public final class KerBenchmark {

    public static void main(String[] args) {
        final int iterations = 1000;
        final double[] r = new double[KerKernel.NUM_PLANES];
        final double[] w = new double[KerKernel.NUM_PLANES];
        final Random rng = new Random(123456L);

        for (int i = 0; i < KerKernel.NUM_PLANES; i++) {
            w[i] = rng.nextDouble();
        }

        double vtPrev = 0.0;

        long t0 = System.nanoTime();
        for (int it = 0; it < iterations; it++) {
            for (int i = 0; i < KerKernel.NUM_PLANES; i++) {
                r[i] = rng.nextDouble();
            }
            KerKernel.KerResult res = KerKernel.computeKer(r, w, vtPrev);
            vtPrev = res.vt;
        }
        long t1 = System.nanoTime();

        long dtNs = t1 - t0;
        // Similar to C++, external measurement captures energy;
        // dtNs allows correlating compute time with energy.
    }
}
