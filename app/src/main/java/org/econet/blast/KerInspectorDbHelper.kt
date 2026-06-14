// filename: app/src/main/java/org/econet/blast/KerInspectorDbHelper.kt
// destination: eco_restoration_shard/app/src/main/java/org/econet/blast/KerInspectorDbHelper.kt
// repo-target: github.com/mk-bluebird/eco_restoration_shard

package org.econet.blast

import android.content.Context
import android.database.Cursor
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteException
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

/**
 * KerInspectorDbHelper
 *
 * Read-only access to EcoNet governance databases.
 * - Copies prebuilt .db files from assets/ to app-private storage on first use.
 * - Opens them READONLY for non-actuating inspection.
 * - Exposes high-level query kinds used by BlastRadiusKerInspectorActivity.
 */
class KerInspectorDbHelper(private val context: Context) {

    enum class DataSource {
        ECONET_CONSTELLATION,
        RESTORATION_INDEX
    }

    enum class QueryKind {
        BLAST_RADIUS_GUARD,
        ROUTING_EFFECTIVE,
        RESTORATION_NODES_PHX,
        ECOPERJOULE_PROD_PHX,
        MT6883_LANE_CONTINUITY
    }

    /**
     * Main entry point for governance queries.
     */
    @Throws(SQLiteException::class, IllegalStateException::class)
    fun runGovernanceQuery(dataSource: DataSource, queryKind: QueryKind): List<String> {
        val db = openDatabase(dataSource)
        val sql = when (queryKind) {
            QueryKind.BLAST_RADIUS_GUARD ->
                "SELECT nodeid, region, zone_id, max_physical_radius_meters, max_thermal_propagation_kelvin, " +
                    "max_acoustic_decibels, network_hop_containment, active_remedy_protocol, medium, " +
                    "max_permitted_attenuation, environmental_safety_floor, min_radius_m, max_radius_m, " +
                    "min_radius_h, max_radius_h, mean_risk_R, mean_knowledge_K, mean_energy_E " +
                    "FROM v_blast_radius_route_guard ORDER BY region, nodeid"

            QueryKind.ROUTING_EFFECTIVE ->
                "SELECT route_id, source_nodeid, destination_nodeid, allocated_bandwidth_mbps, " +
                    "allowed_protocol, routing_status, region, blast_safe, " +
                    "max_physical_radius_meters, max_radius_m, mean_risk_R " +
                    "FROM v_cyber_physical_routing_effective ORDER BY region, route_id"

            QueryKind.RESTORATION_NODES_PHX ->
                "SELECT nodeid, region, planeid, graphid, restorationradiusm, restorationradiushours, " +
                    "deltamasswindowkg, deltakarmawindow, gwriskmax, kerband, topologygrade, nonactuating, " +
                    "authorbostrom, authorcontractid, authorcomment, createdutc " +
                    "FROM vrestorationnodesphx ORDER BY nodeid"

            QueryKind.ECOPERJOULE_PROD_PHX ->
                "SELECT nodeid, region, domain, twindowstart, twindowend, vtresidual, kscore, escore, rscore, " +
                    "lane, kerdeployable, ecoperjoule, thetaecomin, carbonnegativeok, " +
                    "authorbostrom, authorcontractid " +
                    "FROM vcyboquaticecoperjouleprodphx ORDER BY nodeid, twindowstart"

            QueryKind.MT6883_LANE_CONTINUITY ->
                "SELECT kernelid, region, lane, kscore, escore, rscore, vtmax, planesok, topologyok, " +
                    "mt6883registryid, mt6883ok, neuroethicradiushours, neuroethicok, " +
                    "authorbostrom, authorcontractid, authorcomment, createdutc " +
                    "FROM vmt6883lanecontinuity ORDER BY region, lane, kernelid"
        }

        val rows = mutableListOf<String>()
        var cursor: Cursor? = null
        try {
            cursor = db.rawQuery(sql, emptyArray())
            while (cursor.moveToNext()) {
                rows.add(formatRow(queryKind, cursor))
            }
        } finally {
            cursor?.close()
            db.close()
        }

        return rows
    }

    /**
     * Open the appropriate database file in READONLY mode.
     * Assumes files are shipped in assets/ and copied into app files dir on first use.
     */
    @Throws(SQLiteException::class, IllegalStateException::class)
    private fun openDatabase(dataSource: DataSource): SQLiteDatabase {
        val dbFileName = when (dataSource) {
            DataSource.ECONET_CONSTELLATION -> "econetconstellationindex.db"
            DataSource.RESTORATION_INDEX -> "restorationindex.sqlite3"
        }

        val dbFile = File(context.filesDir, dbFileName)
        if (!dbFile.exists()) {
            copyDatabaseFromAssets(dbFileName, dbFile)
        }

        if (!dbFile.exists()) {
            throw IllegalStateException("Database file not available: $dbFileName")
        }

        return SQLiteDatabase.openDatabase(
            dbFile.absolutePath,
            null,
            SQLiteDatabase.OPEN_READONLY or SQLiteDatabase.NO_LOCALIZED_COLLATORS
        )
    }

    private fun copyDatabaseFromAssets(assetName: String, destFile: File) {
        try {
            context.assets.open(assetName).use { input ->
                FileOutputStream(destFile).use { output ->
                    val buffer = ByteArray(8 * 1024)
                    while (true) {
                        val read = input.read(buffer)
                        if (read <= 0) break
                        output.write(buffer, 0, read)
                    }
                    output.flush()
                }
            }
        } catch (e: IOException) {
            // Fail silently; caller will see missing DB and surface configuration error.
        }
    }

