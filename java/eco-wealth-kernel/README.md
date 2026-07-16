# java/eco-wealth-kernel

The `java/eco-wealth-kernel` module provides a Java/JNI wrapper around the Rust `eco-wealth-kernel` crate, allowing JVM-based applications (such as Spring Boot governance dashboards) to compute eco-wealth and query non-compensable planes without leaving the JVM.

It is designed as a thin, deterministic binding layer:

- Rust is the single source of truth for the eco-wealth algebra. [file:1]
- Java exposes a stable, idiomatic API for governance tooling and dashboards.
- JNI bridges the two worlds via a small set of native methods.

This module is non-actuating and purely analytical: it computes metrics and evaluates kernel properties but never interacts with hardware or control systems.

---

## Overview

The Java wrapper:

- Loads the native library built from the Rust `eco-wealth-kernel` crate.
- Exposes a high-level API for:

  - Computing eco-wealth from KER coordinates \(K, E, R \in [0,1]\).  
  - Querying whether a given KER plane is in the `noncompensableplanes` set. [file:1]

- Encodes frozen exponents \(\alpha, \beta, \gamma\) on the JVM side to mirror the Rust kernel, ensuring alignment between ecosystems.

---

## Layout

The module lives under the mono-repo `java` directory:

- `java/eco-wealth-kernel/src/main/java/org/prometheuspraxis/eco/EcoWealthKernel.java`  
- (Optional) `java/eco-wealth-kernel/src/test/java/...` for tests.

The Java package name is:

```text
org.prometheuspraxis.eco
```

This matches the governance-focused naming scheme used across the Prometheus-Praxis mono-repo. [file:1]

---

## EcoWealthKernel API

### Class: `org.prometheuspraxis.eco.EcoWealthKernel`

This class is the main JVM entry point.

#### Static library loading

```java
static {
    System.loadLibrary("eco_wealth_kernel");
}
```

- The native library `libeco_wealth_kernel` (or platform equivalent) is built from the Rust crate `crates/eco-wealth-kernel`.
- Loading is performed once at class initialization time. [file:1]

#### Frozen exponents

```java
private static final double ALPHA = 0.5;
private static final double BETA  = 0.3;
private static final double GAMMA = 0.8;
```

- These constants must match the Rust defaults in the `eco-wealth-kernel` crate.
- They define the eco-wealth kernel’s shape:

  - \(\alpha\) controls the influence of knowledge K.  
  - \(\beta\) controls the influence of eco-impact E.  
  - \(\gamma\) controls sensitivity to risk R. [file:1]

#### Plane enumeration

```java
public enum Plane {
    K,
    E,
    R
}
```

- Encodes the K, E, and R planes used by `noncompensableplanes`. [file:1]
- Provides type safety for governance code instead of magic integers.

---

## Core methods

### `computeWealth(double k, double e, double r)`

```java
/**
 * Compute eco-wealth W using the frozen α, β, γ exponents.
 *
 * Inputs K, E, R are assumed to be normalized to.[1]
 */
public static double computeWealth(double k, double e, double r) {
    return ecoWealthCompute(ALPHA, BETA, GAMMA, k, e, r);
}
```

- Calls the native `eco_wealth_compute` function in Rust.  
- Applies the eco-wealth function:

  \[
  W = \frac{K^\alpha \cdot E^\beta}{1 + R^\gamma}
  \]

- This structure ensures that high risk R always suppresses W, even when K and E are large, enforcing non-compensability at the metric level. [file:1]

### `isNonCompensablePlane(Plane plane)`

```java
/**
 * Check if a plane is in noncompensableplanes.
 *
 * Example: E and R are non-compensable; K is compensable.
 */
public static boolean isNonCompensablePlane(Plane plane) {
    int planeId;
    switch (plane) {
        case K:
            planeId = 0;
            break;
        case E:
            planeId = 1;
            break;
        case R:
            planeId = 2;
            break;
        default:
            planeId = -1;
    }
    return ecoIsNonCompensablePlane(planeId) != 0;
}
```

