// File: android/app/src/main/java/org/prometheuspraxis/cyboquatic/MainActivity.kt
package org.prometheuspraxis.cyboquatic

import android.app.Activity
import android.os.Bundle
import android.widget.LinearLayout
import android.widget.TextView

class MainActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val map = CanalStatusMapView(this)
        val status = TextView(this).apply { textSize = 18f }
        val frames = CanalStatusRepository(this).latestFrames()
        map.submitFrames(frames)
        status.text = if (frames.isEmpty()) {
            "No local cyboquatic workload telemetry is available."
        } else {
            "${frames.count { it.accepted }} of ${frames.size} canal nodes are admissible."
        }

        setContentView(LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            addView(status)
            addView(map, LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                0,
                1f
            ))
        })
    }
}
