package com.eightcee.marioparty1recomp

import android.os.Bundle
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.ViewGroup
import org.libsdl.app.SDLActivity
import java.io.File

class MainActivity : SDLActivity() {
    override fun getLibraries() = arrayOf("main")
    override fun getArguments(): Array<String> = arrayOf(filesDir.absolutePath, File(filesDir,"roms/marioparty.us.z64").absolutePath)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.decorView.systemUiVisibility = 5894
        addContentView(VirtualPadView(this), ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
    }
    override fun dispatchKeyEvent(e: KeyEvent): Boolean {
        if ((e.source and InputDevice.SOURCE_GAMEPAD) != 0 || (e.source and InputDevice.SOURCE_JOYSTICK) != 0) {
            RuntimeBridge.writeDiagnostic("gamepad key="+e.keyCode+" action="+e.action)
        }
        return super.dispatchKeyEvent(e)
    }
    override fun dispatchGenericMotionEvent(e: MotionEvent): Boolean {
        if ((e.source and InputDevice.SOURCE_JOYSTICK) != 0) return super.dispatchGenericMotionEvent(e)
        return super.dispatchGenericMotionEvent(e)
    }
}
