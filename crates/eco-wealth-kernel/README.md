# eco-wealth-kernel

`eco-wealth-kernel` is a Rust crate that implements the eco-wealth kernel for the Prometheus-Praxis ecosafety spine. It encodes a frozen set of exponents \(\alpha, \beta, \gamma\) and a `noncompensableplanes` set to evaluate eco-wealth from KER coordinates (K, E, R) in a strictly non-compensable, governance-aligned way. The crate exposes a stable C ABI so that JVM applications (e.g., Spring Boot governance dashboards) can call into the Rust engine via JNI without leaving the JVM.

The crate is non-actuating and purely analytical: it only computes eco-wealth metrics and plane membership; it does not interact with hardware or control systems.

---

## Features

- Eco-wealth function:
  - Computes an eco-wealth scalar \(W\) from normalized \(K, E, R \in [0,1]\) using frozen exponents \(\alpha, \beta, \gamma\).
  - Enforces non-compensability: high knowledge or eco-benefit cannot hide high risk. [file:1]
- Non-compensable planes:
  - Encodes which axes (planes) are non-compensable (e.g., E and R) as an immutable set.
  - Exposes a simple C ABI function to query membership. [file:1]
- Stable C ABI:
  - `eco_wealth_compute` for eco-wealth evaluation.
  - `eco_is_noncompensable_plane` for plane membership checks.
- JVM integration:
  - Designed to be wrapped by a Java JNI surface (e.g., `EcoWealthKernel` class) used by Spring Boot dashboards. [web:27]

---

## Layout

This crate lives under the monorepo `crates` directory:

- `crates/eco-wealth-kernel/Cargo.toml`  
- `crates/eco-wealth-kernel/src/lib.rs`  

The crate is intended to be built as a native library (e.g., `libeco_wealth_kernel.so` or equivalent) and loaded via JNI in Java.

---

## Eco-wealth function

### Semantics

The eco-wealth kernel evaluates a scalar `W` that combines K (knowledge), E (eco-impact), and R (risk) with frozen exponents:

- Inputs:
  - `K`: knowledge in \([0,1]\).  
  - `E`: eco-impact in \([0,1]\).  
  - `R`: risk in \([0,1]\). [file:1]
- Parameters:
  - \(\alpha\) — exponent for K.  
  - \(\beta\) — exponent for E.  
  - \(\gamma\) — exponent for R.  

A typical eco-wealth function encoded in this crate is:

\[
W = \frac{K^\alpha \cdot E^\beta}{1 + R^\gamma}
\]

This structure:

- Rewards higher K and E through the numerator.  
- Penalizes higher R through the denominator, ensuring non-compensability: increases in R always reduce or cap W, even if K and E are large. [file:1]

### Rust API

The core Rust implementation is exposed via C ABI:

```rust
use std::os::raw::c_double;

/// Compute eco-wealth W from K, E, R with given exponents alpha, beta, gamma.
/// Inputs K, E, R must be normalized to.[1]
#[no_mangle]
pub extern "C" fn eco_wealth_compute(
    alpha: c_double,
    beta: c_double,
    gamma: c_double,
    k: c_double,
    e: c_double,
    r: c_double,
) -> c_double {
    let k_term = k.powf(alpha);
    let e_term = e.powf(beta);
    let r_term = r.powf(gamma);
    k_term * e_term / (1.0 + r_term)
}
```

- The exponents \(\alpha, \beta, \gamma\) are normally provided as frozen values by the Java JNI wrapper or other governance tooling.
- Inputs are expected to be in \([0,1]\); callers are responsible for normalization. [file:1]

---

## Non-compensable planes

### Plane encoding

The crate encodes non-compensable planes as integer IDs:

- `0` — K plane (knowledge).  
- `1` — E plane (eco-impact).  
- `2` — R plane (risk).  

A typical `noncompensableplanes` set is `{1, 2}`, meaning:

- E and R cannot be “offset” or “compensated” by other planes.  
- K is compensable in the sense that low K can be partly offset by improvements in E, but not vice versa when risk is high. [file:1]

### C ABI

```rust
use std::os::raw::c_int;

/// Returns 1 if the plane is non-compensable, 0 otherwise.
#[no_mangle]
pub extern "C" fn eco_is_noncompensable_plane(plane_id: c_int) -> c_int {
    match plane_id {
        1 | 2 => 1,
        _ => 0,
    }
}
```

- JVM callers pass plane IDs as integers for compatibility with JNI.  
- The function returns `1` for non-compensable planes and `0` otherwise. [file:1]

---

## C ABI summary

The crate exposes two C ABI functions intended for JNI and other FFI consumers:

```c
double eco_wealth_compute(
    double alpha,
    double beta,
    double gamma,
    double k,
    double e,
    double r
);

int eco_is_noncompensable_plane(int plane_id);
```

Both functions are pure and side-effect-free:

- No global state.  
- No allocation of resources that need manual cleanup by the caller.  
- Deterministic for given inputs. [web:46]

---

## Usage with Java / JNI

A typical Java wrapper (e.g., in a Spring Boot governance dashboard) will:

- Load the native library:

  ```java
  static {
      System.loadLibrary("eco_wealth_kernel");
  }
  ```

- Freeze the exponents \(\alpha, \beta, \gamma\) in Java to mirror the Rust defaults.  
- Forward normalized `K, E, R` to `eco_wealth_compute` via `native` methods.  
- Query `noncompensableplanes` via `eco_is_noncompensable_plane` to enforce non-compensation rules in UI and governance policies. [file:1]

This design keeps all heavy numeric logic and kernel semantics in Rust while allowing JVM-based governance tooling to remain fully in-process.

---

## Governance and safety posture

- **Non-compensation:**  
  - `noncompensableplanes` ensures eco-impact and risk planes cannot be masked by high knowledge or other planes. [file:1]
- **Risk-awareness:**  
  - The denominator term \((1 + R^\gamma)\) ensures that high R always suppresses eco-wealth, reflecting your ecosafety corridor constraints. [file:1]
- **Deterministic and auditable:**  
  - Frozen \(\alpha, \beta, \gamma\) and fixed plane encoding make the kernel behavior stable and reproducible across languages and deployments. [file:1]
- **Non-actuating:**  
  - The crate only computes metrics; it never drives actuators or external systems, making it safe for CI, governance dashboards, and offline analysis.

---

## Integration with KER spine

`eco-wealth-kernel` is intended to be used alongside the `ker-composition` crate and ALN KER specifications:

- KER provides normalized K, E, R coordinates for evidence shards and compositions. [file:1]
- `eco-wealth-kernel` evaluates a scalar eco-wealth `W` from those coordinates.  
- Governance dashboards and agents can use `W` and `noncompensableplanes` to:

  - Rank or filter evidence.
  - Enforce corridor constraints.
  - Visualize eco-wealth trajectories without violating non-compensation rules. [file:1]

This crate is a core numeric building block in the ecosafety governance pipeline, providing a consistent, verifiable eco-wealth kernel across Rust, JNI, and higher-level applications.
