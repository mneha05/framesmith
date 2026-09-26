package dev.neha.framesmith
import android.app.Activity
import android.os.Bundle
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.TextView
import android.widget.FrameLayout
import android.graphics.Color

class MainActivity : Activity(), SurfaceHolder.Callback {
    companion object { init { System.loadLibrary("framesmith") } }
    external fun nativeStart(surface: Surface)
    external fun nativeStop()
    external fun nativeStats(): String
    private lateinit var overlay: TextView
    override fun onCreate(savedInstanceState: Bundle?) { super.onCreate(savedInstanceState)
        val root=FrameLayout(this); val view=SurfaceView(this); view.holder.addCallback(this); root.addView(view)
        overlay=TextView(this).apply { setTextColor(Color.WHITE); textSize=14f; setPadding(28,28,28,28); setBackgroundColor(0x66000000) }
        root.addView(overlay); setContentView(root)
        overlay.post(object:Runnable{ override fun run(){ overlay.text=nativeStats(); overlay.postDelayed(this,250) } })
    }
    override fun surfaceCreated(h: SurfaceHolder) = nativeStart(h.surface)
    override fun surfaceChanged(h: SurfaceHolder, f:Int, w:Int, hh:Int) {}
    override fun surfaceDestroyed(h: SurfaceHolder) = nativeStop()
}
