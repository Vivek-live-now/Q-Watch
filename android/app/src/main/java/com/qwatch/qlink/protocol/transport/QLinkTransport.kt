package com.qwatch.qlink.protocol.transport

import com.qwatch.qlink.model.*
import kotlinx.coroutines.flow.Flow

interface QLinkTransport {
    val connectionState: Flow<ConnectionState>
    val telemetryFlow: Flow<TelemetrySnapshot>
    val displayFrameFlow: Flow<DisplayFrame>

    suspend fun connect(target: String): Result<Boolean>
    suspend fun disconnect()
    fun isConnected(): Boolean

    suspend fun getDeviceInfo(): Result<DeviceInfo>
    suspend fun getTelemetry(): Result<TelemetrySnapshot>

    suspend fun injectButton(type: ButtonType, event: ButtonEventType): Result<Boolean>
    suspend fun sendNotification(payload: NotificationPayload): Result<Boolean>

    suspend fun syncTime(epochSec: Long, tzOffsetSec: Int): Result<Boolean>
    suspend fun syncWeather(city: String, tempC: Float, humidity: Int, code: Int, desc: String, hi: Float, lo: Float): Result<Boolean>

    suspend fun fetchDisplayFrame(): Result<DisplayFrame>
    suspend fun setDisplayStreaming(enabled: Boolean, fps: Int = 20): Result<Boolean>

    suspend fun listFiles(path: String): Result<List<WatchFile>>
    suspend fun downloadFile(path: String): Result<ByteArray>
    suspend fun uploadFile(path: String, data: ByteArray): Result<Boolean>
    suspend fun deleteFile(path: String): Result<Boolean>
    suspend fun installQApp(filename: String, data: ByteArray): Result<Boolean>
}
