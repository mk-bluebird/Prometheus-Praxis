// filename: CyboOverlay.kt
// destination: Cyboquatics-Android/android/app/src/main/java/org/econet/CyboOverlay.kt
// Purpose:
// - Kotlin/Android facade for the cdylib JSON APIs.
// - UI-only visualization for Cyboquatic eco-metrics and blast-radius.

package org.econet

object CyboOverlay {

    init {
        System.loadLibrary("ecorestorationshard")
    }

    @JvmStatic external fun econet_get_ker_targets(dbPath: String, repoName: String): String?
    @JvmStatic external fun econet_get_blast_radius_for_node(dbPath: String, nodeId: String): String?
    @JvmStatic external fun econet_get_workload_trends_for_node(dbPath: String, nodeId: String): String?
    @JvmStatic external fun econet_get_cybo_node_eco_metrics(dbPath: String, nodeId: String): String?

    @JvmStatic
    fun kerTargets(dbPath: String, repoName: String): String? {
        return econet_get_ker_targets(dbPath, repoName)
    }

    @JvmStatic
    fun blastRadius(dbPath: String, nodeId: String): String? {
        return econet_get_blast_radius_for_node(dbPath, nodeId)
    }

    @JvmStatic
    fun workloadTrends(dbPath: String, nodeId: String): String? {
        return econet_get_workload_trends_for_node(dbPath, nodeId)
    }

    @JvmStatic
    fun cyboNodeEcoMetrics(dbPath: String, nodeId: String): String? {
        return econet_get_cybo_node_eco_metrics(dbPath, nodeId)
    }
}
