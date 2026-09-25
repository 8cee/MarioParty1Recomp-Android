package com.eightcee.marioparty1recomp

import android.app.Application
import com.eightcee.marioparty1recomp.diagnostics.Diagnostics

class MpApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        Diagnostics.init(this)
        Diagnostics.i("APP", "Application started")
        Diagnostics.i("APP", "version=" + BuildConfig.VERSION_NAME)
    }
}
