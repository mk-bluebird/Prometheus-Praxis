// File: tee/ker_enclave/enclave_ker.cpp
// Destination: Prometheus-Praxis/tee/ker_enclave/enclave_ker.cpp
//
// C++ enclave-side implementation for KER computation and ALN-signed assertions.
// Assumes integration with an SGX-style TEE and a minimal crypto API that is
// already validated and available in the toolchain.

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cstring>

// These headers are assumed to exist in the enclave SDK or your crypto layer.
// They must be real, validated dependencies in your build environment.
#include "Enclave_t.h"          // Generated from enclave EDL
#include "ker_crypto.hpp"       // Provides PrivateKey, sign_message, hash_bytes
#include "ker_serde.hpp"        // Provides serialize_assertion, parse_sensor_shard

struct KerAssertion {
    double K;
    double E;
    double R;
    // Minimal metadata for provenance; extend as needed.
    // All strings are kept short and fixed-size where possible for enclave safety.
    char deployment_id[64];
    char lane[16];
};

// Global ALN signing key, provisioned or unsealed inside enclave.
static PrivateKey g_aln_signing_key;
static bool g_key_initialized = false;

static void ensure_signing_key_initialized() {
    if (g_key_initialized) {
        return;
    }
    // In a real deployment this would unseal a key or use an SGX provisioning flow.
    // Here we assume a deterministic key loader from sealed storage.
    if (!load_aln_signing_key(g_aln_signing_key)) {
        ocall_log("Failed to load ALN signing key inside enclave");
    } else {
        g_key_initialized = true;
    }
}

static KerAssertion compute_ker_from_sensor_data(const uint8_t* data, size_t len) {
    KerAssertion assertion{};
    SensorShard shard{};
    if (!parse_sensor_shard(data, len, shard)) {
        ocall_log("Failed to parse sensor shard in enclave");
        assertion.K = 0.0;
        assertion.E = 0.0;
        assertion.R = 1.0;
        return assertion;
    }

    // Compute normalized risk coordinates. These functions are part of your
    // existing ecosafety Lyapunov/KER core and must exist in your toolchain.
    double r_energy = compute_r_energy(shard);
    double r_bod    = compute_r_bod(shard);
    double r_tss    = compute_r_tss(shard);

    // Lyapunov residual V = sum w_j r_j^2, with weights defined by corridors.
    double w_energy = 0.4;
    double w_bod    = 0.3;
    double w_tss    = 0.3;

    double V = w_energy * r_energy * r_energy
             + w_bod    * r_bod    * r_bod
             + w_tss    * r_tss    * r_tss;

    // KER calculation; these functions encode your KER grammar and bands.
    assertion.K = compute_K_from_V(V);
    assertion.E = compute_E_from_V(V);
    assertion.R = compute_R_from_V(V);

    // Copy metadata from shard into assertion (bounded copies).
    std::memset(assertion.deployment_id, 0, sizeof(assertion.deployment_id));
    std::memset(assertion.lane, 0, sizeof(assertion.lane));
    std::strncpy(assertion.deployment_id, shard.deployment_id, sizeof(assertion.deployment_id) - 1);
    std::strncpy(assertion.lane, shard.lane, sizeof(assertion.lane) - 1);

    return assertion;
}

void ker_compute_and_sign(
    const uint8_t* data,
    size_t data_len,
    uint8_t* signature_buf,
    size_t signature_buf_len,
    size_t* signature_len
) {
    if (!data || data_len == 0 || !signature_buf || !signature_len) {
        ocall_log("Invalid arguments to ker_compute_and_sign");
        if (signature_len) {
            *signature_len = 0;
        }
        return;
    }

    ensure_signing_key_initialized();
    if (!g_key_initialized) {
        ocall_log("ALN signing key not initialized");
        *signature_len = 0;
        return;
    }

    KerAssertion assertion = compute_ker_from_sensor_data(data, data_len);

    // Serialize assertion into a canonical byte representation.
    std::vector<uint8_t> payload = serialize_assertion(assertion);

    // Compute evidence hash inside enclave (e.g., hash256 of payload plus ALN spec hash).
    std::vector<uint8_t> evidence_hash = hash_bytes(payload);

    // Construct final message to sign: payload || evidence_hash.
    std::vector<uint8_t> msg_to_sign;
    msg_to_sign.reserve(payload.size() + evidence_hash.size());
    msg_to_sign.insert(msg_to_sign.end(), payload.begin(), payload.end());
    msg_to_sign.insert(msg_to_sign.end(), evidence_hash.begin(), evidence_hash.end());

    std::vector<uint8_t> sig = sign_message(g_aln_signing_key, msg_to_sign);

    if (sig.empty()) {
        ocall_log("Signature generation failed in enclave");
        *signature_len = 0;
        return;
    }

    if (sig.size() > signature_buf_len) {
        ocall_log("Signature buffer too small in ker_compute_and_sign");
        *signature_len = 0;
        return;
    }

    std::memcpy(signature_buf, sig.data(), sig.size());
    *signature_len = sig.size();
}
