package com.eightcee.marioparty1recomp

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import java.io.File

class RomImportActivity : Activity() {
    private val requestRom = 1001
    private lateinit var status: TextView
    private val romFile get() = File(filesDir, "roms/marioparty.us.z64")

    override fun onCreate(state: Bundle?) {
        super.onCreate(state)
        if (RomValidator.isSupported(romFile)) { launch(); return }
        status = TextView(this).apply { text = "Select your Mario Party (USA) ROM" }
        val button = Button(this).apply { text = "SELECT ROM"; setOnClickListener {
            startActivityForResult(Intent(Intent.ACTION_OPEN_DOCUMENT).apply { type = "application/octet-stream"; addCategory(Intent.CATEGORY_OPENABLE) }, requestRom)
        }}
        setContentView(LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(48,48,48,48); addView(status); addView(button) })
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != requestRom || resultCode != RESULT_OK || data?.data == null) return
        val dir = romFile.parentFile!!; dir.mkdirs()
        val tmp = File(dir, "marioparty.tmp")
        contentResolver.openInputStream(data.data!!)!!.use { input -> tmp.outputStream().use { input.copyTo(it) } }
        if (!RomValidator.isSupported(tmp)) { tmp.delete(); status.text = "Unsupported ROM. Mario Party (USA) is required."; return }
        if (romFile.exists()) romFile.delete()
        if (!tmp.renameTo(romFile)) { tmp.copyTo(romFile, overwrite=true); tmp.delete() }
        launch()
    }

    private fun launch() { startActivity(Intent(this, MainActivity::class.java)); finish() }
}
