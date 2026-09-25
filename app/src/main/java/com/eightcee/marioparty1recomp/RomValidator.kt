package com.eightcee.marioparty1recomp

import java.io.File
import java.io.FileInputStream

data class RomValidation(val valid: Boolean, val message: String)

object RomValidator {
    fun validate(file: File): RomValidation {
        if (!file.exists()) return RomValidation(false, "File not found")
        val size = file.length()
        if (size < 4L * 1024 * 1024) return RomValidation(false, "File is too small to be a valid N64 ROM")

        val header = ByteArray(4)
        FileInputStream(file).use {
            if (it.read(header) != 4) return RomValidation(false, "Could not read ROM header")
        }

        val magic = header.joinToString("") { "%02X".format(it) }
        return when (magic) {
            "80371240" -> RomValidation(true, "Big-endian .z64 format detected ($size bytes)")
            "37804012" -> RomValidation(false, "Byte-swapped .v64 ROM detected. Please use a clean .z64 ROM.")
            "40123780" -> RomValidation(false, "Little-endian .n64 ROM detected. Please use a clean .z64 ROM.")
            else -> RomValidation(false, "Unknown N64 ROM header: $magic")
        }
    }
}