- Encodes planes as integer IDs (0 = K, 1 = E, 2 = R) to match the Rust/FFI layer. [file:1]
- Calls the native `eco_is_noncompensable_plane` function:

  - Returns `true` if the plane belongs to `noncompensableplanes`.  
  - Typically E and R are non-compensable, while K may be compensable. [file:1]

---

## Native bindings

Two native methods connect to the Rust `eco-wealth-kernel` crate:

```java
private static native double ecoWealthCompute(
        double alpha,
        double beta,
        double gamma,
        double k,
        double e,
        double r
);

private static native int ecoIsNonCompensablePlane(int planeId);
```

- `ecoWealthCompute` is a direct JNI binding to the C ABI function `eco_wealth_compute`.  
- `ecoIsNonCompensablePlane` is a binding to `eco_is_noncompensable_plane`. [file:1]

These methods are never called directly by governance code; they are wrapped by `computeWealth` and `isNonCompensablePlane` for type safety and parameter validation.

---

## Integration with Spring Boot

The `EcoWealthKernel` class is designed to be used from Spring Boot and similar JVM frameworks:

- **Controller usage example:**

  ```java
  @RestController
  @RequestMapping("/eco")
  public class EcoWealthController {

      @GetMapping("/wealth")
      public double wealth(
              @RequestParam("k") double k,
              @RequestParam("e") double e,
              @RequestParam("r") double r
      ) {
          // Inputs should be pre-normalized to.[1]
          return EcoWealthKernel.computeWealth(k, e, r);
      }

      @GetMapping("/noncompensable")
      public boolean isNonCompensable(@RequestParam("plane") String plane) {
          EcoWealthKernel.Plane p = EcoWealthKernel.Plane.valueOf(plane.toUpperCase());
          return EcoWealthKernel.isNonCompensablePlane(p);
      }
  }
  ```

- **Governance dashboards:**

  - Can display eco-wealth summaries and flag non-compensable dimensions by calling `computeWealth` and `isNonCompensablePlane`.
  - Can enforce corridor rules client-side before committing changes to KER shards. [file:1]

---

## Build and linkage expectations

- The Rust crate `crates/eco-wealth-kernel` must be built as a shared library (e.g., `libeco_wealth_kernel.so`, `eco_wealth_kernel.dll`, or platform equivalent). [web:46]
- The library must be placed on the JVM’s native library path so that:

  ```java
  System.loadLibrary("eco_wealth_kernel");
  ```

  succeeds at class initialization time.

- JNI headers are generated from `EcoWealthKernel` using standard JDK tooling (e.g., `javac -h`). The Rust crate exports the matching C ABI functions (`eco_wealth_compute`, `eco_is_noncompensable_plane`) without additional JNI glue code. [web:43][web:47]

---

## Governance and safety posture

- **Non-compensation enforcement:**

  - By exposing `noncompensableplanes` directly to the JVM, governance code can prevent user interfaces and pipelines from attempting to “trade off” eco-impact or risk against other dimensions. [file:1]

- **Risk-aware metrics:**

  - The eco-wealth function structure ensures that increasing risk always reduces or caps the eco-wealth score, consistent with the ecosafety corridor. [file:1]

- **Deterministic behavior:**

  - Frozen exponents \(\alpha, \beta, \gamma\) and fixed plane encodings ensure that all JVM and Rust deployments share the same semantics, making audits and replay analysis reliable. [file:1]

- **Non-actuating design:**

  - The module computes metrics only; it does not perform any actuation or external side effects, making it safe for CI, dashboards, and offline analysis.

---

## Alignment with KER and KERComposition

`java/eco-wealth-kernel` is intended to operate on KER-derived coordinates:

- K, E, R values are produced by KER particles and compositions (e.g., via the `ker-composition` crate and ALN `KERComposition2026v1` spec). [file:1]
- The eco-wealth kernel then compresses these into a scalar `W` for governance decisions and visualizations.
- This separation of concerns keeps composition algebra, eco-wealth evaluation, and governance UI aligned but modular.
