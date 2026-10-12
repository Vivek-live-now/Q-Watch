package com.qwatch.qlink.anim

/**
 * Binary animation header specification (16 bytes fixed layout).
 * Matches include/anim_engine.h AnimHeader struct.
 */
data class AnimHeader(
    val magic: Long,          // 0x4D4E4151 ("QANM" in little-endian)
    val version: Int,        // 1
    val width: Int,          // 128
    val height: Int,         // 64
    val frameCount: Int,     // Total frames in animation
    val frameDelayMs: Int,   // Delay per frame in milliseconds (e.g. 50ms = 20 FPS)
    val flags: Int           // bit 0 = loop, bit 1 = invert
) {
    val isLooping: Boolean get() = (flags and 0x0001) != 0
    val isInverted: Boolean get() = (flags and 0x0002) != 0
    val fps: Int get() = if (frameDelayMs > 0) (1000 / frameDelayMs) else 20
    val durationSec: Float get() = (frameCount * (if (frameDelayMs > 0) frameDelayMs else 50)) / 1000f
}

/**
 * Validation result for QANM binary format.
 */
data class AnimValidationResult(
    val isValid: Boolean,
    val error: String? = null,
    val header: AnimHeader? = null
)

/**
 * Real-time transfer progress state during upload/download.
 */
data class TransferProgress(
    val progress: Float = 0f,
    val percent: Int = 0,
    val bytesTransferred: Long = 0L,
    val totalBytes: Long = 0L
)

/**
 * Decoded animation asset with header and 1024-byte frames.
 */
data class AnimFile(
    val name: String,
    val header: AnimHeader,
    val frames: List<ByteArray>,
    val totalBytes: Long
)

/**
 * Deployment lifecycle state for an animation in the Market.
 */
enum class AnimDeployState {
    IDLE,
    DEPLOYING,
    DEPLOYED,
    FAILED
}

/**
 * Catalog categorization for filtering in UI.
 */
enum class AnimCategory(val label: String) {
    ALL("ALL"),
    FAVORITES("★ FAVORITES"),
    INSTALLED("INSTALLED"),
    MOODS("MOODS"),
    ACTIONS("ACTIONS"),
    MECHA("MECHA")
}

/**
 * Model representing an entry in the Mochi Animation Market.
 */
data class AnimMarketItem(
    val name: String,               // e.g. "dancing.anim"
    val displayName: String,        // e.g. "DANCING"
    val category: AnimCategory,
    val frameCount: Int,
    val sizeBytes: Long,
    val isInstalled: Boolean,
    val deployState: AnimDeployState = AnimDeployState.IDLE,
    val isBootAnim: Boolean = false,
    val isCustom: Boolean = false,
    val isFavorite: Boolean = false,
    val isDuplicate: Boolean = false,
    val fps: Int = 20,
    val durationSec: Float = 1.0f,
    val transferProgress: TransferProgress? = null
) {
    val formattedSize: String
        get() {
            val kb = sizeBytes / 1024.0
            return "${String.format("%,d", sizeBytes)} B (${String.format("%.1f", kb)} KB)"
        }

    val formattedDuration: String
        get() = "${frameCount} frames · ${String.format("%.1f", durationSec)}s @ ${fps}fps"
}

object AnimClassifier {
    private val MECHA_KEYWORDS = setOf(
        "police", "tough", "menacing", "evil", "devil", "buzzing", "blinding", "glowing", "cyber", "gundam"
    )
    private val MOOD_KEYWORDS = setOf(
        "adore", "blank", "brave", "contempt", "energetic", "enraged", "fierce", "furious",
        "scared", "sick", "smirk", "sneeze", "sobbing", "weeping", "happy", "love", "angry",
        "dizzy", "surprised", "sleepy", "sad", "wink", "shy"
    )

    fun classify(name: String): AnimCategory {
        val clean = name.lowercase().removeSuffix(".anim").trim()
        return when {
            MECHA_KEYWORDS.any { clean.contains(it) } -> AnimCategory.MECHA
            MOOD_KEYWORDS.any { clean.contains(it) } -> AnimCategory.MOODS
            else -> AnimCategory.ACTIONS
        }
    }

    fun formatDisplayName(filename: String): String {
        return filename.removeSuffix(".anim")
            .replace('_', ' ')
            .replace('-', ' ')
            .uppercase()
    }
}

/**
 * Pixel drawing tools for the full-fledged Live OLED Studio & Editor.
 */
