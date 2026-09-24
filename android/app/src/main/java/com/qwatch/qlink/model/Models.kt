package com.qwatch.qlink.model

enum class ConnectionState {
    DISCONNECTED,
    SCANNING,
    CONNECTING,
    CONNECTED_BLE,
    CONNECTED_WIFI,
    ERROR
}

enum class ButtonType {
    UP,
    OK,
    DOWN,
    CANCEL
}

enum class ButtonEventType {
    SHORT_PRESS,
    LONG_PRESS,
    DOUBLE_TAP
}

data class DeviceInfo(
    val deviceName: String = "Q-Watch",
    val model: String = "ESP32-S3-SuperMini",
    val firmwareVersion: String = "1.4.0",
    val protocolVersion: String = "1.0.0",
    val compileDate: String = "",
    val macAddress: String = "",
    val ipAddress: String = "",
    val rssi: Int = 0,
    val batteryPercent: Int = 0,
    val batteryVoltageMv: Int = 0,
    val freeHeapBytes: Long = 0,
    val fsTotalBytes: Long = 0,
    val fsUsedBytes: Long = 0,
    val uptimeSec: Long = 0
)

data class BmeData(
    val temperatureC: Float = 0.0f,
    val pressureHpa: Float = 0.0f,
    val humidityPct: Float = 0.0f,
    val altitudeM: Float = 0.0f
)

data class ImuData(
    val pitchDeg: Float = 0.0f,
    val rollDeg: Float = 0.0f,
    val accelX: Float = 0.0f,
    val accelY: Float = 0.0f,
    val accelZ: Float = 1.0f
)

data class CompassData(
    val headingDeg: Float = 0.0f,
    val magneticFieldUt: Float = 0.0f
)

data class HealthData(
    val heartRateBpm: Int = 0,
    val spo2Pct: Int = 0,
    val fingerDetected: Boolean = false
)

data class PedometerData(
    val steps: Long = 0,
    val distanceKm: Float = 0.0f,
    val caloriesKcal: Long = 0
)

data class TelemetrySnapshot(
    val timestampMs: Long = System.currentTimeMillis(),
    val bme: BmeData = BmeData(),
    val imu: ImuData = ImuData(),
    val compass: CompassData = CompassData(),
    val health: HealthData = HealthData(),
    val pedometer: PedometerData = PedometerData(),
    val batteryPct: Int = 0,
    val batteryMv: Int = 0,
    val uptimeSec: Long = 0
)

data class DisplayFrame(
    val buffer: ByteArray = ByteArray(1024), // 128x64 1-bit monochrome SSD1306/SH1106
    val width: Int = 128,
    val height: Int = 64,
    val frameNumber: Long = 0,
    val timestampMs: Long = System.currentTimeMillis()
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false
        other as DisplayFrame
        return buffer.contentEquals(other.buffer) && frameNumber == other.frameNumber
    }

    override fun hashCode(): Int {
        var result = buffer.contentHashCode()
        result = 31 * result + frameNumber.hashCode()
        return result
    }
}

data class WatchFile(
    val name: String,
    val path: String,
    val sizeBytes: Long,
    val isDirectory: Boolean
)

data class QAppMeta(
    val id: String,
    val title: String,
    val version: String,
    val author: String,
    val description: String,
    val category: String,
    val filename: String,
    val sizeBytes: Long,
    val iconSymbol: String = "⚡"
)

data class NotificationPayload(
    val appName: String,
    val packageName: String,
    val title: String,
    val body: String,
    val category: String = "MESSAGE",
    val alertStyle: String = "CHIME",
    val ledColorHex: String = "#00E5FF",
    val timestampMs: Long = System.currentTimeMillis()
)
