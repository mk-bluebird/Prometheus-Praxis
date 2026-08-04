// File: kotlin/src/main/kotlin/org/cyboquatic/dashboard/WorkloadDashboardApp.kt
package org.cyboquatic.dashboard

import javafx.application.Platform
import javafx.scene.paint.Color
import tornadofx.*
import java.sql.Connection
import java.sql.DriverManager

data class TelemetryPoint(
    val timestamp: Double,
    val energyreqJ: Double,
    val deltaVtMS: Double,
    val fogRoute: String,
    val kerE: Double
)

class WorkloadDashboardController : Controller() {

    private val jdbcUrl = "jdbc:sqlite:./data/cyboquatic_workload_dashboard.db"

    fun fetchLatestPoints(limit: Int = 200): List<TelemetryPoint> {
        DriverManager.getConnection(jdbcUrl).use { conn ->
            val sql = """
                SELECT timestamp_s, energyreq_j, delta_vt_m_s, fog_route, ker_e
                FROM cyboquatic_workload_telemetry
                ORDER BY timestamp_s DESC
                LIMIT ?
            """.trimIndent()
            conn.prepareStatement(sql).use { ps ->
                ps.setInt(1, limit)
                ps.executeQuery().use { rs ->
                    val pts = mutableListOf<TelemetryPoint>()
                    while (rs.next()) {
                        pts.add(
                            TelemetryPoint(
                                timestamp = rs.getDouble("timestamp_s"),
                                energyreqJ = rs.getDouble("energyreq_j"),
                                deltaVtMS = rs.getDouble("delta_vt_m_s"),
                                fogRoute = rs.getString("fog_route"),
                                kerE = rs.getDouble("ker_e")
                            )
                        )
                    }
                    return pts.reversed()
                }
            }
        }
    }

    fun hasKerEViolation(): Boolean {
        DriverManager.getConnection(jdbcUrl).use { conn ->
            val sql = """
                SELECT COUNT(*) AS cnt
                FROM cyboquatic_workload_telemetry
                WHERE ker_e > 0.0
            """.trimIndent()
            conn.createStatement().use { st ->
                st.executeQuery(sql).use { rs ->
                    val cnt = rs.getInt("cnt")
                    return cnt > 0
                }
            }
        }
    }
}

class WorkloadDashboardView : View("Cyboquatic Workload Dashboard") {

    private val controller: WorkloadDashboardController by inject()

    private val energySeries = XYChart.Series<Number, Number>().apply { name = "energyreqJ vs ΔVt" }

    override val root = borderpane {
        center = linechart("Energy vs ΔVt", NumberAxis(), NumberAxis()) {
            data.add(energySeries)
        }
        bottom = hbox {
            label("KER eco-impact status:") {
                addClass(Styles.statusLabel)
            }
            label("OK") {
                id = "kerStatusValue"
                textFill = Color.GREEN
            }
        }
    }

    init {
        runAsyncWithProgress {
            while (true) {
                val points = controller.fetchLatestPoints()
                val violation = controller.hasKerEViolation()
                Platform.runLater {
                    energySeries.data.clear()
                    points.forEach { p ->
                        val dp = XYChart.Data<Number, Number>(p.deltaVtMS, p.energyreqJ)
                        val color = when (p.fogRoute) {
                            "PRIMARY_CANAL" -> Color.GREEN
                            "SECONDARY_CANAL" -> Color.ORANGE
                            "HOLD_TANK" -> Color.RED
                            else -> Color.GRAY
                        }
                        dp.node = javafx.scene.shape.Circle(3.0, color)
                        energySeries.data.add(dp)
                    }
                    val statusLabel = root.lookup("#kerStatusValue") as javafx.scene.control.Label
                    if (violation) {
                        statusLabel.text = "VIOLATION"
                        statusLabel.textFill = Color.RED
                    } else {
                        statusLabel.text = "OK"
                        statusLabel.textFill = Color.GREEN
                    }
                }
                Thread.sleep(1000)
            }
        }
    }

    class Styles : Stylesheet() {
        companion object {
            val statusLabel by cssclass()
        }
        init {
            statusLabel {
                fontSize = 14.px
                padding = box(4.px)
            }
        }
    }
}

class WorkloadDashboardApp : App(WorkloadDashboardView::class, WorkloadDashboardView.Styles::class)

fun main() {
    launch<WorkloadDashboardApp>()
}
