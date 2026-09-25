package com.eightcee.marioparty1recomp

import android.app.Application
import com.eightcee.marioparty1recomp.diagnostics.Diagnostics

class MpApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        Diagnostics.init(this)
        Diagnostics.i("APP", "Application started")
        val versionName = try {
            packageManager.getPackageInfo(packageName, 0).versionName ?: "unknown"
        } catch (_: Exception) {
            "unknown"
        }
        Diagnostics.i("APP", "version=" + versionName)
    }
}
