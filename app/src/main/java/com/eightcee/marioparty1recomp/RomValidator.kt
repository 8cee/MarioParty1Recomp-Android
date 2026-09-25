package com.eightcee.marioparty1recomp

import java.io.File
import java.io.FileInputStream

data class RomValidation(
    val valid: Boolean,
    val message: String,
    val title: String = "",
    val gameCode: String = ""
)

object RomValidator {
    fun validate(file: File): RomValidation {
        if (!file.exists()) return RomValidation(false, "File not found")
        val size = file.length()
        if (size < 4L * 1024 * 1024) return RomValidation(false, "File is too small to be a valid N64 ROM")

        val header = ByteArray(0x40)
        FileInputStream(file).use {
            if (it.read(header) != header.size) return RomValidation(false, "Could not read the N64 header")
        }

        val magic = header.copyOfRange(0, 4).joinToString("") { "%02X".format(it) }
        if (magic != "80371240") {
            return when (magic) {
                "37804012" -> RomValidation(false, "Byte-swapped .v64 ROM detected. Please use the clean US .z64 ROM.")
                "40123780" -> RomValidation(false, "Little-endian .n64 ROM detected. Please use the clean US .z64 ROM.")
                else -> RomValidation(false, "Unknown N64 ROM header: $magic")
            }
        }

        val title = header.copyOfRange(0x20, 0x34)
            .toString(Charsets.US_ASCII)
            .trimEnd('\u0000', ' ')
        val gameCode = header.copyOfRange(0x3B, 0x3F)
            .toString(Charsets.US_ASCII)
            .trimEnd('\u0000', ' ')

        val titleLooksRight = title.uppercase().contains("MARIO PARTY")
        val usaRegion = gameCode.endsWith("E")

        if (!titleLooksRight) {
            return RomValidation(false, "This does not look like Mario Party 1. ROM title: '$title'", title, gameCode)
        }
        if (!usaRegion) {
            return RomValidation(false, "Mario Party detected, but this build requires the US/NTSC-U ROM. Game code: $gameCode", title, gameCode)
        }

        return RomValidation(
            true,
            "Mario Party US .z64 detected — $size bytes, title '$title', game code $gameCode",
            title,
            gameCode
        )
    }
}
