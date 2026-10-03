package com.qwatch.qlink.anim

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Color
import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * High-performance parser and decoder for Q-Watch binary animation (.anim) assets.
 */
object AnimParser {

    const val ANIM_MAGIC = 0x4D4E4151L // "QANM"
    const val ANIM_HEADER_SIZE = 16
    const val ANIM_WIDTH = 128
    const val ANIM_HEIGHT = 64
    const val ANIM_FRAME_BYTES = 1024 // (128 * 64) / 8

    /**
     * Comprehensive validation of a QANM binary animation buffer.
     * Verifies 16-byte header magic, dimensions, version, frame count, and payload boundary.
     */
    fun validateQanm(bytes: ByteArray): AnimValidationResult {
        if (bytes.size < ANIM_HEADER_SIZE) {
            return AnimValidationResult(false, "File smaller than 16-byte header (${bytes.size}B)")
        }

        val buffer = ByteBuffer.wrap(bytes, 0, ANIM_HEADER_SIZE).order(ByteOrder.LITTLE_ENDIAN)
        val magic = buffer.int.toLong() and 0xFFFFFFFFL
        if (magic != ANIM_MAGIC) {
            return AnimValidationResult(false, "Invalid QANM magic 0x${magic.toString(16).uppercase()} (expected 0x4D4E4151)")
        }

        val version = buffer.short.toInt() and 0xFFFF
        if (version != 1) {
            return AnimValidationResult(false, "Unsupported QANM version $version (expected v1)")
        }

        val width = buffer.get().toInt() and 0xFF
        val height = buffer.get().toInt() and 0xFF
        if (width != ANIM_WIDTH || height != ANIM_HEIGHT) {
            return AnimValidationResult(false, "Invalid dimensions ${width}x${height} (expected ${ANIM_WIDTH}x${ANIM_HEIGHT})")
        }

        val frameCount = buffer.short.toInt() and 0xFFFF
        if (frameCount == 0) {
            return AnimValidationResult(false, "Animation contains 0 frames")
        }

        val frameDelayMs = buffer.short.toInt() and 0xFFFF
        val flags = buffer.short.toInt() and 0xFFFF

        val expectedSize = ANIM_HEADER_SIZE + (frameCount * ANIM_FRAME_BYTES)
        if (bytes.size != expectedSize) {
            return AnimValidationResult(
                false,
                "Size mismatch: expected $expectedSize bytes for $frameCount frames, got ${bytes.size} bytes"
            )
        }

        val header = AnimHeader(
            magic = magic,
            version = version,
            width = width,
            height = height,
            frameCount = frameCount,
            frameDelayMs = if (frameDelayMs > 0) frameDelayMs else 50,
            flags = flags
        )
        return AnimValidationResult(true, null, header)
    }

    /**
     * Parses the 16-byte binary header from an animation file.
     */
    fun parseHeader(bytes: ByteArray): AnimHeader? {
        val result = validateQanm(bytes)
        return if (result.isValid) result.header else null
    }

    /**
     * Parses an entire .anim binary into an AnimFile containing all 1024-byte frames.
     */
    fun parseAnim(name: String, bytes: ByteArray): AnimFile? {
        val header = parseHeader(bytes) ?: return null
        val totalExpected = ANIM_HEADER_SIZE + (header.frameCount * ANIM_FRAME_BYTES)
        if (bytes.size < totalExpected) return null

        val frames = mutableListOf<ByteArray>()
        var offset = ANIM_HEADER_SIZE
        for (i in 0 until header.frameCount) {
            val frame = bytes.copyOfRange(offset, offset + ANIM_FRAME_BYTES)
            frames.add(frame)
            offset += ANIM_FRAME_BYTES
        }

        return AnimFile(
            name = name,
            header = header,
            frames = frames,
            totalBytes = bytes.size.toLong()
        )
    }

    /**
     * Loads and parses an animation file from Android app assets.
     */
    fun loadFromAssets(context: Context, assetPath: String): AnimFile? {
        return try {
            context.assets.open(assetPath).use { input: InputStream ->
                val bytes = input.readBytes()
                val filename = assetPath.substringAfterLast('/')
                parseAnim(filename, bytes)
            }
        } catch (_: Exception) {
            null
        }
    }

    /**
     * Unpacks 1024-byte XBM row-major frame into an 8192-element BooleanArray (true = ON, false = OFF).
     * Format: 64 rows of 16 bytes each. Within each byte, bit 0 is leftmost pixel (x % 8 == 0).
     */
    fun decodeFramePixels(frameData: ByteArray): BooleanArray {
        val pixels = BooleanArray(ANIM_WIDTH * ANIM_HEIGHT)
        if (frameData.size < ANIM_FRAME_BYTES) return pixels

        for (y in 0 until ANIM_HEIGHT) {
            val rowOffset = y * 16
            val pixelRowOffset = y * ANIM_WIDTH
            for (x in 0 until ANIM_WIDTH) {
                val byteIndex = rowOffset + (x shr 3)
                val bitIndex = x and 7
                val isSet = ((frameData[byteIndex].toInt() and 0xFF) and (1 shl bitIndex)) != 0
                pixels[pixelRowOffset + x] = isSet
            }
        }
        return pixels
    }

    /**
     * Decodes a 1024-byte frame into an Android ARGB_8888 Bitmap for high-performance rendering.
     */
    fun decodeFrameToBitmap(
        frameData: ByteArray,
        onColor: Int = Color.WHITE,
        offColor: Int = Color.BLACK
    ): Bitmap {
        val bmp = Bitmap.createBitmap(ANIM_WIDTH, ANIM_HEIGHT, Bitmap.Config.ARGB_8888)
        if (frameData.size < ANIM_FRAME_BYTES) return bmp

        val pixelArray = IntArray(ANIM_WIDTH * ANIM_HEIGHT)
        for (y in 0 until ANIM_HEIGHT) {
            val rowOffset = y * 16
            val pixelRowOffset = y * ANIM_WIDTH
            for (x in 0 until ANIM_WIDTH) {
                val byteIndex = rowOffset + (x shr 3)
                val bitIndex = x and 7
                val isSet = ((frameData[byteIndex].toInt() and 0xFF) and (1 shl bitIndex)) != 0
                pixelArray[pixelRowOffset + x] = if (isSet) onColor else offColor
            }
        }
        bmp.setPixels(pixelArray, 0, ANIM_WIDTH, 0, 0, ANIM_WIDTH, ANIM_HEIGHT)
        return bmp
    }
}
