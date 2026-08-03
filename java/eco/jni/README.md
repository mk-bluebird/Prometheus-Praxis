# java/eco/jni module: JNI Synapse Bridge

This directory documents JNI-based synapsis between Java and C++:

- `EcoSynapseJniBridge.java`  
  - Role: Thin Java wrapper over the C++ `eco_compute_simple_score` function in `libeco_synapse` (built from `cpp/tools/eco_synapse_cpp_bridge.cpp`).  
  - Wiring:
    - `System.loadLibrary("eco_synapse")` loads the shared library.
    - `native double eco_compute_simple_score(double k, double e, double r)` declares the binding.
    - Java calls are non-actuating analytics, mirroring the CLI+CSV pattern but via JNI for lower overhead.

JNI usage should remain minimal and well-audited. Most cross-language flows should prefer CLI + CSV/JSONL, reserving JNI for performance-critical analytics where ABI stability is carefully managed.
