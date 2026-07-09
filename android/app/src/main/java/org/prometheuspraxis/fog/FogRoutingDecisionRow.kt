// Filename: android/app/src/main/java/org/prometheuspraxis/fog/FogRoutingDecisionRow.kt

package org.prometheuspraxis.fog

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.PrimaryKey

@Entity(tableName = "fog_routing_decisions")
data class FogRoutingDecisionRow(
    @PrimaryKey(autoGenerate = true)
    val id: Long = 0L,
    @ColumnInfo(name = "timestamp_utc")
    val timestampUtc: String,
    @ColumnInfo(name = "node_id")
    val nodeId: String,
    @ColumnInfo(name = "previous_v")
    val previousV: Float,
    @ColumnInfo(name = "current_v")
    val currentV: Float,
    @ColumnInfo(name = "verdict")
    val verdict: String,
    @ColumnInfo(name = "diagnostic_only")
    val diagnosticOnly: Boolean,
    @ColumnInfo(name = "evidence_hex")
    val evidenceHex: String,
)
