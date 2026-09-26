package dev.neha.framesmith

import android.app.Activity
import android.content.res.AssetManager
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.FrameLayout
import android.widget.TextView

class MainActivity : Activity(), SurfaceHolder.Callback {
    companion object {
        init { System.loadLibrary("framesmith") }
    }

    external fun nativeStart(surface: Surface, assets: AssetManager): Boolean
    external fun nativeStop()
    external fun nativeStats(): String

    private lateinit var overlay: TextView
    private var overlayRunning = false

    private val overlayTick = object : Runnable {
        override fun run() {
            if (!overlayRunning) return
            overlay.text = nativeStats()
            overlay.postDelayed(this, 250)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = FrameLayout(this)
        val surface = SurfaceView(this)
        surface.holder.addCallback(this)
        root.addView(
            surface,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
            )
        )

        overlay = TextView(this).apply {
            setTextColor(Color.WHITE)
            textSize = 14f
            typeface = android.graphics.Typeface.MONOSPACE
            setPadding(28, 20, 28, 20)
            setBackgroundColor(0xAA080B12.toInt())
        }
        val overlayParams = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT
        ).apply {
            gravity = Gravity.TOP or Gravity.START
            leftMargin = 22
            topMargin = 22
        }
        root.addView(overlay, overlayParams)
        setContentView(root)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        overlayRunning = nativeStart(holder.surface, assets)
        if (overlayRunning) overlay.post(overlayTick)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        // Vulkan receives OUT_OF_DATE/SUBOPTIMAL during rotation/resize and
        // recreates the swapchain on the render thread.
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        overlayRunning = false
        overlay.removeCallbacks(overlayTick)
        nativeStop()
    }

    override fun onDestroy() {
        overlayRunning = false
        overlay.removeCallbacks(overlayTick)
        nativeStop()
        super.onDestroy()
    }
}
