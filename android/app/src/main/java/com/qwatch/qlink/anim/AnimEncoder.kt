package com.qwatch.qlink.anim

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.cos
import kotlin.math.sin

/**
 * High-performance encoder, dithering processor, GIF decoder, and procedural generator
 * for the Q-Watch .anim binary format.
 */
object AnimEncoder {

    private val BAYER_4X4 = arrayOf(
        floatArrayOf(0f, 8f, 2f, 10f),
        floatArrayOf(12f, 4f, 14f, 6f),
        floatArrayOf(3f, 11f, 1f, 9f),
        floatArrayOf(15f, 7f, 13f, 5f)
    )

    /**
     * Packs an 8192-element BooleanArray (128x64) directly into a 1024-byte XBM row-major frame.
     */
    fun encodePixelsToFrame(pixels: BooleanArray, invert: Boolean = false): ByteArray {
        val frameBytes = ByteArray(AnimParser.ANIM_FRAME_BYTES)
        val w = AnimParser.ANIM_WIDTH
        val h = AnimParser.ANIM_HEIGHT

        for (y in 0 until h) {
            val rowOffset = y * 16
            val pixelRowOffset = y * w
            for (x in 0 until w) {
                var isSet = pixels[pixelRowOffset + x]
                if (invert) isSet = !isSet
                if (isSet) {
                    val byteIdx = rowOffset + (x shr 3)
                    val bitIdx = x and 7
                    frameBytes[byteIdx] = (frameBytes[byteIdx].toInt() or (1 shl bitIdx)).toByte()
                }
            }
        }
        return frameBytes
    }

    /**
     * Converts a single Bitmap into a 128x64 1-bit monochrome XBM frame (1024 bytes).
     */
    fun encodeBitmapToFrame(
        src: Bitmap,
        useDithering: Boolean = true,
        threshold: Float = 128f,
        invert: Boolean = false,
        ditherMode: DitherMode = if (useDithering) DitherMode.FLOYD_STEINBERG else DitherMode.THRESHOLD,
        fitMode: AspectFitMode = AspectFitMode.FIT
    ): ByteArray {
        val studioFrame = decodeBitmapToStudioFrame(src, ditherMode, threshold, invert, fitMode)
        return encodePixelsToFrame(studioFrame.pixels)
    }

