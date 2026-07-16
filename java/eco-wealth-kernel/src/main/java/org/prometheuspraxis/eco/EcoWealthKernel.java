package org.prometheuspraxis.eco;

/**
 * EcoWealthKernel
 *
 * Java/JNI wrapper exposing frozen α, β, γ exponents and the
 * noncompensableplanes set, allowing Spring Boot dashboards to call
 * into the Rust eco-wealth engine without leaving the JVM.
 */
public final class EcoWealthKernel {

    static {
        System.loadLibrary("eco_wealth_kernel");
    }

    // Frozen exponents; must match Rust defaults.
    private static final double ALPHA = 0.5;
    private static final double BETA  = 0.3;
    private static final double GAMMA = 0.8;

    // Planes: 0 = K, 1 = E, 2 = R.
    private static final int PLANE_K = 0;
    private static final int PLANE_E = 1;
    private static final int PLANE_R = 2;

    private EcoWealthKernel() {
    }

    /**
     * Compute eco-wealth W using the frozen α, β, γ exponents.
     *
     * Inputs K, E, R are assumed to be normalized to [0,1].
     */
    public static double computeWealth(double k, double e, double r) {
        return ecoWealthCompute(ALPHA, BETA, GAMMA, k, e, r);
    }

    /**
     * Check if a plane is in noncompensableplanes.
     *
     * Example: E and R are non-compensable; K is compensable.
     */
    public static boolean isNonCompensablePlane(Plane plane) {
        int planeId;
        switch (plane) {
            case K:
                planeId = PLANE_K;
                break;
            case E:
                planeId = PLANE_E;
                break;
            case R:
                planeId = PLANE_R;
                break;
            default:
                planeId = -1;
        }
        return ecoIsNonCompensablePlane(planeId) != 0;
    }

    public enum Plane {
        K,
        E,
        R
    }

    // Native bindings to Rust C ABI

    private static native double ecoWealthCompute(
            double alpha,
            double beta,
            double gamma,
            double k,
            double e,
            double r
    );

    private static native int ecoIsNonCompensablePlane(int planeId);
}
