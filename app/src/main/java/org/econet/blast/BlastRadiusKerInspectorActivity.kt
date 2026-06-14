// filename: app/src/main/java/org/econet/blast/BlastRadiusKerInspectorActivity.kt
// destination: eco_restoration_shard/app/src/main/java/org/econet/blast/BlastRadiusKerInspectorActivity.kt
// repo-target: github.com/mk-bluebird/eco_restoration_shard

package org.econet.blast

import android.content.Context
import android.database.Cursor
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteException
import android.os.Bundle
import android.view.View
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.isVisible

/**
 * BlastRadiusKerInspectorActivity
 *
 * Read-only inspector over EcoNet / EcoRestoration governance SQLite databases.
 * This activity never mutates databases and never actuates external systems.
 *
 * It reads from:
 * - v_blast_radius_route_guard (econetconstellationindex.db)
 * - v_cyber_physical_routing_effective (econetconstellationindex.db)
 * - vrestorationnodesphx (restorationindex.sqlite3)
 * - vcyboquaticecoperjouleprodphx (restorationindex.sqlite3)
 * - vmt6883lanecontinuity (restorationindex.sqlite3)
 */
class BlastRadiusKerInspectorActivity : AppCompatActivity() {

    private lateinit var dbHelper: KerInspectorDbHelper

    private lateinit var spinnerDataSource: Spinner
    private lateinit var spinnerView: Spinner
    private lateinit var buttonRefresh: Button
    private lateinit var textStatus: TextView
    private lateinit var listResults: ListView
    private lateinit var progressBar: ProgressBar

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(createLayout())

        dbHelper = KerInspectorDbHelper(this)

