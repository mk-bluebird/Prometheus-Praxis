// filename: CyboOverlay.kt
// destination: Cyboquatics-Android/android/app/src/main/java/org/econet/CyboOverlay.kt
// Purpose:
// - Kotlin/Android facade for the cdylib JSON APIs.
// - UI-only visualization for Cyboquatic eco-metrics and blast-radius.
// - Extended with AI-chat specialized functions.

package org.econet

object CyboOverlay {

    init {
        System.loadLibrary("ecorestorationshard")
    }

    @JvmStatic external fun econet_get_ker_targets(dbPath: String, repoName: String): String?
    @JvmStatic external fun econet_get_blast_radius_for_node(dbPath: String, nodeId: String): String?
    @JvmStatic external fun econet_get_workload_trends_for_node(dbPath: String, nodeId: String): String?
    @JvmStatic external fun econet_get_cybo_node_eco_metrics(dbPath: String, nodeId: String): String?

    // AI-Chat specialized FFI functions
    @JvmStatic external fun econet_get_repo_manifest_agent(dbPath: String, repoName: String): String?
    @JvmStatic external fun econet_get_agent_safe_catalog(dbPath: String, repoName: String): String?
    @JvmStatic external fun econet_get_node_window(dbPath: String, nodeId: String): String?
    @JvmStatic external fun econet_get_blastradius_summary(dbPath: String, nodeId: String): String?

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

    // AI-Chat specialized methods

    @JvmStatic
    fun repoManifestAgent(dbPath: String, repoName: String): String? {
        return econet_get_repo_manifest_agent(dbPath, repoName)
    }

    @JvmStatic
    fun agentSafeCatalog(dbPath: String, repoName: String): String? {
        return econet_get_agent_safe_catalog(dbPath, repoName)
    }

    @JvmStatic
    fun nodeWindow(dbPath: String, nodeId: String): String? {
        return econet_get_node_window(dbPath, nodeId)
    }

    @JvmStatic
    fun blastRadiusSummary(dbPath: String, nodeId: String): String? {
        return econet_get_blastradius_summary(dbPath, nodeId)
    }
}
