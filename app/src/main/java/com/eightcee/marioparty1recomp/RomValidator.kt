package com.eightcee.marioparty1recomp

import java.io.File
import java.security.MessageDigest

object RomValidator {
    private const val EXPECTED_SIZE = 33_554_432L
    private const val EXPECTED_SHA1 = "1159bd56730094bfc71be30113e1cfc8bacf34f3"

    fun isSupported(file: File): Boolean {
        if (!file.isFile || file.length() != EXPECTED_SIZE) return false
        val digest = MessageDigest.getInstance("SHA-1")
        file.inputStream().use { input ->
            val buffer = ByteArray(1024 * 1024)
            while (true) {
                val n = input.read(buffer)
                if (n <= 0) break
                digest.update(buffer, 0, n)
            }
        }
        return digest.digest().joinToString("") { "%02x".format(it) } == EXPECTED_SHA1
    }
}
