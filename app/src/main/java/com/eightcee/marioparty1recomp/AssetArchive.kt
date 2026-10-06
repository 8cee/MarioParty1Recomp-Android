package com.eightcee.marioparty1recomp

import java.io.File

/**
 * Lifecycle for assets generated from the user's verified Mario Party (USA) ROM.
 *
 * A non-empty file is not enough: extraction must finish atomically and write a
 * version stamp. This prevents interrupted/old extraction output from being
 * offered to the game as a valid O2R archive.
 */
object AssetArchive {
    const val ARCHIVE_NAME = "marioparty.o2r"
    const val RECIPE_VERSION = 1
    private const val STAMP_NAME = "marioparty.o2r.version"
    private const val MIN_ARCHIVE_BYTES = 1024L

    enum class State {
        MISSING,
        STALE,
        READY
    }

    fun state(archive: File): State {
        if (!archive.isFile || archive.length() < MIN_ARCHIVE_BYTES) return State.MISSING
        val stamp = File(archive.parentFile, STAMP_NAME)
        val version = stamp.takeIf { it.isFile }?.readText()?.trim()?.toIntOrNull()
        return if (version == RECIPE_VERSION) State.READY else State.STALE
    }

    fun isReady(archive: File): Boolean = state(archive) == State.READY

    /**
     * Called only after the native extractor has successfully closed its output.
     * The archive should be written to a temporary file and renamed before this.
     */
    fun markReady(archive: File) {
        require(archive.isFile && archive.length() >= MIN_ARCHIVE_BYTES)
        archive.parentFile?.mkdirs()
        File(archive.parentFile, STAMP_NAME).writeText(RECIPE_VERSION.toString())
    }

    fun invalidate(archive: File) {
        File(archive.parentFile, STAMP_NAME).delete()
    }
}
