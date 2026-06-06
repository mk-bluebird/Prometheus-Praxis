// filename: android/app/src/main/java/org/econet/CyboNodeScores.kt
// destination: Cyboquatics-Android/android/app/src/main/java/org/econet/CyboNodeScores.kt
// Purpose:
// - Kotlin/Android facade for read-only Cyboquatic node scoring.
// - Visualization-only: no corridor or actuation control.

package org.econet

object CyboNodeScores {
    init {
        System.loadLibrary("eco_restoration_shard_cybo")
    }

    @JvmStatic external fun econetCyboListNodes(dbPath: String, rplaneMax: Double): String?

    fun listEcorestorativeNodes(dbPath: String, rplaneMax: Double = 0.2): String? {
        return econetCyboListNodes(dbPath, rplaneMax)
    }
}
