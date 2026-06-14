// filename android/app/src/main/java/com/econet/cyboquatic/BlastRadiusInspector.kt
// destination eco_restoration_shard/android/app/src/main/java/com/econet/cyboquatic/BlastRadiusInspector.kt
// repo-target github.com/mk-bluebird/eco_restoration_shard

package com.econet.cyboquatic

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteException
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import android.view.View
import android.widget.*
import androidx.core.view.isVisible

/**
 * Android Operator Panel: Blast Radius and MT6883 Inspector
 *
 * Role:
 *  - Read-only view over EcoNet SQLite governance DB
 *  - Renders v_blast_radius_route_guard and v_cyber_physical_routing_effective
 *  - Provides HITL-style visibility: operators can see why a route is blocked,
 *    but cannot actuate hardware or change DB state.
 */
class BlastRadiusInspector : AppCompatActivity() {

    private lateinit var nodeIdInput: EditText
    private lateinit var destNodeIdInput: EditText
    private lateinit var inspectNodeBtn: Button
    private lateinit var inspectRouteBtn: Button
    private lateinit var statusView: TextView
    private lateinit var progressBar: ProgressBar

    private val dbPath: String by lazy {
        // For production, this should be wired via configuration or Room.
        // Here we assume the SQLite file is available on-device at this path.
        getDatabasePath("econet_constellation_index.sqlite3").absolutePath
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_blast_radius_inspector)

        nodeIdInput = findViewById(R.id.input_node_id)
        destNodeIdInput = findViewById(R.id.input_dest_node_id)
        inspectNodeBtn = findViewById(R.id.button_inspect_node)
        inspectRouteBtn = findViewById(R.id.button_inspect_route)
        statusView = findViewById(R.id.text_status)
        progressBar = findViewById(R.id.progress_loading)

        inspectNodeBtn.setOnClickListener {
            val nodeId = nodeIdInput.text.toString().trim()
            if (nodeId.isEmpty()) {
                toast("Enter a node DID")
            } else {
                runWithProgress { inspectNode(nodeId) }
            }
        }

