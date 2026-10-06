package com.eightcee.marioparty1recomp

import java.io.File

/**
 * Owns the generated Mario Party asset archive lifecycle.
 * The archive must be produced by the MP1 extractor from a verified user ROM.
 * Never treat the ROM itself as an O2R archive.
 */
object AssetArchive {
    private const val MIN_ARCHIVE_BYTES = 1024L

    fun isReady(file: File): Boolean =
        file.isFile && file.length() >= MIN_ARCHIVE_BYTES
}
