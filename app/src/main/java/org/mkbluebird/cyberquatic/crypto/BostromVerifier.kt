// app/src/main/java/org/mkbluebird/cyberquatic/crypto/BostromVerifier.kt
import org.bitcoinj.core.Base58
import java.security.MessageDigest

class BostromVerifier {
    companion object {
        private const val EXPECTED_PREFIX = "bostrom"
    }
    
    fun verifySignature(
        message: String,
        signature: String,
        signerDid: String
    ): VerificationResult {
        // Extract address from DID
        if (!signerDid.startsWith(EXPECTED_PREFIX)) {
            return VerificationResult.Invalid("Invalid DID prefix")
        }
        
        val address = signerDid.removePrefix(EXPECTED_PREFIX)
        
        // Verify ed25519 signature
        val messageHash = MessageDigest.getInstance("SHA-256")
            .digest(message.toByteArray())
        
        val signatureBytes = Base58.decode(signature)
        val publicKeyBytes = recoverPublicKey(address)
        
        val isValid = Ed25519.verify(
            signature = signatureBytes,
            message = messageHash,
            publicKey = publicKeyBytes
        )
        
        return if (isValid) {
            VerificationResult.Valid(signerDid)
        } else {
            VerificationResult.Invalid("Signature mismatch")
        }
    }
    
    private fun recoverPublicKey(address: String): ByteArray {
        // Bostrom addresses are bech32-encoded
        // This is simplified; real implementation needs full bech32 decoder
        return Base58.decode(address)
    }
}
