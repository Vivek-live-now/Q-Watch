package com.qwatch.qlink.irdb

data class IrdbEntry(
    val filename: String,
    val deviceType: String,
    val brand: String,
    val model: String,
    val series: String = "",
    val path: String,
    val additionalInfo: String = ""
) {
    val rawDownloadUrl: String
        get() = "https://raw.githubusercontent.com/Lucaslhm/Flipper-IRDB/main/" + path.replace('\\', '/')

    val cleanDisplayName: String
        get() = if (brand.isNotBlank() && model.isNotBlank() && !model.contains(brand, ignoreCase = true)) {
            "$brand $model"
        } else if (model.isNotBlank()) {
            model
        } else {
            filename.removeSuffix(".ir")
        }
}

data class IrParsedButton(
    val name: String,
    val type: String = "parsed", // "parsed" or "raw"
    val protocol: String = "",
    val address: String = "",
    val command: String = "",
    val frequency: Long = 0,
    val rawTimingsCount: Int = 0,
    val nbits: Int = 0,
    val rawTimings: List<Int> = emptyList(),
    val dutyCycle: Float = 0.33f
) {
    val isProtocolBased: Boolean
        get() = type.equals("parsed", ignoreCase = true) &&
                protocol.isNotBlank() &&
                !protocol.equals("raw", ignoreCase = true) &&
                !protocol.equals("unknown", ignoreCase = true)

    val isRaw: Boolean
        get() = type.equals("raw", ignoreCase = true) ||
                protocol.isBlank() ||
                protocol.equals("raw", ignoreCase = true) ||
                rawTimings.isNotEmpty()

    val effectiveBits: Int
        get() = if (nbits > 0) nbits else getDefaultBitsForProtocol(protocol)

    companion object {
        fun getDefaultBitsForProtocol(protocol: String): Int {
            val p = protocol.trim().uppercase()
            return when {
                p == "RCA" || p == "NIKAI" -> 24
                p == "SONY" || p == "SIRC" || p == "RC5" -> 12
                p == "SIRC15" || p == "SHARP" || p == "DENON" -> 15
                p == "SIRC20" || p == "RC6" -> 20
                p == "RC5X" -> 13
                p == "JVC" -> 16
                p == "PANASONIC" -> 48
                p == "NEC42" -> 42
                p == "SAMSUNG" || p == "SAMSUNG32" || p.startsWith("NEC") -> 32
                else -> 32
            }
        }

        fun parseHexOrFlipperBytes(valStr: String): Long {
            val trimmed = valStr.trim()
            if (trimmed.isEmpty()) return 0L
            if (trimmed.contains(' ') || trimmed.contains('\t')) {
                val tokens = trimmed.split("\\s+".toRegex()).filter { it.isNotEmpty() }
                var result = 0L
                for (i in tokens.indices) {
                    if (i >= 8) break
                    val byteVal = tokens[i].removePrefix("0x").removePrefix("0X").toLongOrNull(16) ?: 0L
                    result = result or ((byteVal and 0xFF) shl (i * 8))
                }
                return result
            }
            if (trimmed.startsWith("0x", ignoreCase = true)) {
                return trimmed.substring(2).toLongOrNull(16) ?: 0L
            }
            return trimmed.toLongOrNull(16) ?: (trimmed.toLongOrNull(10) ?: 0L)
        }
    }
}

enum class IrTransmitState {
    IDLE,
    TRANSMITTING,
    SUCCESS,
    SIMULATED,
    UNSUPPORTED,
    FAILED
}

data class IrTransmitFeedback(
    val state: IrTransmitState = IrTransmitState.IDLE,
    val buttonName: String = "",
    val message: String = "",
    val error: String? = null,
    val timestamp: Long = System.currentTimeMillis()
)

data class IrRemoteFile(
    val entry: IrdbEntry,
    val filetype: String,
    val rawText: String,
    val buttons: List<IrParsedButton>
)

enum class IrdbTransferStatus {
    IDLE,
    DOWNLOADING,
    CONNECTING_BLE,
    UPLOADING,
    SUCCESS,
    ERROR
}

data class IrdbTransferProgress(
    val status: IrdbTransferStatus = IrdbTransferStatus.IDLE,
    val message: String = "",
    val percent: Int = 0,
    val bytesTransferred: Long = 0L,
    val totalBytes: Long = 0L,
    val error: String? = null
)
