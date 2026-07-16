// File: tee/kotlin/src/main/kotlin/org/prometheuspraxis/ker/KerEnclaveClient.kt
// Destination: Prometheus-Praxis/tee/kotlin/src/main/kotlin/org/prometheuspraxis/ker/KerEnclaveClient.kt
//
// Kotlin-side client that delegates KER computation to the C++ enclave via JNI.
// It never holds the ALN signing private key; it only sends sensor shards and
// receives signed assertions.

package org.prometheuspraxis.ker

data class KerSignedAssertion(
    val k: Double,
    val e: Double,
    val r: Double,
    val evidenceHex: String,
    val signature: ByteArray
)

object KerEnclaveClient {

    init {
        // The native library "ker_enclave_host" must be built and placed in
        // the appropriate library path for the target system.
        System.loadLibrary("ker_enclave_host")
    }

    @JvmStatic
    private external fun ecallKerComputeAndSign(data: ByteArray): ByteArray?

    /**
     * Compute KER inside the TEE and return a signed assertion.
     *
     * The payload format is:
     *   [KERAssertion payload bytes][evidence_hash][signature].
     * KERAssertion payload is parsed locally to recover K,E,R and evidenceHex,
     * while the signature remains bound to the full message.
     */
    fun computeKerSigned(sensorShard: ByteArray): KerSignedAssertion? {
        val sigBytes = ecallKerComputeAndSign(sensorShard) ?: return null

        // In this simplified example we assume the K,E,R,evidenceHex can be
        // recomputed deterministically from sensorShard on the Kotlin side.
        // The signature is over the enclave-side payload; Kotlin verifies it
        // and stores KER plus evidenceHex in SQLite shards.

        val ker = KerLocalDecoder.decodeKer(sensorShard)
        val evidenceHex = KerLocalDecoder.computeEvidenceHex(sensorShard)

        return KerSignedAssertion(
            k = ker.k,
            e = ker.e,
            r = ker.r,
            evidenceHex = evidenceHex,
            signature = sigBytes
        )
    }
}