    /**
     * Converts a single Bitmap into an editable StudioFrame with dithering and aspect ratio controls.
     */
    fun decodeBitmapToStudioFrame(
        src: Bitmap,
        ditherMode: DitherMode = DitherMode.FLOYD_STEINBERG,
        threshold: Float = 128f,
        invert: Boolean = false,
        fitMode: AspectFitMode = AspectFitMode.FIT
    ): StudioFrame {
        val scaled = Bitmap.createBitmap(AnimParser.ANIM_WIDTH, AnimParser.ANIM_HEIGHT, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(scaled)
        canvas.drawColor(Color.BLACK)

        val srcW = src.width.toFloat()
        val srcH = src.height.toFloat()

        when (fitMode) {
            AspectFitMode.STRETCH -> {
                canvas.drawBitmap(src, null, Rect(0, 0, AnimParser.ANIM_WIDTH, AnimParser.ANIM_HEIGHT), Paint(Paint.FILTER_BITMAP_FLAG))
            }
            AspectFitMode.CROP -> {
                val scale = (AnimParser.ANIM_WIDTH / srcW).coerceAtLeast(AnimParser.ANIM_HEIGHT / srcH)
                val destW = (srcW * scale).toInt()
                val destH = (srcH * scale).toInt()
                val offsetX = (AnimParser.ANIM_WIDTH - destW) / 2
                val offsetY = (AnimParser.ANIM_HEIGHT - destH) / 2
                canvas.drawBitmap(src, null, Rect(offsetX, offsetY, offsetX + destW, offsetY + destH), Paint(Paint.FILTER_BITMAP_FLAG))
            }
            AspectFitMode.FIT -> {
                val scale = (AnimParser.ANIM_WIDTH / srcW).coerceAtMost(AnimParser.ANIM_HEIGHT / srcH)
                val destW = (srcW * scale).toInt()
                val destH = (srcH * scale).toInt()
                val offsetX = (AnimParser.ANIM_WIDTH - destW) / 2
                val offsetY = (AnimParser.ANIM_HEIGHT - destH) / 2
                canvas.drawBitmap(src, null, Rect(offsetX, offsetY, offsetX + destW, offsetY + destH), Paint(Paint.FILTER_BITMAP_FLAG))
            }
        }

        val gray = Array(AnimParser.ANIM_HEIGHT) { FloatArray(AnimParser.ANIM_WIDTH) }
        for (y in 0 until AnimParser.ANIM_HEIGHT) {
            for (x in 0 until AnimParser.ANIM_WIDTH) {
                val pixel = scaled.getPixel(x, y)
                val r = Color.red(pixel)
                val g = Color.green(pixel)
                val b = Color.blue(pixel)
                gray[y][x] = (0.299f * r + 0.587f * g + 0.114f * b)
            }
        }

        val frame = StudioFrame()

        when (ditherMode) {
            DitherMode.THRESHOLD -> {
                for (y in 0 until AnimParser.ANIM_HEIGHT) {
                    for (x in 0 until AnimParser.ANIM_WIDTH) {
                        val isSet = gray[y][x] >= threshold
                        frame.setPixel(x, y, if (invert) !isSet else isSet)
                    }
                }
            }
            DitherMode.BAYER -> {
                for (y in 0 until AnimParser.ANIM_HEIGHT) {
                    for (x in 0 until AnimParser.ANIM_WIDTH) {
                        val bayerVal = (BAYER_4X4[y % 4][x % 4] + 0.5f) * (256f / 16f)
                        val isSet = gray[y][x] >= bayerVal
                        frame.setPixel(x, y, if (invert) !isSet else isSet)
                    }
                }
            }
            DitherMode.FLOYD_STEINBERG -> {
                for (y in 0 until AnimParser.ANIM_HEIGHT) {
                    for (x in 0 until AnimParser.ANIM_WIDTH) {
                        val oldVal = gray[y][x].coerceIn(0f, 255f)
                        val newVal = if (oldVal >= threshold) 255f else 0f
                        frame.setPixel(x, y, if (invert) (newVal == 0f) else (newVal == 255f))
                        val err = oldVal - newVal

                        if (x + 1 < AnimParser.ANIM_WIDTH) gray[y][x + 1] += err * (7f / 16f)
                        if (y + 1 < AnimParser.ANIM_HEIGHT) {
                            if (x - 1 >= 0) gray[y + 1][x - 1] += err * (3f / 16f)
                            gray[y + 1][x] += err * (5f / 16f)
                            if (x + 1 < AnimParser.ANIM_WIDTH) gray[y + 1][x + 1] += err * (1f / 16f)
                        }
                    }
                }
            }
            DitherMode.ATKINSON -> {
                for (y in 0 until AnimParser.ANIM_HEIGHT) {
                    for (x in 0 until AnimParser.ANIM_WIDTH) {
                        val oldVal = gray[y][x].coerceIn(0f, 255f)
                        val newVal = if (oldVal >= threshold) 255f else 0f
                        frame.setPixel(x, y, if (invert) (newVal == 0f) else (newVal == 255f))
                        val err = oldVal - newVal
                        val frac = err / 8f

                        if (x + 1 < AnimParser.ANIM_WIDTH) gray[y][x + 1] += frac
                        if (x + 2 < AnimParser.ANIM_WIDTH) gray[y][x + 2] += frac
                        if (y + 1 < AnimParser.ANIM_HEIGHT) {
                            if (x - 1 >= 0) gray[y + 1][x - 1] += frac
                            gray[y + 1][x] += frac
                            if (x + 1 < AnimParser.ANIM_WIDTH) gray[y + 1][x + 1] += frac
                        }
                        if (y + 2 < AnimParser.ANIM_HEIGHT) gray[y + 2][x] += frac
                    }
                }
            }
        }

        return frame
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
        invert: Boolean = false,
        ditherMode: DitherMode = if (useDithering) DitherMode.FLOYD_STEINBERG else DitherMode.THRESHOLD,
        fitMode: AspectFitMode = AspectFitMode.FIT
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

        for (bmp in bitmaps) {
            val frameData = encodeBitmapToFrame(bmp, useDithering, threshold, invert, ditherMode, fitMode)
            buffer.put(frameData)
        }

        return buffer.array()
    }

    /**
     * Packs an interactive multi-frame studio sequence into a valid QANM binary.
     */
    fun encodeStudioFrames(
        frames: List<StudioFrame>,
        delayMs: Int = 50,
        loop: Boolean = true,
        invert: Boolean = false
    ): ByteArray {
        val frameCount = frames.size.coerceAtLeast(1)
        val totalBytes = AnimParser.ANIM_HEADER_SIZE + (frameCount * AnimParser.ANIM_FRAME_BYTES)
        val buffer = ByteBuffer.allocate(totalBytes).order(ByteOrder.LITTLE_ENDIAN)

        buffer.putInt(AnimParser.ANIM_MAGIC.toInt())
        buffer.putShort(1.toShort())
        buffer.put(AnimParser.ANIM_WIDTH.toByte())
        buffer.put(AnimParser.ANIM_HEIGHT.toByte())
        buffer.putShort(frameCount.toShort())
        buffer.putShort(delayMs.toShort())
        val flags = (if (loop) 0x0001 else 0) or (if (invert) 0x0002 else 0)
        buffer.putShort(flags.toShort())
        buffer.putShort(0.toShort())

        for (sf in frames) {
            val frameBytes = encodePixelsToFrame(sf.pixels, invert)
            buffer.put(frameBytes)
        }

        return buffer.array()
    }

    // =========================================================================
    // BUILT-IN PROCEDURAL TACTICAL GENERATORS (Editable in Studio)
    // =========================================================================

    fun generateRadarFrames(totalFrames: Int = 24, delayMs: Int = 50): List<StudioFrame> {
        val frames = mutableListOf<StudioFrame>()
        val cx = 64
        val cy = 32
        val rOuter = 28
        val rMid = 18
        val rInner = 8

        for (t in 0 until totalFrames) {
            val sf = StudioFrame(delayMs = delayMs)

            // Corner brackets
            for (i in 0 until 8) {
                sf.setPixel(i, 0, true); sf.setPixel(0, i, true)
                sf.setPixel(127 - i, 0, true); sf.setPixel(127, i, true)
                sf.setPixel(i, 63, true); sf.setPixel(0, 63 - i, true)
                sf.setPixel(127 - i, 63, true); sf.setPixel(127, 63 - i, true)
            }

            // Concentric circles
            for (deg in 0 until 360 step 4) {
                val rad = Math.toRadians(deg.toDouble())
                sf.setPixel(cx + (cos(rad) * rOuter).toInt(), cy + (sin(rad) * rOuter).toInt(), true)
                sf.setPixel(cx + (cos(rad) * rMid).toInt(), cy + (sin(rad) * rMid).toInt(), true)
                sf.setPixel(cx + (cos(rad) * rInner).toInt(), cy + (sin(rad) * rInner).toInt(), true)
            }

            // Crosshairs
            for (x in cx - rOuter - 4..cx + rOuter + 4) {
                if (kotlin.math.abs(x - cx) > 2) sf.setPixel(x, cy, true)
            }
            for (y in cy - rOuter - 4..cy + rOuter + 4) {
                if (kotlin.math.abs(y - cy) > 2) sf.setPixel(cx, y, true)
            }

            // Rotating sweep line
            val ang = (t.toDouble() / totalFrames) * 2.0 * Math.PI
            for (dist in 2..rOuter) {
                sf.setPixel(cx + (cos(ang) * dist).toInt(), cy + (sin(ang) * dist).toInt(), true)
            }

            // Blips
            if (t in 6..9) {
                for (dx in -1..1) for (dy in -1..1) sf.setPixel(cx + 12 + dx, cy - 10 + dy, true)
            }
            if (t in 14..17) {
                for (dx in -1..1) for (dy in -1..1) sf.setPixel(cx - 14 + dx, cy + 8 + dy, true)
            }

            frames.add(sf)
        }
        return frames
    }

    fun generateGunbarrelFrames(totalFrames: Int = 20, delayMs: Int = 50): List<StudioFrame> {
        val frames = mutableListOf<StudioFrame>()
        val cx = 64
        val cy = 32
        val maxRadius = 45

        for (t in 0 until totalFrames) {
            val sf = StudioFrame(delayMs = delayMs)
            val phase = t.toDouble() / totalFrames
            val grooves = 8
            val rot = phase * Math.PI * 0.5

            for (g in 0 until grooves) {
                val baseTheta = rot + (g * 2.0 * Math.PI / grooves)
                for (r in 12 until maxRadius step 2) {
                    val curve = baseTheta + (r * 0.05)
                    sf.setPixel(cx + (cos(curve) * r).toInt(), cy + (sin(curve) * r).toInt(), true)
                    sf.setPixel(cx + 1 + (cos(curve) * r).toInt(), cy + (sin(curve) * r).toInt(), true)
                }
            }

            val targetR = (10 + (2.0 * sin(phase * 4.0 * Math.PI))).toInt()
            for (deg in 0 until 360 step 6) {
                val rad = Math.toRadians(deg.toDouble())
                sf.setPixel(cx + (cos(rad) * targetR).toInt(), cy + (sin(rad) * targetR).toInt(), true)
            }

            for (dx in -1..1) for (dy in -1..1) sf.setPixel(cx + dx, cy + dy, true)
            frames.add(sf)
        }
        return frames
    }

    fun generateMatrixFrames(totalFrames: Int = 24, delayMs: Int = 50): List<StudioFrame> {
        val frames = mutableListOf<StudioFrame>()
        val cols = 32
        val colSpeeds = intArrayOf(3, 5, 2, 4, 3, 4, 2, 5, 3, 2, 4, 3, 5, 2, 4, 3, 2, 5, 4, 3, 2, 4, 5, 3, 2, 4, 3, 5, 2, 4, 3, 4)
        val colOffsets = intArrayOf(0, 14, 28, 4, 42, 10, 50, 22, 8, 36, 18, 54, 2, 40, 26, 12, 48, 6, 32, 16, 58, 20, 44, 30, 4, 38, 24, 52, 10, 46, 18, 60)

        for (t in 0 until totalFrames) {
            val sf = StudioFrame(delayMs = delayMs)
            for (c in 0 until cols) {
                val headY = (colOffsets[c] + t * colSpeeds[c]) % 80
                val colX = c * 4
                for (trail in 0 until 12) {
                    val ty = headY - trail
                    if (ty in 0 until 64) {
                        if (trail == 0) {
                            sf.setPixel(colX, ty, true)
                            sf.setPixel(colX + 1, ty, true)
                            sf.setPixel(colX, ty - 1, true)
                            sf.setPixel(colX + 1, ty - 1, true)
                        } else if ((trail + (t % 3)) % 2 == 0) {
                            sf.setPixel(colX, ty, true)
                        }
                    }
                }
            }
            frames.add(sf)
        }
        return frames
    }

    fun generateEcgFrames(totalFrames: Int = 24, delayMs: Int = 50): List<StudioFrame> {
        val frames = mutableListOf<StudioFrame>()
        val cy = 32
        val ecgY = IntArray(128) { cy }
        for (i in 0 until 10) ecgY[30 + i] = cy - (3 * sin(i * Math.PI / 10)).toInt()
        ecgY[44] = cy + 4
        ecgY[46] = cy - 22
        ecgY[47] = cy - 26
        ecgY[48] = cy - 20
        ecgY[50] = cy + 10
        for (i in 0 until 16) ecgY[60 + i] = cy - (6 * sin(i * Math.PI / 16)).toInt()

        for (t in 0 until totalFrames) {
            val sf = StudioFrame(delayMs = delayMs)
            for (gx in 0 until 128 step 16) {
                for (gy in 0 until 64 step 4) sf.setPixel(gx, gy, true)
            }
            val sweepX = ((t.toDouble() / totalFrames) * 128).toInt()
            for (x in 0 until 128) {
                val dist = (x - sweepX + 128) % 128
                if (dist > 110 || dist < 2) {
                    val y = ecgY[x]
                    sf.setPixel(x, y, true)
                    if (x > 0) {
                        val prevY = ecgY[x - 1]
                        val minY = kotlin.math.min(y, prevY)
                        val maxY = kotlin.math.max(y, prevY)
                        for (ly in minY..maxY) sf.setPixel(x, ly, true)
                    }
                }
            }
            for (sy in cy - 12..cy + 12) sf.setPixel(sweepX, sy, true)
            frames.add(sf)
        }
        return frames
    }

    fun generateLockonFrames(totalFrames: Int = 20, delayMs: Int = 50): List<StudioFrame> {
        val frames = mutableListOf<StudioFrame>()
        val cx = 64
        val cy = 32

        for (t in 0 until totalFrames) {
            val sf = StudioFrame(delayMs = delayMs)
            val boxSz = 26 - (6 * sin((t.toDouble() / totalFrames) * Math.PI)).toInt()
            val k = 6

            for (i in 0 until k) {
                sf.setPixel(cx - boxSz + i, cy - boxSz, true)
                sf.setPixel(cx - boxSz, cy - boxSz + i, true)
                sf.setPixel(cx + boxSz - i, cy - boxSz, true)
                sf.setPixel(cx + boxSz, cy - boxSz + i, true)
                sf.setPixel(cx - boxSz + i, cy + boxSz, true)
                sf.setPixel(cx - boxSz, cy + boxSz - i, true)
                sf.setPixel(cx + boxSz - i, cy + boxSz, true)
                sf.setPixel(cx + boxSz, cy + boxSz - i, true)
            }

            for (x in cx - 10..cx + 10) {
                if (kotlin.math.abs(x - cx) > 2) sf.setPixel(x, cy, true)
            }
            for (y in cy - 10..cy + 10) {
                if (kotlin.math.abs(y - cy) > 2) sf.setPixel(cx, y, true)
            }

            val rot = (t.toDouble() / totalFrames) * 2.0 * Math.PI
            for (deg in intArrayOf(0, 90, 180, 270)) {
                val rad = rot + Math.toRadians(deg.toDouble())
                for (r in boxSz + 4..boxSz + 8) {
                    sf.setPixel(cx + (cos(rad) * r).toInt(), cy + (sin(rad) * r).toInt(), true)
                }
            }

            frames.add(sf)
        }
        return frames
    }

    // =========================================================================
    // LIGHTWEIGHT STANDALONE GIF DECODER
    // =========================================================================

    /**
     * Extracts individual frames and their delays from an animated GIF.
     */
    fun decodeGif(inputStream: InputStream): List<Pair<Bitmap, Int>> {
        val data = inputStream.readBytes()
        if (data.size < 13) return emptyList()

        val sig = String(data, 0, 6)
        if (sig != "GIF87a" && sig != "GIF89a") return emptyList()

        var pos = 6
        fun readByte(): Int = data[pos++].toInt() and 0xFF
        fun readShort(): Int {
            val v = (data[pos].toInt() and 0xFF) or ((data[pos + 1].toInt() and 0xFF) shl 8)
            pos += 2
            return v
        }

        val canvasW = readShort()
        val canvasH = readShort()
        val packed = readByte()
        pos += 2 // skip bg & aspect

        val hasGct = (packed and 0x80) != 0
        val gctSize = 1 shl ((packed and 0x07) + 1)
        val gct = IntArray(if (hasGct) gctSize else 0)
        if (hasGct) {
            for (i in 0 until gctSize) {
                val r = readByte()
                val g = readByte()
                val b = readByte()
                gct[i] = Color.rgb(r, g, b)
            }
        }

        val frames = mutableListOf<Pair<Bitmap, Int>>()
        var lastDelayMs = 50
        var transIdx: Int? = null
        var currentCanvas = Bitmap.createBitmap(canvasW.coerceAtLeast(1), canvasH.coerceAtLeast(1), Bitmap.Config.ARGB_8888)

        while (pos < data.size) {
            val block = readByte()
            if (block == 0x3B) break // Trailer
            if (block == 0x21) {
                val extType = readByte()
                if (extType == 0xF9) {
                    readByte() // size 4
                    val gcePacked = readByte()
                    val delay10ms = readShort()
                    val trans = readByte()
                    readByte() // terminator
                    lastDelayMs = (delay10ms * 10).coerceAtLeast(20)
                    transIdx = if ((gcePacked and 0x01) != 0) trans else null
                } else {
                    // Skip sub-blocks
                    while (pos < data.size) {
                        val sz = readByte()
                        if (sz == 0) break
                        pos += sz
                    }
                }
            } else if (block == 0x2C) {
                val left = readShort()
                val top = readShort()
                val iw = readShort()
                val ih = readShort()
                val ipacked = readByte()
                val hasLct = (ipacked and 0x80) != 0
                val palette = if (hasLct) {
                    val lctSize = 1 shl ((ipacked and 0x07) + 1)
                    val lct = IntArray(lctSize)
                    for (i in 0 until lctSize) {
                        lct[i] = Color.rgb(readByte(), readByte(), readByte())
                    }
                    lct
                } else {
                    gct
                }

                val minCodeSize = readByte()
                // Read image data sub-blocks
                val compData = ArrayList<Byte>()
                while (pos < data.size) {
                    val sz = readByte()
                    if (sz == 0) break
                    for (k in 0 until sz) compData.add(data[pos + k])
                    pos += sz
                }

                // Decompress LZW
                val pixels = decompressLzw(compData, minCodeSize, iw * ih)
                val frameBmp = currentCanvas.copy(Bitmap.Config.ARGB_8888, true)
                for (y in 0 until ih) {
                    val cy = top + y
                    if (cy >= canvasH) break
                    for (x in 0 until iw) {
                        val cx = left + x
                        if (cx >= canvasW) break
                        val pIdx = y * iw + x
                        if (pIdx < pixels.size) {
                            val cIdx = pixels[pIdx]
                            if (cIdx != transIdx && cIdx in palette.indices) {
                                frameBmp.setPixel(cx, cy, palette[cIdx])
                            }
                        }
                    }
                }

                currentCanvas = frameBmp
                frames.add(frameBmp to lastDelayMs)
                lastDelayMs = 50
                transIdx = null
            }
        }

        return if (frames.isNotEmpty()) frames else listOf(currentCanvas to 50)
    }

    private fun decompressLzw(data: List<Byte>, minCodeSize: Int, expectedPixels: Int): IntArray {
        val clearCode = 1 shl minCodeSize
        val endCode = clearCode + 1
        var codeSize = minCodeSize + 1
        var maxCode = (1 shl codeSize) - 1

        val table = ArrayList<IntArray>()
        fun resetTable() {
            table.clear()
            for (i in 0 until clearCode) {
                table.add(intArrayOf(i))
            }
            table.add(intArrayOf()) // clear
            table.add(intArrayOf()) // end
        }

        resetTable()
        var bitBuf = 0
        var bitsCount = 0
        var byteIdx = 0
        val dataLen = data.size

        fun readBits(n: Int): Int {
            while (bitsCount < n && byteIdx < dataLen) {
                bitBuf = bitBuf or ((data[byteIdx].toInt() and 0xFF) shl bitsCount)
                byteIdx++
                bitsCount += 8
            }
            val v = bitBuf and ((1 shl n) - 1)
            bitBuf = bitBuf ushr n
            bitsCount -= n
            return v
        }

        val out = IntArray(expectedPixels)
        var outIdx = 0
        var prevCode = -1

        while (outIdx < expectedPixels && byteIdx <= dataLen) {
            val code = readBits(codeSize)
            if (code == endCode) break
            if (code == clearCode) {
                resetTable()
                codeSize = minCodeSize + 1
                maxCode = (1 shl codeSize) - 1
                prevCode = -1
                continue
            }

            val entry: IntArray = when {
                code < table.size -> table[code]
                code == table.size && prevCode != -1 -> {
                    val p = table[prevCode]
                    val arr = IntArray(p.size + 1)
                    System.arraycopy(p, 0, arr, 0, p.size)
                    arr[p.size] = p[0]
                    arr
                }
                else -> break
            }

            for (px in entry) {
                if (outIdx < expectedPixels) {
                    out[outIdx++] = px
                }
            }

            if (prevCode != -1 && table.size < 4096) {
                val p = table[prevCode]
                val arr = IntArray(p.size + 1)
                System.arraycopy(p, 0, arr, 0, p.size)
                arr[p.size] = entry[0]
                table.add(arr)
                if (table.size > maxCode && codeSize < 12) {
                    codeSize++
                    maxCode = (1 shl codeSize) - 1
                }
            }
            prevCode = code
        }

        return out
    }
}
