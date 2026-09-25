package com.eightcee.marioparty1recomp

import android.net.Uri
import android.os.Bundle
import android.os.Build
import android.view.View
import android.view.WindowManager
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.InputDevice
import kotlin.math.abs
import android.provider.OpenableColumns
import android.view.Gravity
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import com.eightcee.marioparty1recomp.diagnostics.Diagnostics
import java.io.File
import java.io.FileOutputStream

class MainActivity : AppCompatActivity() {
    private lateinit var status: TextView
    private lateinit var launchButton: Button
    private var selectedRom: File? = null

    private val romPicker = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) importRom(uri)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Diagnostics.i("UI", "MainActivity created")
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            window.attributes = window.attributes.apply {
                layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            }
        }
        @Suppress("DEPRECATION")
        window.decorView.systemUiVisibility =
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or
            View.SYSTEM_UI_FLAG_FULLSCREEN or
            View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
            View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
            View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE

        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(48, 48, 48, 48)
            gravity = Gravity.CENTER_HORIZONTAL
        }

        layout.addView(TextView(this).apply {
            text = "Mario Party 1 Recomp - Android"
            textSize = 24f
        })

        status = TextView(this).apply {
            text = "No ROM selected.\nSelect your legally obtained Mario Party (USA) .z64 ROM."
            textSize = 17f
            setPadding(0, 30, 0, 30)
        }
        layout.addView(status)

        layout.addView(Button(this).apply {
            text = "Select Mario Party 1 ROM"
            setOnClickListener { romPicker.launch(arrayOf("application/octet-stream", "*/*")) }
        })

        launchButton = Button(this).apply {
            text = "Initialize Native Runtime"
            isEnabled = false
            setOnClickListener {
                val rom = selectedRom ?: return@setOnClickListener
                val result = RuntimeBridge.initialize(rom.absolutePath)
                Diagnostics.i("RUNTIME", result)
                status.text = result
                VirtualPadView.onGameStarted(result.startsWith("Native runtime initialized"))
            }
        }
        layout.addView(launchButton)

        layout.addView(Button(this).apply {
            text = "Export Diagnostic Log"
            setOnClickListener { Diagnostics.exportLog(this@MainActivity) }
        })

        val root = FrameLayout(this)
        root.addView(layout, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ))
        root.addView(VirtualPadView(this), FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ))
        setContentView(root)

        val cached = File(filesDir, "roms/marioparty.us.z64")
        if (cached.exists()) {
            selectedRom = cached
            launchButton.isEnabled = true
            status.text = "ROM ready: ${cached.name}\n${cached.length()} bytes"
        }
    }

    private fun importRom(uri: Uri) {
        try {
            val displayName = contentResolver.query(uri, null, null, null, null)?.use { c ->
                val i = c.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                if (i >= 0 && c.moveToFirst()) c.getString(i) else "selected.rom"
            } ?: "selected.rom"

            val dir = File(filesDir, "roms").apply { mkdirs() }
            val target = File(dir, "marioparty.us.z64")

            contentResolver.openInputStream(uri).use { input ->
                requireNotNull(input) { "Unable to open selected file" }
                FileOutputStream(target).use { out -> input.copyTo(out) }
            }

            val validation = RomValidator.validate(target)
            Diagnostics.i("ROM", "Selected $displayName: $validation")

            if (validation.valid) {
                selectedRom = target
                launchButton.isEnabled = true
                status.text = "ROM accepted: $displayName\n${validation.message}"
            } else {
                target.delete()
                selectedRom = null
                launchButton.isEnabled = false
                status.text = "ROM rejected: ${validation.message}"
            }
        } catch (t: Throwable) {
            Diagnostics.e("ROM", "ROM import failed", t)
            status.text = "ROM import failed: ${t.message}"
        }
    }
    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val source = event.source
        val isGamepad = (source and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
            (source and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
        if (!isGamepad) return super.dispatchKeyEvent(event)

        val id = when (event.keyCode) {
            KeyEvent.KEYCODE_BUTTON_A -> VirtualPadView.BTN_A
            KeyEvent.KEYCODE_BUTTON_B -> VirtualPadView.BTN_B
            KeyEvent.KEYCODE_BUTTON_L1 -> VirtualPadView.BTN_L
            KeyEvent.KEYCODE_BUTTON_R1 -> VirtualPadView.BTN_R
            KeyEvent.KEYCODE_BUTTON_L2 -> VirtualPadView.BTN_Z
            KeyEvent.KEYCODE_BUTTON_START -> VirtualPadView.BTN_START
            KeyEvent.KEYCODE_DPAD_UP -> VirtualPadView.BTN_DPAD_UP
            KeyEvent.KEYCODE_DPAD_DOWN -> VirtualPadView.BTN_DPAD_DOWN
            KeyEvent.KEYCODE_DPAD_LEFT -> VirtualPadView.BTN_DPAD_LEFT
            KeyEvent.KEYCODE_DPAD_RIGHT -> VirtualPadView.BTN_DPAD_RIGHT
            else -> -1
        }
        if (id < 0) return super.dispatchKeyEvent(event)

        val pressed = event.action == KeyEvent.ACTION_DOWN
        RuntimeBridge.setButton(id, pressed)
        if (event.repeatCount == 0) {
            Diagnostics.i("INPUT", "controller button id=$id pressed=$pressed device=${event.device?.name}")
        }
        return true
    }

    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        val isJoystick = (event.source and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
        if (!isJoystick || event.action != MotionEvent.ACTION_MOVE) {
            return super.onGenericMotionEvent(event)
        }

        fun axis(axis: Int): Float {
            val value = event.getAxisValue(axis)
            return if (abs(value) < 0.08f) 0f else value.coerceIn(-1f, 1f)
        }

        RuntimeBridge.setAxis(axis(MotionEvent.AXIS_X), -axis(MotionEvent.AXIS_Y))

        val cx = axis(MotionEvent.AXIS_Z)
        val cy = axis(MotionEvent.AXIS_RZ)
        RuntimeBridge.setButton(VirtualPadView.BTN_C_LEFT, cx < -0.5f)
        RuntimeBridge.setButton(VirtualPadView.BTN_C_RIGHT, cx > 0.5f)
        RuntimeBridge.setButton(VirtualPadView.BTN_C_UP, cy < -0.5f)
        RuntimeBridge.setButton(VirtualPadView.BTN_C_DOWN, cy > 0.5f)
        return true
    }
}
