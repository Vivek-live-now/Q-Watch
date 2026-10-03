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
