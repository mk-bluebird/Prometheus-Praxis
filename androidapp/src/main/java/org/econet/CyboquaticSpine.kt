// filename: CyboquaticSpine.kt
// destination: Cyboquatics-Android/androidapp/src/main/java/org/econet/CyboquaticSpine.kt

package org.econet

object CyboquaticSpine {
    init {
        System.loadLibrary("cyboquatic_spine")
    }

    @JvmStatic external fun cybo_get_node_blastradius(dbPath: String, nodeId: String): String?
    @JvmStatic external fun cybo_get_workload_window(dbPath: String, nodeId: String): String?

    @JvmStatic
    fun nodeBlastRadius(dbPath: String, nodeId: String): String? {
        return cybo_get_node_blastradius(dbPath, nodeId)
    }

    @JvmStatic
    fun workloadWindow(dbPath: String, nodeId: String): String? {
        return cybo_get_workload_window(dbPath, nodeId)
    }
}
