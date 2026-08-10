// File: android/app/src/main/java/org/prometheuspraxis/cyboquatic/WorkloadBridge.kt
package org.prometheuspraxis.cyboquatic

import android.database.Cursor

object WorkloadBridge {
    fun toWorkloadFrame(cursor: Cursor): WorkloadFrame =
        WorkloadFrame(
            canalNode = cursor.getString(cursor.getColumnIndexOrThrow("canal_node")),
            observedUtc = cursor.getString(cursor.getColumnIndexOrThrow("observed_utc")),
            energyReqJ = cursor.getDouble(cursor.getColumnIndexOrThrow("energyreq_j")),
            deltaVt = cursor.getDouble(cursor.getColumnIndexOrThrow("delta_vt")),
            ecoImpactValue = cursor.getDouble(cursor.getColumnIndexOrThrow("eco_impact_value")),
            accepted = cursor.getInt(cursor.getColumnIndexOrThrow("accepted")) == 1
        )
}
