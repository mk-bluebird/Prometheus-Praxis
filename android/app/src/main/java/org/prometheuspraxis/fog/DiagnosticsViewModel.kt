// Filename: android/app/src/main/java/org/prometheuspraxis/fog/DiagnosticsViewModel.kt
// Kotlin Android diagnostics dashboard, reading the shared SQLite DB via Room.
// This assumes that the SQLite file is memory-mapped or synchronised from edge nodes.

package org.prometheuspraxis.fog

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

data class FogRoutingDecisionUi(
    val timestampUtc: String,
    val nodeId: String,
    val previousV: Float,
    val currentV: Float,
    val verdict: String,
    val diagnosticOnly: Boolean,
    val evidenceHex: String,
)

class DiagnosticsViewModel(
    private val dao: FogRoutingDecisionDao,
) : ViewModel() {

    private val _decisions = MutableStateFlow<List<FogRoutingDecisionUi>>(emptyList())
    val decisions: StateFlow<List<FogRoutingDecisionUi>> = _decisions

    fun refreshDecisions() {
        viewModelScope.launch(Dispatchers.IO) {
            val rows = dao.getRecentDecisions()
            val ui = rows.map {
                FogRoutingDecisionUi(
                    timestampUtc = it.timestampUtc,
                    nodeId = it.nodeId,
                    previousV = it.previousV,
                    currentV = it.currentV,
                    verdict = it.verdict,
                    diagnosticOnly = it.diagnosticOnly,
                    evidenceHex = it.evidenceHex,
                )
            }
            _decisions.value = ui
        }
    }
}
