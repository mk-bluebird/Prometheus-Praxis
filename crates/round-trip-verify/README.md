<!-- File: round-trip-verify/README.md -->
# round-trip-verify

`round-trip-verify` executes Rust, C++, Java, Kotlin, and Lua workload-assessment entry points using the same eight telemetry inputs. It asserts equality for `energyreqJ`, `deltaVt`, `knowledge_factor`, `eco_impact_value`, and `accepted`.

## Inputs

The verifier passes these positional values to each implementation:

1. Flow rate in cubic metres per second.
2. Hydraulic lift in metres.
3. Pump efficiency in the interval `(0, 1]`.
4. Runtime in seconds.
5. Voltage drop in volts.
6. Renewable-energy fraction in `[0, 1]`.
7. Embodied carbon intensity in grams per joule.
8. Biodiversity-risk value in `[0, 1]`.

## Build

```sh
cargo build --manifest-path round-trip-verify/Cargo.toml --release
```

## Run

Build all language entry points first, then provide executable commands through environment variables when their locations differ from the defaults:

```sh
export CYBOQUATIC_RUST_CMD="./cyboquatic-core/target/release/workload_assess"
export CYBOQUATIC_CPP_CMD="./build/cyboquatic_workload"
export CYBOQUATIC_JAVA_CMD="java -cp build/java org.prometheuspraxis.cyboquatic.WorkloadTelemetry"
export CYBOQUATIC_KOTLIN_CMD="./build/kotlin/workload_assess.kexe"
export CYBOQUATIC_LUA_CMD="luajit lua/cyboquatic/workload_assess.lua"
export LD_LIBRARY_PATH="./cyboquatic-core/target/release:${LD_LIBRARY_PATH}"

./round-trip-verify/target/release/round-trip-verify
```

The verifier exits nonzero if an implementation fails, omits a required output, produces a non-finite value, differs by more than `1e-9` for a floating-point result, or returns a different acceptance result.
