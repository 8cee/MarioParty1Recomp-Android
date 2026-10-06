package com.eightcee.marioparty1recomp

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.content.FileProvider
import java.io.File

class RomImportActivity : Activity() {
    private val requestRom = 1001
    private lateinit var status: TextView
    private val romFile get() = File(filesDir, "roms/marioparty.us.z64")
    private val assetArchive get() = File(filesDir, "assets/marioparty.o2r")

    override fun onCreate(state: Bundle?) {
        super.onCreate(state)
        if (RomValidator.isSupported(romFile)) {
            showReadyScreen()
            return
        }
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
        try {
            val input = contentResolver.openInputStream(data.data!!)
                ?: throw java.io.IOException("Could not open selected ROM")
            input.use { source -> tmp.outputStream().use { source.copyTo(it) } }
            if (!RomValidator.isSupported(tmp)) {
                tmp.delete()
                status.text = "Unsupported ROM. Mario Party (USA) is required."
                return
            }
        } catch (e: Exception) {
            tmp.delete()
            status.text = "ROM import failed: " + (e.message ?: e.javaClass.simpleName)
            return
        }
        if (romFile.exists()) romFile.delete()
        if (!tmp.renameTo(romFile)) { tmp.copyTo(romFile, overwrite=true); tmp.delete() }
        showReadyScreen()
    }

    private fun showReadyScreen() {
        val archiveReady = AssetArchive.isReady(assetArchive)
        status = TextView(this).apply {
            text = if (archiveReady) "Mario Party assets ready"
                   else "Mario Party (USA) ROM ready — asset extraction required"
        }
        val play = Button(this).apply {
            text = if (archiveReady) "PLAY" else "PLAY (RAW ROM)"
            setOnClickListener { launch() }
        }
        val buildAssets = Button(this).apply {
            text = "BUILD GAME ASSETS"
            isEnabled = !archiveReady
            setOnClickListener {
                status.text = "MP1 O2R extractor is not installed in this build yet."
            }
        }
        val diagnostics = diagnosticButton()
        setContentView(LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(48, 48, 48, 48)
            addView(status)
            addView(play)
            addView(buildAssets)
            addView(diagnostics)
        })
    }

    private fun diagnosticButton() = Button(this).apply {
        text = "SHARE DIAGNOSTICS"
        isEnabled = diagnosticFile().isFile
        setOnClickListener { shareDiagnostics() }
    }

    private fun diagnosticFile() = File(filesDir, "mp1-diagnostic.log")

    private fun shareDiagnostics() {
        val log = diagnosticFile()
        if (!log.isFile) {
            status.text = "No diagnostic log exists yet."
            return
        }
        val uri = FileProvider.getUriForFile(this, "${packageName}.files", log)
        startActivity(Intent.createChooser(Intent(Intent.ACTION_SEND).apply {
            type = "text/plain"
            putExtra(Intent.EXTRA_STREAM, uri)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }, "Share Mario Party diagnostics"))
    }

    private fun launch() { startActivity(Intent(this, MainActivity::class.java)) }
}
