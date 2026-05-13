// filename: android/app/src/main/java/org/econet/BlastRadiusClient.kt
// destination: EcoNet/android/app/src/main/java/org/econet/BlastRadiusClient.kt
// purpose:
//   Kotlin/Android JNI wrapper around the Rust cdylib for read-only queries.

package org.econet

object BlastRadiusClient {
    init {
        System.loadLibrary("econet_blastradius_spine")
    }

    @JvmStatic
    private external fun econet_blastradius_spine_init_json(
        rootPathUtf8: String,
        regionUtf8: String,
        minRestorationScore: Double
    ): String?

    @JvmStatic
    private external fun econet_blastradius_spine_improvement_json(
        rootPathUtf8: String,
        laneUtf8: String
    ): String?

    @JvmStatic
    fun listShardsForRegion(rootPath: String, region: String, minRestorationScore: Double): String? {
        return econet_blastradius_spine_init_json(rootPath, region, minRestorationScore)
    }

    @JvmStatic
    fun listImprovementOk(rootPath: String, lane: String): String? {
        return econet_blastradius_spine_improvement_json(rootPath, lane)
    }
}
