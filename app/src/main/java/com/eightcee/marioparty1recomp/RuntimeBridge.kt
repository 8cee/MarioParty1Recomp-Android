package com.eightcee.marioparty1recomp

object RuntimeBridge {
    init { System.loadLibrary("main") }
    external fun setVirtualPad(buttons: Int, stickX: Float, stickY: Float)
    external fun writeDiagnostic(message: String)
}