        inspectRouteBtn.setOnClickListener {
            val src = nodeIdInput.text.toString().trim()
            val dst = destNodeIdInput.text.toString().trim()
            if (src.isEmpty() || dst.isEmpty()) {
                toast("Enter both source and destination node DIDs")
            } else {
                runWithProgress { inspectRoute(src, dst) }
            }
        }
    }

    private fun runWithProgress(block: () -> Unit) {
        progressBar.isVisible = true
        statusView.text = ""
        statusView.post {
            try {
                block()
            } finally {
                progressBar.isVisible = false
            }
        }
    }

    private fun inspectNode(nodeId: String) {
        val db = openReadOnly(dbPath) ?: return
        try {
            val cursor = db.rawQuery(
                """
                SELECT
                    nodeid,
                    region,
                    max_physical_radius_meters,
                    max_thermal_propagation_kelvin,
                    max_acoustic_decibels,
                    network_hop_containment,
                    medium,
                    max_permitted_attenuation,
                    environmental_safety_floor,
                    min_radius_m,
                    max_radius_m,
                    min_radius_h,
                    max_radius_h,
                    mean_risk_R,
                    mean_knowledge_K,
                    mean_energy_E
                FROM v_blast_radius_route_guard
                WHERE nodeid = ?
                LIMIT 1;
                """.trimIndent(),
                arrayOf(nodeId)
            )

            cursor.use {
                if (!it.moveToFirst()) {
                    statusView.text = "No blast-radius zone registered for node $nodeId"
                    return
                }

                val region = it.getString(1)
                val maxRadius = it.getDouble(2)
                val maxThermal = it.getDouble(3)
                val maxAcoustic = it.getDouble(4)
                val hopLimit = it.getInt(5)
                val medium = it.getString(6)
                val maxAtten = it.getDouble(7)
                val safetyFloor = it.getDouble(8)
                val minRadius = if (!it.isNull(9)) it.getDouble(9) else null
                val curMaxRadius = if (!it.isNull(10)) it.getDouble(10) else null
                val minHours = if (!it.isNull(11)) it.getDouble(11) else null
                val maxHours = if (!it.isNull(12)) it.getDouble(12) else null
                val meanR = if (!it.isNull(13)) it.getDouble(13) else null
                val meanK = if (!it.isNull(14)) it.getDouble(14) else null
                val meanE = if (!it.isNull(15)) it.getDouble(15) else null

                val physicalOk = curMaxRadius == null || curMaxRadius <= maxRadius
                val riskOk = meanR == null || meanR <= 0.13
                val lyapOk = meanK == null || meanK >= 0.85

                val sb = StringBuilder()
                sb.appendLine("Node: $nodeId")
                sb.appendLine("Region: $region")
                sb.appendLine()
                sb.appendLine("Blast Radius Envelope:")
                sb.appendLine("  Max permitted radius: $maxRadius m")
                sb.appendLine("  Observed radius: ${curMaxRadius ?: "n/a"} m")
                sb.appendLine("  Time window: ${minHours ?: "n/a"} .. ${maxHours ?: "n/a"} h")
                sb.appendLine()
                sb.appendLine("Medium Invariants:")
                sb.appendLine("  Medium: $medium")
                sb.appendLine("  Max attenuation: $maxAtten")
                sb.appendLine("  Safety floor: $safetyFloor")
                sb.appendLine("  Network hops: $hopLimit")
                sb.appendLine()
                sb.appendLine("Ecological Corridor Snapshot:")
                sb.appendLine("  Mean R: ${meanR ?: "n/a"}")
                sb.appendLine("  Mean K: ${meanK ?: "n/a"}")
                sb.appendLine("  Mean E: ${meanE ?: "n/a"}")
                sb.appendLine()
                sb.appendLine("Safety Flags:")
                sb.appendLine("  Physical envelope OK: $physicalOk")
                sb.appendLine("  R <= 0.13: $riskOk")
                sb.appendLine("  Lyapunov K >= 0.85: $lyapOk")

                statusView.text = sb.toString()
            }
        } catch (e: SQLiteException) {
            statusView.text = "SQLite error while inspecting node: ${e.message}"
        } finally {
            db.close()
        }
    }

    private fun inspectRoute(sourceNodeId: String, destNodeId: String) {
        val db = openReadOnly(dbPath) ?: return
        try {
            val cursor = db.rawQuery(
                """
                SELECT
                    route_id,
                    routing_status,
                    blast_safe,
                    allocated_bandwidth_mbps,
                    allowed_protocol,
                    region
                FROM v_cyber_physical_routing_effective
                WHERE source_nodeid = ?
                  AND destination_nodeid = ?
                LIMIT 1;
                """.trimIndent(),
                arrayOf(sourceNodeId, destNodeId)
            )

            cursor.use {
                if (!it.moveToFirst()) {
                    statusView.text = "No route found from $sourceNodeId to $destNodeId"
                    return
                }

                val routeId = it.getInt(0)
                val routingStatus = it.getString(1)
                val blastSafe = it.getInt(2) != 0
                val bandwidth = it.getInt(3)
                val protocol = it.getString(4)
                val region = it.getString(5)

                val permitted = blastSafe && routingStatus == "ACTIVE_ROUTED"

                val sb = StringBuilder()
                sb.appendLine("Route: $routeId")
                sb.appendLine("From: $sourceNodeId")
                sb.appendLine("To: $destNodeId")
                sb.appendLine("Region: $region")
                sb.appendLine()
                sb.appendLine("Link Budget:")
                sb.appendLine("  Bandwidth: $bandwidth Mbps")
                sb.appendLine("  Protocol: $protocol")
                sb.appendLine()
                sb.appendLine("Blast Radius Guard:")
                sb.appendLine("  blast_safe: $blastSafe")
                sb.appendLine("  routing_status: $routingStatus")
                sb.appendLine()
                sb.appendLine("Effective Verdict:")
                sb.appendLine("  permitted: $permitted")

                statusView.text = sb.toString()
            }
        } catch (e: SQLiteException) {
            statusView.text = "SQLite error while inspecting route: ${e.message}"
        } finally {
            db.close()
        }
    }

    private fun openReadOnly(path: String): SQLiteDatabase? {
        return try {
            SQLiteDatabase.openDatabase(
                path,
                null,
                SQLiteDatabase.OPEN_READONLY or SQLiteDatabase.NO_LOCALIZED_COLLATORS
            )
        } catch (e: SQLiteException) {
            statusView.text = "Failed to open governance DB at $path: ${e.message}"
            null
        }
    }

    private fun toast(msg: String) {
        Toast.makeText(this, msg, Toast.LENGTH_LONG).show()
    }
}
