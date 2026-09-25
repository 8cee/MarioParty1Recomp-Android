package com.eightcee.marioparty1recomp

object RuntimeBridge {
    init {
        System.loadLibrary("mp1runtime")
    }

    external fun initialize(romPath: String): String
    external fun setButton(id: Int, pressed: Boolean)
    external fun setAxis(x: Float, y: Float)
}
