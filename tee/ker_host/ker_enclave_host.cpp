// File: tee/ker_host/ker_enclave_host.cpp
// Destination: Prometheus-Praxis/tee/ker_host/ker_enclave_host.cpp
//
// Untrusted host-side C++ glue that loads the enclave, performs ECalls, and
// exposes a JNI interface for Kotlin. This file never sees the ALN private key.

#include <jni.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include "sgx_urts.h"          // SGX user-mode runtime, or equivalent TEE runtime.
#include "Enclave_u.h"         // Generated from ker_enclave.edl

static sgx_enclave_id_t g_eid = 0;

// Forward declaration for enclave load/unload; these should be called from
// application startup/shutdown.
bool load_enclave(const char* enclave_path) {
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    ret = sgx_create_enclave(enclave_path, 1, nullptr, nullptr, &g_eid, nullptr);
    return (ret == SGX_SUCCESS);
}

void unload_enclave() {
    if (g_eid != 0) {
        sgx_destroy_enclave(g_eid);
        g_eid = 0;
    }
}

extern "C" void ocall_log(const char* msg) {
    // Minimal logging; forward to stderr or system log.
    if (msg) {
        fprintf(stderr, "[KER_ENCLAVE] %s\n", msg);
    }
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_org_prometheuspraxis_ker_KerEnclaveClient_ecallKerComputeAndSign(
    JNIEnv* env,
    jclass,
    jbyteArray jdata
) {
    if (g_eid == 0) {
        // Enclave not loaded; caller must initialise.
        return nullptr;
    }

    if (jdata == nullptr) {
        return nullptr;
    }

    jsize len = env->GetArrayLength(jdata);
    if (len <= 0) {
        return nullptr;
    }

    std::vector<uint8_t> data(static_cast<size_t>(len));
    env->GetByteArrayRegion(jdata, 0, len, reinterpret_cast<jbyte*>(data.data()));

    uint8_t sig_buf[1024];
    size_t sig_len = 0;

    sgx_status_t ret = ker_compute_and_sign(
        g_eid,
        data.data(),
        data.size(),
        sig_buf,
        sizeof(sig_buf),
        &sig_len
    );
    if (ret != SGX_SUCCESS || sig_len == 0) {
        return nullptr;
    }

    jbyteArray jsig = env->NewByteArray(static_cast<jsize>(sig_len));
    if (!jsig) {
        return nullptr;
    }
    env->SetByteArrayRegion(jsig, 0, static_cast<jsize>(sig_len),
                            reinterpret_cast<const jbyte*>(sig_buf));
    return jsig;
}