enum class DrawTool(val label: String) {
    PENCIL("PEN"),
    ERASER("ERASE"),
    LINE("LINE"),
    RECT("BOX"),
    FILL_RECT("FILL BOX"),
    CIRCLE("CIRCLE"),
    INVERT("INVERT"),
    CLEAR("CLEAR"),
    FILL("FILL ALL")
}

/**
 * Advanced dithering algorithms for media quantization.
 */
enum class DitherMode(val label: String) {
    FLOYD_STEINBERG("FLOYD-STEINBERG"),
    ATKINSON("ATKINSON"),
    BAYER("BAYER 4X4"),
    THRESHOLD("THRESHOLD")
}

/**
 * Aspect ratio handling when importing images and GIFs.
 */
enum class AspectFitMode(val label: String) {
    FIT("LETTERBOX (FIT)"),
    STRETCH("STRETCH"),
    CROP("CENTER CROP")
}

/**
 * Single mutable frame in the Live Animation Studio with full pixel editing utilities.
 */
data class StudioFrame(
    val pixels: BooleanArray = BooleanArray(128 * 64),
    var delayMs: Int = 50
) {
    fun clone(): StudioFrame = StudioFrame(pixels.clone(), delayMs)

    fun getPixel(x: Int, y: Int): Boolean {
        if (x !in 0 until 128 || y !in 0 until 64) return false
        return pixels[y * 128 + x]
    }

    fun setPixel(x: Int, y: Int, value: Boolean) {
        if (x in 0 until 128 && y in 0 until 64) {
            pixels[y * 128 + x] = value
        }
    }

    fun clear() {
        pixels.fill(false)
    }

    fun fillAll() {
        pixels.fill(true)
    }

    fun invert() {
        for (i in pixels.indices) pixels[i] = !pixels[i]
    }

    fun drawLine(x0: Int, y0: Int, x1: Int, y1: Int, value: Boolean) {
        var x = x0
        var y = y0
        val dx = kotlin.math.abs(x1 - x0)
        val dy = kotlin.math.abs(y1 - y0)
        val sx = if (x0 < x1) 1 else -1
        val sy = if (y0 < y1) 1 else -1
        var err = dx - dy

        while (true) {
            setPixel(x, y, value)
            if (x == x1 && y == y1) break
            val e2 = 2 * err
            if (e2 > -dy) {
                err -= dy
                x += sx
            }
            if (e2 < dx) {
                err += dx
                y += sy
            }
        }
    }

    fun drawRect(x0: Int, y0: Int, x1: Int, y1: Int, fill: Boolean, value: Boolean) {
        val minX = kotlin.math.min(x0, x1)
        val maxX = kotlin.math.max(x0, x1)
        val minY = kotlin.math.min(y0, y1)
        val maxY = kotlin.math.max(y0, y1)

        if (fill) {
            for (y in minY..maxY) {
                for (x in minX..maxX) {
                    setPixel(x, y, value)
                }
            }
        } else {
            for (x in minX..maxX) {
                setPixel(x, minY, value)
                setPixel(x, maxY, value)
            }
            for (y in minY..maxY) {
                setPixel(minX, y, value)
                setPixel(maxX, y, value)
            }
        }
    }

    fun drawCircle(cx: Int, cy: Int, radius: Int, value: Boolean) {
        var x = 0
        var y = radius
        var d = 3 - 2 * radius
        fun plot8(px: Int, py: Int) {
            setPixel(cx + px, cy + py, value)
            setPixel(cx - px, cy + py, value)
            setPixel(cx + px, cy - py, value)
            setPixel(cx - px, cy - py, value)
            setPixel(cx + py, cy + px, value)
            setPixel(cx - py, cy + px, value)
            setPixel(cx + py, cy - px, value)
            setPixel(cx - py, cy - px, value)
        }
        plot8(x, y)
        while (y >= x) {
            x++
            d += if (d > 0) {
                y--
                4 * (x - y) + 10
            } else {
                4 * x + 6
            }
            plot8(x, y)
        }
    }

    fun shift(dx: Int, dy: Int) {
        val next = BooleanArray(pixels.size)
        val w = 128
        val h = 64
        for (y in 0 until h) {
            val ny = y + dy
            if (ny !in 0 until h) continue
            for (x in 0 until w) {
                val nx = x + dx
                if (nx in 0 until w) {
                    next[ny * w + nx] = pixels[y * w + x]
                }
            }
        }
        System.arraycopy(next, 0, pixels, 0, pixels.size)
    }

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is StudioFrame) return false
        return pixels.contentEquals(other.pixels) && delayMs == other.delayMs
    }

    override fun hashCode(): Int {
        var result = pixels.contentHashCode()
        result = 31 * result + delayMs
        return result
    }
}

