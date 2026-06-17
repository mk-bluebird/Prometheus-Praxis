// filename: android/app/src/main/java/org/econet/ChatKermapFacade.kt
// destination: Cyboquatics-Android/android/app/src/main/java/org/econet/ChatKermapFacade.kt
// purpose:
//   Lightweight Kotlin facade for AI-chat usage: small, JSON-string APIs over
//   the existing native cdylib, to reduce round-trip tokens.

package org.econet

object ChatKermapFacade {

    init {
        System.loadLibrary("ecorestorationshard")
    }

    @JvmStatic
    private external fun econetgetkertargets(dbPath: String, repoName: String): String?

    @JvmStatic
    private external fun econetgetworkloadtrendsfornode(dbPath: String, nodeId: String): String?

    fun getKerTargetsJson(dbPath: String, repoName: String): String? {
        return econetgetkertargets(dbPath, repoName)
    }

    fun getWorkloadTrendsJson(dbPath: String, nodeId: String): String? {
        return econetgetworkloadtrendsfornode(dbPath, nodeId)
    }
}