        bindViews()
        setupSpinners()
        setupRefreshButton()
    }

    private fun bindViews() {
        spinnerDataSource = findViewById(ID_SPINNER_DATASOURCE)
        spinnerView = findViewById(ID_SPINNER_VIEW)
        buttonRefresh = findViewById(ID_BUTTON_REFRESH)
        textStatus = findViewById(ID_TEXT_STATUS)
        listResults = findViewById(ID_LIST_RESULTS)
        progressBar = findViewById(ID_PROGRESS)
    }

    private fun setupSpinners() {
        val dataSources = listOf(
            "EcoNet Constellation (Routing / Blast)" to KerInspectorDbHelper.DataSource.ECONET_CONSTELLATION,
            "Restoration Index (Phoenix KER / MT6883)" to KerInspectorDbHelper.DataSource.RESTORATION_INDEX
        )

        val dataSourceAdapter = ArrayAdapter(
            this,
            android.R.layout.simple_spinner_item,
            dataSources.map { it.first }
        ).also { adapter ->
            adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        }
        spinnerDataSource.adapter = dataSourceAdapter

        val views = listOf(
            "Blast Radius Guard (per node)" to KerInspectorDbHelper.QueryKind.BLAST_RADIUS_GUARD,
            "Routing Effective (per route)" to KerInspectorDbHelper.QueryKind.ROUTING_EFFECTIVE,
            "Phoenix Restoration Nodes (PROD-eligible)" to KerInspectorDbHelper.QueryKind.RESTORATION_NODES_PHX,
            "Phoenix Eco-per-Joule (Cyboquatic PROD)" to KerInspectorDbHelper.QueryKind.ECOPERJOULE_PROD_PHX,
            "MT6883 Lane Continuity" to KerInspectorDbHelper.QueryKind.MT6883_LANE_CONTINUITY
        )

        val viewAdapter = ArrayAdapter(
            this,
            android.R.layout.simple_spinner_item,
            views.map { it.first }
        ).also { adapter ->
            adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        }
        spinnerView.adapter = viewAdapter

        spinnerDataSource.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(
                parent: AdapterView<*>?,
                view: View?,
                position: Int,
                id: Long
            ) {
                updateStatus("Selected data source: ${dataSources[position].first}")
            }

            override fun onNothingSelected(parent: AdapterView<*>?) = Unit
        }

        spinnerView.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(
                parent: AdapterView<*>?,
                view: View?,
                position: Int,
                id: Long
            ) {
                updateStatus("Selected view: ${views[position].first}")
            }

            override fun onNothingSelected(parent: AdapterView<*>?) = Unit
        }
    }

    private fun setupRefreshButton() {
        buttonRefresh.setOnClickListener {
            runQuery()
        }
    }

    private fun runQuery() {
        progressBar.isVisible = true
        textStatus.text = "Running query..."
        listResults.adapter = null

        val dataSource = when (spinnerDataSource.selectedItemPosition) {
            0 -> KerInspectorDbHelper.DataSource.ECONET_CONSTELLATION
            else -> KerInspectorDbHelper.DataSource.RESTORATION_INDEX
        }

        val queryKind = when (spinnerView.selectedItemPosition) {
            0 -> KerInspectorDbHelper.QueryKind.BLAST_RADIUS_GUARD
            1 -> KerInspectorDbHelper.QueryKind.ROUTING_EFFECTIVE
            2 -> KerInspectorDbHelper.QueryKind.RESTORATION_NODES_PHX
            3 -> KerInspectorDbHelper.QueryKind.ECOPERJOULE_PROD_PHX
            else -> KerInspectorDbHelper.QueryKind.MT6883_LANE_CONTINUITY
        }

        try {
            val rows = dbHelper.runGovernanceQuery(dataSource, queryKind)
            val adapter = ArrayAdapter(
                this,
                android.R.layout.simple_list_item_1,
                rows
            )
            listResults.adapter = adapter
            updateStatus("Rows: ${rows.size} (${dataSource.name}, ${queryKind.name})")
        } catch (e: SQLiteException) {
            updateStatus("SQLite error: ${e.message}")
        } catch (e: IllegalStateException) {
            updateStatus("Configuration error: ${e.message}")
        } finally {
            progressBar.isVisible = false
        }
    }

    private fun updateStatus(message: String) {
        textStatus.text = message
    }

    /**
     * Build a simple layout programmatically so the file is self-contained.
     * This uses only standard widgets and numeric view IDs defined in-companion.
     */
    private fun createLayout(): View {
        val root = LinearLayout(this)
        root.orientation = LinearLayout.VERTICAL
        root.setPadding(16, 16, 16, 16)

        val header = TextView(this)
        header.text = "Blast Radius & KER Inspector"
        header.textSize = 18f
        header.id = ID_TEXT_HEADER

        val dataSourceLabel = TextView(this)
        dataSourceLabel.text = "Data source"
        dataSourceLabel.textSize = 14f

        val spinnerDs = Spinner(this)
        spinnerDs.id = ID_SPINNER_DATASOURCE

        val viewLabel = TextView(this)
        viewLabel.text = "Governance view"
        viewLabel.textSize = 14f

        val spinnerViewLocal = Spinner(this)
        spinnerViewLocal.id = ID_SPINNER_VIEW

        val btnRefresh = Button(this)
        btnRefresh.id = ID_BUTTON_REFRESH
        btnRefresh.text = "Refresh"

        val status = TextView(this)
        status.id = ID_TEXT_STATUS
        status.textSize = 12f

        val progress = ProgressBar(this)
        progress.id = ID_PROGRESS
        progress.isIndeterminate = true
        progress.isVisible = false

        val list = ListView(this)
        list.id = ID_LIST_RESULTS

        root.addView(header)
        root.addView(dataSourceLabel)
        root.addView(spinnerDs)
        root.addView(viewLabel)
        root.addView(spinnerViewLocal)
        root.addView(btnRefresh)
        root.addView(progress)
        root.addView(status)
        root.addView(list)

        return root
    }

    companion object {
        private const val ID_TEXT_HEADER = 0x1001
        private const val ID_SPINNER_DATASOURCE = 0x1002
        private const val ID_SPINNER_VIEW = 0x1003
        private const val ID_BUTTON_REFRESH = 0x1004
        private const val ID_TEXT_STATUS = 0x1005
        private const val ID_LIST_RESULTS = 0x1006
        private const val ID_PROGRESS = 0x1007
    }
}
