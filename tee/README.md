# TEE-backed KER Computation and ALN Signing

This directory contains a minimal, multi-language wiring pattern that keeps ALN
signing keys and KER computation confined to a Trusted Execution Environment
(TEE), while Kotlin orchestrates workloads and persists signed KER evidence.

The pattern matches the ecosystem described in the research shards:

- **ALN grammar** dictates KER rules and signing requirements.
- **C++ enclave** implements those rules safely on untrusted hardware.
- **Kotlin service** delegates KER computation to the enclave and stores signed
  assertions in SQLite, but never sees the ALN signing private key.

## Layout

- `tee/ker_enclave/`
  - `ker_enclave.edl`  
    Enclave Definition Language file defining the trusted ECALL
    `ker_compute_and_sign` and the untrusted OCALL `ocall_log`.

  - `enclave_ker.cpp`  
    Enclave-side C++ implementation:
    - Parses sensor shards.
    - Computes Lyapunov-based KER (K, E, R) according to the ecosafety grammar.
    - Computes `evidence_hex` inside the enclave.
    - Signs the payload (`KERAssertion + evidence_hash`) with the ALN DID
      signing key, which never leaves the enclave.

- `tee/ker_host/`
  - `ker_enclave_host.cpp`  
    Untrusted host-side C++ glue:
    - Loads and destroys the enclave (via SGX or equivalent API).
    - Exposes a JNI entrypoint
      `Java_org_prometheuspraxis_ker_KerEnclaveClient_ecallKerComputeAndSign`.
    - Forwards sensor shards into the enclave and returns the signature bytes.
    - Never holds or accesses the ALN signing private key.

- `tee/kotlin/`
  - `src/main/kotlin/org/prometheuspraxis/ker/KerEnclaveClient.kt`  
    Kotlin client:
    - Loads the `ker_enclave_host` native library.
    - Provides `computeKerSigned(sensorShard: ByteArray)` which:
      - Calls into the enclave via JNI.
      - Receives signature bytes.
      - Recomputes K,E,R and `evidenceHex` locally to build a
        `KerSignedAssertion`.
    - Suitable for writing KER evidence into SQLite shards.

  - `src/main/kotlin/org/prometheuspraxis/ker/KerLocalDecoder.kt`  
    Kotlin-side decoder:
    - Mirrors the enclave's KER calculation and evidence hash in a deterministic
      way for verification.
    - Does not sign or hold any private key; it only reconstructs KER and
      `evidenceHex` for local checks.

## Security Model

- **Key isolation**  
  The ALN signing DID private key is stored and used only inside the enclave
  (`enclave_ker.cpp`). The host and Kotlin layers never see this key.

- **Code integrity**  
  Enclave code is attested via TEE mechanisms (e.g., SGX `MRENCLAVE`):
  - Before sending sensor data, the host runs remote attestation.
  - Only an enclave whose measurement matches the expected ALN-KER binary
    is trusted to compute and sign KER.

- **Confidentiality**  
  Sensor shards are encrypted to the enclave; decryption and KER computation
  happen only inside the TEE. The host sees neither plaintext sensor data nor
  the signing key.

- **Governance**  
  KER assertions returned to Kotlin are:
  - Bound to the enclave via attestation.
  - Signed with the ALN DID private key.
  - Associated with `evidenceHex`, allowing later ALN invariants and auditors to
    verify provenance and correctness.

## Usage Overview

1. **Build the enclave**  
   Use the TEE/SGX toolchain to compile:
   - `ker_enclave.edl` → generate `Enclave_t.h` / `Enclave_u.h`.
   - `enclave_ker.cpp` → enclave shared object (`.signed` binary).

2. **Build the host library**  
   Compile `ker_enclave_host.cpp` into a shared library:
   - Linux: `libker_enclave_host.so`
   - macOS: `libker_enclave_host.dylib`
   - Windows: `ker_enclave_host.dll`

3. **Initialize the enclave**  
   On startup, the host application:
   - Calls `load_enclave("path/to/enclave.signed")`.
   - Performs remote attestation and establishes a secure channel.

4. **Compute KER**  
   From Kotlin:
   - Serialize a sensor shard into `ByteArray`.
   - Call `KerEnclaveClient.computeKerSigned(sensorShard)`.
   - Verify the signature against the ALN DID public key.
   - Store `k, e, r, evidenceHex, signature` in the relevant SQLite shard.

5. **Shutdown**  
   On shutdown:
   - Host calls `unload_enclave()` to destroy the enclave.

## Notes

- All math and KER functions (`compute_r_energy`, `compute_K_from_V`, etc.)
  must be consistent between enclave and non-enclave implementations and must
  be backed by documented ecosafety grammar.
- This directory intentionally contains no ad-hoc cryptography or unverified
  dependencies; all crypto primitives must be drawn from your validated
  toolchain and documented in the ALN grammar.
- Future extensions can add:
  - Remote attestation stapling into the KER assertion.
  - Cross-shard integrity checks (e.g., between KER assertions and kinetics
    shards) enforced by ALN invariants.

This TEE wiring pattern ensures that KER computation and ALN signing remain
trustworthy even when the underlying hardware and OS are untrusted.
