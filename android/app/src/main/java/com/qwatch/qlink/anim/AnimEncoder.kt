package com.qwatch.qlink.anim

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Encodes Bitmaps and image sequences into the Q-Watch .anim binary format.
 * Features:
 * - Aspect ratio preserving scaling to 128x64 with letterboxing
 * - 2D spatial Floyd-Steinberg error-diffusion dithering for authentic retro OLED textures
 * - Configurable threshold and color inversion controls
 * - Bit-packing into 1024-byte XBM row-major frames
 * - 16-byte QANM header generation
 */
object AnimEncoder {

    /**
     * Converts a single Bitmap into a 128x64 1-bit monochrome XBM frame (1024 bytes).
     */
    fun encodeBitmapToFrame(
        src: Bitmap,
        useDithering: Boolean = true,
        threshold: Float = 128f,
        invert: Boolean = false
    ): ByteArray {
        val scaled = Bitmap.createBitmap(AnimParser.ANIM_WIDTH, AnimParser.ANIM_HEIGHT, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(scaled)
        canvas.drawColor(Color.BLACK)

        // Calculate aspect ratio fit with letterboxing
        val srcW = src.width.toFloat()
        val srcH = src.height.toFloat()
        val scale = (AnimParser.ANIM_WIDTH / srcW).coerceAtMost(AnimParser.ANIM_HEIGHT / srcH)
        val destW = (srcW * scale).toInt()
        val destH = (srcH * scale).toInt()
        val offsetX = (AnimParser.ANIM_WIDTH - destW) / 2
        val offsetY = (AnimParser.ANIM_HEIGHT - destH) / 2

        val paint = Paint(Paint.FILTER_BITMAP_FLAG)
        canvas.drawBitmap(src, null, Rect(offsetX, offsetY, offsetX + destW, offsetY + destH), paint)

        // Convert to grayscale luminance matrix
        val gray = Array(AnimParser.ANIM_HEIGHT) { FloatArray(AnimParser.ANIM_WIDTH) }
        for (y in 0 until AnimParser.ANIM_HEIGHT) {
            for (x in 0 until AnimParser.ANIM_WIDTH) {
                val pixel = scaled.getPixel(x, y)
                val r = Color.red(pixel)
                val g = Color.green(pixel)
                val b = Color.blue(pixel)
                // Rec. 601 luma formula
                gray[y][x] = (0.299f * r + 0.587f * g + 0.114f * b)
            }
        }

        // Binary bitmap matrix
        val mono = Array(AnimParser.ANIM_HEIGHT) { BooleanArray(AnimParser.ANIM_WIDTH) }

        if (useDithering) {
            // Floyd-Steinberg error diffusion
            for (y in 0 until AnimParser.ANIM_HEIGHT) {
                for (x in 0 until AnimParser.ANIM_WIDTH) {
                    val oldVal = gray[y][x].coerceIn(0f, 255f)
                    val newVal = if (oldVal >= threshold) 255f else 0f
                    mono[y][x] = if (invert) (newVal == 0f) else (newVal == 255f)
                    val err = oldVal - newVal

                    if (x + 1 < AnimParser.ANIM_WIDTH) {
                        gray[y][x + 1] += err * (7f / 16f)
                    }
                    if (y + 1 < AnimParser.ANIM_HEIGHT) {
                        if (x - 1 >= 0) {
                            gray[y + 1][x - 1] += err * (3f / 16f)
                        }
                        gray[y + 1][x] += err * (5f / 16f)
                        if (x + 1 < AnimParser.ANIM_WIDTH) {
                            gray[y + 1][x + 1] += err * (1f / 16f)
                        }
                    }
                }
            }
        } else {
            // Pure thresholding
            for (y in 0 until AnimParser.ANIM_HEIGHT) {
                for (x in 0 until AnimParser.ANIM_WIDTH) {
                    val isSet = gray[y][x] >= threshold
                    mono[y][x] = if (invert) !isSet else isSet
                }
            }
        }

        // Pack into 1024 bytes XBM (16 bytes per row, bit 0 is leftmost pixel)
        val frameBytes = ByteArray(AnimParser.ANIM_FRAME_BYTES)
        for (y in 0 until AnimParser.ANIM_HEIGHT) {
            val rowOffset = y * 16
            for (x in 0 until AnimParser.ANIM_WIDTH) {
                if (mono[y][x]) {
                    val byteIdx = rowOffset + (x shr 3)
                    val bitIdx = x and 7
                    frameBytes[byteIdx] = (frameBytes[byteIdx].toInt() or (1 shl bitIdx)).toByte()
                }
            }
        }

        return frameBytes
    }

    /**
     * Builds a full .anim binary byte array from a sequence of Bitmaps and target FPS/delay.
     */
    fun encodeAnimation(
        bitmaps: List<Bitmap>,
        delayMs: Int = 50,
        loop: Boolean = true,
        useDithering: Boolean = true,
        threshold: Float = 128f,
        invert: Boolean = false
    ): ByteArray {
        val frameCount = bitmaps.size.coerceAtLeast(1)
        val totalBytes = AnimParser.ANIM_HEADER_SIZE + (frameCount * AnimParser.ANIM_FRAME_BYTES)
        val buffer = ByteBuffer.allocate(totalBytes).order(ByteOrder.LITTLE_ENDIAN)

        // 16-byte Header
        buffer.putInt(AnimParser.ANIM_MAGIC.toInt()) // 0x4D4E4151
        buffer.putShort(1.toShort())                 // Version 1
        buffer.put(AnimParser.ANIM_WIDTH.toByte())   // 128
        buffer.put(AnimParser.ANIM_HEIGHT.toByte())  // 64
        buffer.putShort(frameCount.toShort())        // Frame Count
        buffer.putShort(delayMs.toShort())           // Frame Delay ms
        val flags = (if (loop) 0x0001 else 0) or (if (invert) 0x0002 else 0)
        buffer.putShort(flags.toShort())             // Flags
        buffer.putShort(0.toShort())                 // Reserved

        // Encode each frame
        for (bmp in bitmaps) {
            val frameData = encodeBitmapToFrame(bmp, useDithering, threshold, invert)
            buffer.put(frameData)
        }

        return buffer.array()
    }
}
