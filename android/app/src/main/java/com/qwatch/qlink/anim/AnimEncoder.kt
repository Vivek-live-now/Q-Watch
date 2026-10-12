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
     * Adjusts brightness (-100..+100) and contrast (-100..+100) of a bitmap via ColorMatrix.
     */
    fun adjustBrightnessContrast(src: Bitmap, brightness: Float, contrast: Float): Bitmap {
        if (brightness == 0f && contrast == 0f) return src
        val bmp = Bitmap.createBitmap(src.width, src.height, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bmp)
        val paint = Paint(Paint.FILTER_BITMAP_FLAG)
        val c = ((contrast + 100f) / 100f).coerceIn(0f, 4f)
        val cm = android.graphics.ColorMatrix(floatArrayOf(
            c, 0f, 0f, 0f, brightness,
            0f, c, 0f, 0f, brightness,
            0f, 0f, c, 0f, brightness,
            0f, 0f, 0f, 1f, 0f
        ))
        paint.colorFilter = android.graphics.ColorMatrixColorFilter(cm)
        canvas.drawBitmap(src, 0f, 0f, paint)
        return bmp
    }

    /**
     * Converts a single Bitmap into an editable StudioFrame with dithering, brightness, contrast, and aspect ratio controls.
     */
    fun decodeBitmapToStudioFrame(
        src: Bitmap,
        ditherMode: DitherMode = DitherMode.FLOYD_STEINBERG,
        threshold: Float = 128f,
        invert: Boolean = false,
        fitMode: AspectFitMode = AspectFitMode.FIT,
        brightness: Float = 0f,
        contrast: Float = 0f
    ): StudioFrame {
        val adjustedSrc = if (brightness != 0f || contrast != 0f) adjustBrightnessContrast(src, brightness, contrast) else src
        val scaled = Bitmap.createBitmap(AnimParser.ANIM_WIDTH, AnimParser.ANIM_HEIGHT, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(scaled)
        canvas.drawColor(Color.BLACK)

        val srcW = adjustedSrc.width.toFloat()
        val srcH = adjustedSrc.height.toFloat()

        when (fitMode) {
            AspectFitMode.STRETCH -> {
                canvas.drawBitmap(adjustedSrc, null, Rect(0, 0, AnimParser.ANIM_WIDTH, AnimParser.ANIM_HEIGHT), Paint(Paint.FILTER_BITMAP_FLAG))
            }
            AspectFitMode.CROP -> {
                val scale = (AnimParser.ANIM_WIDTH / srcW).coerceAtLeast(AnimParser.ANIM_HEIGHT / srcH)
                val destW = (srcW * scale).toInt()
                val destH = (srcH * scale).toInt()
                val offsetX = (AnimParser.ANIM_WIDTH - destW) / 2
                val offsetY = (AnimParser.ANIM_HEIGHT - destH) / 2
                canvas.drawBitmap(adjustedSrc, null, Rect(offsetX, offsetY, offsetX + destW, offsetY + destH), Paint(Paint.FILTER_BITMAP_FLAG))
            }
            AspectFitMode.FIT -> {
                val scale = (AnimParser.ANIM_WIDTH / srcW).coerceAtMost(AnimParser.ANIM_HEIGHT / srcH)
                val destW = (srcW * scale).toInt()
                val destH = (srcH * scale).toInt()
                val offsetX = (AnimParser.ANIM_WIDTH - destW) / 2
                val offsetY = (AnimParser.ANIM_HEIGHT - destH) / 2
                canvas.drawBitmap(adjustedSrc, null, Rect(offsetX, offsetY, offsetX + destW, offsetY + destH), Paint(Paint.FILTER_BITMAP_FLAG))
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
                val shift = (threshold - 128f) * (255f / 128f)
                for (y in 0 until AnimParser.ANIM_HEIGHT) {
                    for (x in 0 until AnimParser.ANIM_WIDTH) {
                        val bayerVal = (BAYER_4X4[y % 4][x % 4] + 0.5f) * (256f / 16f) + shift
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

    /**
     * Checks if byte array starts with GIF magic signature ("GIF87a" or "GIF89a").
     */
    fun isGif(data: ByteArray): Boolean {
        if (data.size < 6) return false
        val s = String(data, 0, 6)
        return s == "GIF87a" || s == "GIF89a"
    }

    /**
     * Checks if byte array starts with Q-Watch .anim magic signature ("QANM" / 0x4D4E4151).
     */
    fun isAnim(data: ByteArray): Boolean {
        if (data.size < 4) return false
        return (data[0].toInt() and 0xFF) == 0x51 &&
               (data[1].toInt() and 0xFF) == 0x41 &&
               (data[2].toInt() and 0xFF) == 0x4E &&
               (data[3].toInt() and 0xFF) == 0x4D
    }

    /**
     * Checks if byte array starts with standard Windows BMP signature ("BM").
     */
    fun isBmp(data: ByteArray): Boolean {
        return data.size >= 2 && data[0] == 'B'.code.toByte() && data[1] == 'M'.code.toByte()
    }

    /**
     * Packs an 8192-element BooleanArray (128x64) into a standard Windows 1-bit monochrome BMP (1086 bytes)
     * natively rendered by Q-Watch AnimEngine.
     */
    fun encodeStudioFrameToBmp(
        frame: StudioFrame,
        invert: Boolean = false
    ): ByteArray {
        val totalSize = 14 + 40 + 8 + 1024 // 1086 bytes
        val buffer = ByteBuffer.allocate(totalSize).order(ByteOrder.LITTLE_ENDIAN)

        // 14-byte BMP File Header
        buffer.put('B'.code.toByte())
        buffer.put('M'.code.toByte())
        buffer.putInt(totalSize) // File size = 1086
        buffer.putInt(0)         // Reserved
        buffer.putInt(62)        // Pixel data offset = 14 + 40 + 8 = 62

        // 40-byte BITMAPINFOHEADER
        buffer.putInt(40)                      // Header size
        buffer.putInt(AnimParser.ANIM_WIDTH)   // Width = 128
        buffer.putInt(AnimParser.ANIM_HEIGHT)  // Height = 64 (positive = bottom-up)
        buffer.putShort(1.toShort())           // Planes = 1
        buffer.putShort(1.toShort())           // 1-bit monochrome
        buffer.putInt(0)                       // BI_RGB (no compression)
        buffer.putInt(1024)                    // Image size in bytes
        buffer.putInt(0)                       // X pixels per meter
        buffer.putInt(0)                       // Y pixels per meter
        buffer.putInt(2)                       // Colors used = 2
        buffer.putInt(0)                       // Colors important

        // 8-byte Palette (Color 0 = Black, Color 1 = White)
        buffer.put(0.toByte()); buffer.put(0.toByte()); buffer.put(0.toByte()); buffer.put(0.toByte())
        buffer.put(0xFF.toByte()); buffer.put(0xFF.toByte()); buffer.put(0xFF.toByte()); buffer.put(0.toByte())

        // 1024 bytes pixel data (64 rows of 16 bytes each, bottom-up, MSB first)
        val w = AnimParser.ANIM_WIDTH
        val h = AnimParser.ANIM_HEIGHT
        val rowBytes = w / 8 // 16 bytes

        for (fileRow in 0 until h) {
            val screenY = (h - 1) - fileRow // bottom-up row
            for (col in 0 until rowBytes) {
                var byteVal = 0
                for (b in 0 until 8) {
                    val screenX = col * 8 + b
                    var lit = frame.getPixel(screenX, screenY)
                    if (invert) lit = !lit
                    if (lit) {
                        byteVal = byteVal or (1 shl (7 - b)) // MSB first for standard BMP
                    }
                }
                buffer.put(byteVal.toByte())
            }
        }

        return buffer.array()
    }

    /**
     * Encodes a single Bitmap into a 128x64 1-bit monochrome BMP (1086 bytes) with dithering and fit modes.
     */
    fun encodeBitmapToBmp(
        src: Bitmap,
        ditherMode: DitherMode = DitherMode.FLOYD_STEINBERG,
        threshold: Float = 128f,
        invert: Boolean = false,
        fitMode: AspectFitMode = AspectFitMode.FIT
    ): ByteArray {
        val studioFrame = decodeBitmapToStudioFrame(src, ditherMode, threshold, invert, fitMode)
        return encodeStudioFrameToBmp(studioFrame, false)
    }

    /**
     * Decodes a standard 128x64 1-bit BMP into an editable StudioFrame.
     */
    fun decodeBmpToStudioFrame(data: ByteArray): StudioFrame? {
        if (!isBmp(data) || data.size < 62 + 1024) return null
        val buf = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN)
        val pixelOffset = buf.getInt(10)
        val width = buf.getInt(18)
        val height = buf.getInt(22)
        val bpp = buf.getShort(28).toInt()

        if (width != AnimParser.ANIM_WIDTH || kotlin.math.abs(height) != AnimParser.ANIM_HEIGHT || bpp != 1) {
            return null
        }

        val isBottomUp = height > 0
        val frame = StudioFrame()
        val rowBytes = AnimParser.ANIM_WIDTH / 8

        for (row in 0 until AnimParser.ANIM_HEIGHT) {
            val fileRow = if (isBottomUp) (AnimParser.ANIM_HEIGHT - 1 - row) else row
            val offset = pixelOffset + fileRow * rowBytes
            if (offset + rowBytes > data.size) break

            for (col in 0 until rowBytes) {
                val bVal = data[offset + col].toInt() and 0xFF
                for (b in 0 until 8) {
                    val isSet = (bVal and (1 shl (7 - b))) != 0
                    frame.setPixel(col * 8 + b, row, isSet)
                }
            }
        }
        return frame
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
        return decodeGif(inputStream.readBytes())
    }

    /**
     * Extracts individual frames and their delays from an animated GIF byte array.
     * Incorporates custom LZW decoding with automatic android.graphics.Movie / BitmapFactory fallback.
     */
    fun decodeGif(data: ByteArray): List<Pair<Bitmap, Int>> {
        if (data.size < 6) return emptyList()

        // 1. Try manual LZW decoder
        try {
            val sig = String(data, 0, 6)
            if (sig == "GIF87a" || sig == "GIF89a") {
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

                if (frames.isNotEmpty()) {
                    return frames
                }
            }
        } catch (_: Exception) {
            // Fall through to fallback decoders
        }

        // 2. Fallback: android.graphics.Movie frame extraction
        try {
            val movie = android.graphics.Movie.decodeByteArray(data, 0, data.size)
            if (movie != null && movie.duration() > 0) {
                val duration = movie.duration()
                val step = 50 // ms per frame
                val w = movie.width().coerceAtLeast(1)
                val h = movie.height().coerceAtLeast(1)
                val fallbackFrames = mutableListOf<Pair<Bitmap, Int>>()
                var t = 0
                while (t < duration && fallbackFrames.size < 120) {
                    val bmp = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
                    val c = Canvas(bmp)
                    movie.setTime(t)
                    movie.draw(c, 0f, 0f)
                    fallbackFrames.add(bmp to step)
                    t += step
                }
                if (fallbackFrames.isNotEmpty()) return fallbackFrames
            }
        } catch (_: Exception) {
            // Fall through
        }

        // 3. Fallback: Single frame BitmapFactory
        try {
            val bmp = android.graphics.BitmapFactory.decodeByteArray(data, 0, data.size)
            if (bmp != null) {
                return listOf(bmp to 50)
            }
        } catch (_: Exception) {
            // Nothing more
        }

        return emptyList()
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
