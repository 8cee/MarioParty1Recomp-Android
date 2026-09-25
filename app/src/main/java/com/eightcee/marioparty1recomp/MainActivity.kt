package com.eightcee.marioparty1recomp

import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import android.view.Gravity
import android.widget.Button
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
            }
        }
        layout.addView(launchButton)

        layout.addView(Button(this).apply {
            text = "Export Diagnostic Log"
            setOnClickListener { Diagnostics.exportLog(this@MainActivity) }
        })

        setContentView(layout)

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
}