    private fun formatRow(kind: QueryKind, cursor: Cursor): String {
        return when (kind) {
            QueryKind.BLAST_RADIUS_GUARD -> {
                val nodeid = cursor.getString(0)
                val region = cursor.getString(1)
                val zoneId = cursor.getString(2)
                val maxR = cursor.getDouble(3)
                val maxT = cursor.getDouble(4)
                val maxDb = cursor.getDouble(5)
                val hops = cursor.getInt(6)
                val remedy = cursor.getString(7)
                val medium = cursor.getString(8)
                val atten = cursor.getDouble(9)
                val floor = cursor.getDouble(10)
                val minRm = cursor.getDouble(11)
                val maxRm = cursor.getDouble(12)
                val minRh = cursor.getDouble(13)
                val maxRh = cursor.getDouble(14)
                val meanR = cursor.getDouble(15)
                val meanK = cursor.getDouble(16)
                val meanE = cursor.getDouble(17)

                "node=$nodeid region=$region zone=$zoneId medium=$medium " +
                    "maxR=%.2f m maxT=%.3f K maxDb=%.1f dB hops=%d remedy=%s " +
                    "r[m]=[%.2f,%.2f] r[h]=[%.2f,%.2f] R=%.3f K=%.3f E=%.3f".format(
                        maxR, maxT, maxDb, hops, remedy,
                        minRm, maxRm, minRh, maxRh, meanR, meanK, meanE
                    )
            }

            QueryKind.ROUTING_EFFECTIVE -> {
                val routeId = cursor.getLong(0)
                val src = cursor.getString(1)
                val dst = cursor.getString(2)
                val bw = cursor.getInt(3)
                val proto = cursor.getString(4)
                val status = cursor.getString(5)
                val region = cursor.getString(6)
                val blastSafe = cursor.getInt(7)
                val maxR = cursor.getDouble(8)
                val maxRm = cursor.getDouble(9)
                val meanR = cursor.getDouble(10)

                "route=$routeId region=$region $src -> $dst bw=${bw}Mbps proto=$proto status=$status " +
                    "blastSafe=$blastSafe maxRcap=%.2f m maxRobs=%.2f m meanR=%.3f".format(
                        maxR, maxRm, meanR
                    )
            }

            QueryKind.RESTORATION_NODES_PHX -> {
                val nodeid = cursor.getString(0)
                val region = cursor.getString(1)
                val planeId = cursor.getString(2)
                val graphId = cursor.getString(3)
                val restRm = cursor.getDouble(4)
                val restRh = cursor.getDouble(5)
                val deltaMass = cursor.getDouble(6)
                val deltaKarma = cursor.getDouble(7)
                val gwRiskMax = cursor.getDouble(8)
                val kerBand = cursor.getString(9)
                val topoGrade = cursor.getString(10)
                val nonAct = cursor.getInt(11)
                val author = cursor.getString(12)
                val contract = cursor.getString(13)

                "node=$nodeid region=$region plane=$planeId graph=$graphId " +
                    "restR=%.2f m restT=%.2f h dM=%.3f kg dK=%.3f gwR=%.3f " +
                    "KER=$kerBand topo=$topoGrade nonact=$nonAct author=$author contract=$contract".format(
                        restRm, restRh, deltaMass, deltaKarma, gwRiskMax
                    )
            }

            QueryKind.ECOPERJOULE_PROD_PHX -> {
                val nodeid = cursor.getString(0)
                val region = cursor.getString(1)
                val domain = cursor.getString(2)
                val tStart = cursor.getString(3)
                val tEnd = cursor.getString(4)
                val vtResidual = cursor.getDouble(5)
                val k = cursor.getDouble(6)
                val e = cursor.getDouble(7)
                val r = cursor.getDouble(8)
                val lane = cursor.getString(9)
                val kerDeployable = cursor.getInt(10)
                val ecoPerJ = cursor.getDouble(11)
                val thetaEco = cursor.getDouble(12)
                val carbonOk = cursor.getInt(13)
                val author = cursor.getString(14)
                val contract = cursor.getString(15)

                "node=$nodeid region=$region domain=$domain lane=$lane K=%.3f E=%.3f R=%.3f " +
                    "Vt=%.3f ecoperJ=%.6f thetaEco=%.6f carbonOk=$carbonOk kerDeploy=$kerDeployable " +
                    "window=[$tStart,$tEnd] author=$author contract=$contract".format(
                        k, e, r, vtResidual, ecoPerJ, thetaEco
                    )
            }

            QueryKind.MT6883_LANE_CONTINUITY -> {
                val kernelId = cursor.getString(0)
                val region = cursor.getString(1)
                val lane = cursor.getString(2)
                val k = cursor.getDouble(3)
                val e = cursor.getDouble(4)
                val r = cursor.getDouble(5)
                val vtMax = cursor.getDouble(6)
                val planesOk = cursor.getInt(7)
                val topoOk = cursor.getInt(8)
                val registryId = cursor.getLong(9)
                val mtOk = cursor.getInt(10)
                val neuroHours = cursor.getDouble(11)
                val neuroOk = cursor.getInt(12)
                val author = cursor.getString(13)
                val contract = cursor.getString(14)

                "kernel=$kernelId region=$region lane=$lane K=%.3f E=%.3f R=%.3f VtMax=%.3f " +
                    "planesOk=$planesOk topoOk=$topoOk mt6883id=$registryId mt6883ok=$mtOk " +
                    "neuroEthicR=%.2f h neuroOk=$neuroOk author=$author contract=$contract".format(
                        k, e, r, vtMax, neuroHours
                    )
            }
        }
    }
}
