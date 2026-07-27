// Filename: src/ker_java/KerKernel.java

package ker_java;

public final class KerKernel {

    public static final int NUM_PLANES = 7;

    public static KerResult computeKer(double[] r, double[] w, double vtPrev) {
        double vt = 0.0;
        double maxRisk = 0.0;

        for (int i = 0; i < NUM_PLANES; i++) {
            double ri = r[i];
            if (ri < 0.0) ri = 0.0;
            if (ri > 1.0) ri = 1.0;
            double term = w[i] * ri * ri;
            vt += term;
            if (ri > maxRisk) {
                maxRisk = ri;
            }
        }

        double deltaVt = vt - vtPrev;

        double k = 0.95 - 0.4 * maxRisk;
        if (deltaVt > 0.0) {
            k -= 0.25;
        }
        if (k < 0.0) k = 0.0;
        if (k > 1.0) k = 1.0;

        double e = 0.95 - vt;
        if (deltaVt > 0.0) {
            e -= 0.3;
        }
        if (e < 0.0) e = 0.0;
        if (e > 1.0) e = 1.0;

        double rCoord = vt + Math.max(deltaVt, 0.0);
        if (rCoord > 1.0) rCoord = 1.0;
        if (rCoord < 0.0) rCoord = 0.0;

        return new KerResult(vt, k, e, rCoord);
    }

    public static final class KerResult {
        public final double vt;
        public final double k;
        public final double e;
        public final double r;

        public KerResult(double vt, double k, double e, double r) {
            this.vt = vt;
            this.k = k;
            this.e = e;
            this.r = r;
        }
    }
}
