// File: android/app/src/main/java/org/prometheuspraxis/cyboquatic/CanalStatusMapView.kt
package org.prometheuspraxis.cyboquatic

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.view.View

class CanalStatusMapView(context: Context) : View(context) {
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private var frames: List<WorkloadFrame> = emptyList()

    fun submitFrames(values: List<WorkloadFrame>) {
        frames = values
        contentDescription = "Canal-node ecological status map with ${values.size} nodes"
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        canvas.drawColor(Color.rgb(239, 248, 240))
        if (frames.isEmpty()) return

        val spacing = width.toFloat() / (frames.size + 1)
        frames.forEachIndexed { index, frame ->
            val x = spacing * (index + 1)
            val y = height * 0.50f
            paint.color = when {
                !frame.accepted -> Color.rgb(183, 44, 44)
                frame.ecoImpactValue >= 0.85 -> Color.rgb(31, 121, 65)
                else -> Color.rgb(204, 143, 22)
            }
            canvas.drawCircle(x, y, 28f, paint)
            paint.color = Color.DKGRAY
            paint.textSize = 26f
            paint.textAlign = Paint.Align.CENTER
            canvas.drawText(frame.canalNode, x, y + 62f, paint)
            canvas.drawText(
                "E %.2f ΔV %.2f".format(frame.ecoImpactValue, frame.deltaVt),
                x,
                y + 92f,
                paint
            )
        }
    }
}
