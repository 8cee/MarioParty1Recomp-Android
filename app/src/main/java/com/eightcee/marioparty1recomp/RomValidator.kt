package com.eightcee.marioparty1recomp

import java.io.File
import java.io.FileInputStream
import java.security.MessageDigest

data class RomValidation(
    val valid: Boolean,
    val message: String,
    val title: String = "",
    val gameCode: String = "",
    val sha1: String = ""
)

object RomValidator {
    // Clean Mario Party (USA) big-endian .z64 dump validated from the user's ROM.
    private const val EXPECTED_SIZE = 33_554_432L
    private const val EXPECTED_SHA1 = "1159bd56730094bfc71be30113e1cfc8bacf34f3"

    fun validate(file: File): RomValidation {
        if (!file.exists()) return RomValidation(false, "File not found")
        val size = file.length()
        if (size != EXPECTED_SIZE) {
            return RomValidation(false, "Wrong ROM size: $size bytes. Expected $EXPECTED_SIZE bytes.")
        }

        val header = ByteArray(0x40)
        FileInputStream(file).use {
            if (it.read(header) != header.size) return RomValidation(false, "Could not read the N64 header")
        }

        val magic = header.copyOfRange(0, 4).joinToString("") { "%02X".format(it) }
        if (magic != "80371240") {
            return when (magic) {
                "37804012" -> RomValidation(false, "Byte-swapped .v64 ROM detected. Use the clean US .z64 ROM.")
                "40123780" -> RomValidation(false, "Little-endian .n64 ROM detected. Use the clean US .z64 ROM.")
                else -> RomValidation(false, "Unknown N64 ROM header: $magic")
            }
        }

        val title = header.copyOfRange(0x20, 0x34)
            .toString(Charsets.US_ASCII)
            .trimEnd('\u0000', ' ')
        val gameCode = header.copyOfRange(0x3B, 0x3F)
            .toString(Charsets.US_ASCII)
            .trimEnd('\u0000', ' ')

        if (!title.uppercase().contains("MARIOPARTY")) {
            return RomValidation(false, "This does not look like Mario Party 1. ROM title: '$title'", title, gameCode)
        }
        if (gameCode != "CLBE") {
            return RomValidation(false, "Wrong Mario Party region/revision. Expected game code CLBE, got $gameCode.", title, gameCode)
        }

        val sha1 = digest(file, "SHA-1")
        if (!sha1.equals(EXPECTED_SHA1, ignoreCase = true)) {
            return RomValidation(
                false,
                "Mario Party US ROM detected, but checksum does not match the clean supported dump. SHA-1: $sha1",
                title,
                gameCode,
                sha1
            )
        }

        return RomValidation(
            true,
            "Exact supported Mario Party (USA) .z64 verified. SHA-1 $sha1",
            title,
            gameCode,
            sha1
        )
    }

    private fun digest(file: File, algorithm: String): String {
        val md = MessageDigest.getInstance(algorithm)
        FileInputStream(file).use { input ->
            val buffer = ByteArray(1024 * 1024)
            while (true) {
                val read = input.read(buffer)
                if (read <= 0) break
                md.update(buffer, 0, read)
            }
        }
        return md.digest().joinToString("") { "%02x".format(it) }
    }
}
