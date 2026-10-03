package com.eightcee.marioparty1recomp

import android.content.pm.ActivityInfo
import android.os.Build
import android.os.Bundle
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.ViewGroup
import android.view.WindowManager
import org.libsdl.app.SDLActivity
import java.io.File

class MainActivity : SDLActivity() {
    private var virtualPadView: VirtualPadView? = null

    override fun getLibraries() = arrayOf("main")

    override fun getArguments(): Array<String> = arrayOf(
        filesDir.absolutePath,
        File(filesDir, "roms/marioparty.us.z64").absolutePath
    )

    override fun onCreate(savedInstanceState: Bundle?) {
        requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            val attrs = window.attributes
            attrs.layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            window.attributes = attrs
        }

        super.onCreate(savedInstanceState)

        applyImmersiveMode()

        if (mLayout != null && virtualPadView == null) {
            virtualPadView = VirtualPadView(this)
            mLayout.addView(
                virtualPadView,
                ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT
                )
            )
        }

        RuntimeBridge.writeDiagnostic("MainActivity created")
    }

    override fun setOrientationBis(w: Int, h: Int, resizable: Boolean, hint: String?) {
        requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
    }

    override fun dispatchKeyEvent(e: KeyEvent): Boolean {
        if ((e.source and InputDevice.SOURCE_GAMEPAD) != 0 ||
            (e.source and InputDevice.SOURCE_JOYSTICK) != 0) {
            RuntimeBridge.writeDiagnostic("gamepad key=" + e.keyCode + " action=" + e.action)
        }
        return super.dispatchKeyEvent(e)
    }

    override fun dispatchGenericMotionEvent(e: MotionEvent): Boolean {
        return super.dispatchGenericMotionEvent(e)
    }

    override fun onResume() {
        super.onResume()
        applyImmersiveMode()
        RuntimeBridge.writeDiagnostic("MainActivity resumed")
    }

    override fun onPause() {
        RuntimeBridge.setVirtualPad(0, 0f, 0f)
        RuntimeBridge.writeDiagnostic("MainActivity paused; virtual pad released")
        super.onPause()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            applyImmersiveMode()
        } else {
            RuntimeBridge.setVirtualPad(0, 0f, 0f)
        }
    }

    @Suppress("DEPRECATION")
    private fun applyImmersiveMode() {
        window.decorView.systemUiVisibility =
            android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or
            android.view.View.SYSTEM_UI_FLAG_LAYOUT_STABLE or
            android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
            android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
            android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
            android.view.View.SYSTEM_UI_FLAG_FULLSCREEN
    }

    override fun onDestroy() {
        RuntimeBridge.setVirtualPad(0, 0f, 0f)
        virtualPadView = null
        super.onDestroy()
    }
}
