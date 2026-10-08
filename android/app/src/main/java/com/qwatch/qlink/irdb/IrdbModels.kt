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
    val type: String, // "parsed" or "raw"
    val protocol: String = "",
    val address: String = "",
    val command: String = "",
    val frequency: Long = 0,
    val rawTimingsCount: Int = 0
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
    val bytesTransferred: Int = 0,
    val totalBytes: Int = 0,
    val error: String? = null
)
