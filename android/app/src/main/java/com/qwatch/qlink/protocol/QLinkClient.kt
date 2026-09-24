package com.qwatch.qlink.protocol

import android.content.Context
import com.qwatch.qlink.model.*
import com.qwatch.qlink.protocol.transport.QLinkBleTransport
import com.qwatch.qlink.protocol.transport.QLinkTransport
import com.qwatch.qlink.protocol.transport.QLinkWifiTransport
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.*
import java.util.TimeZone

class QLinkClient private constructor() {

    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    private var wifiTransport: QLinkWifiTransport? = null
    private var bleTransport: QLinkBleTransport? = null
    private var currentTransport: QLinkTransport? = null

    private val _connectionState = MutableStateFlow(ConnectionState.DISCONNECTED)
    val connectionState: StateFlow<ConnectionState> = _connectionState.asStateFlow()

    private val _telemetry = MutableStateFlow(TelemetrySnapshot())
    val telemetry: StateFlow<TelemetrySnapshot> = _telemetry.asStateFlow()

    private val _displayFrame = MutableStateFlow(DisplayFrame())
    val displayFrame: StateFlow<DisplayFrame> = _displayFrame.asStateFlow()

    fun init(context: Context) {
        wifiTransport = QLinkWifiTransport(scope)
        bleTransport = QLinkBleTransport(context.applicationContext, scope)

        // Observe Wi-Fi transport flows
        wifiTransport?.let { wt ->
            wt.connectionState.onEach { state ->
                if (currentTransport === wt) _connectionState.value = state
            }.launchIn(scope)

            wt.telemetryFlow.onEach { snapshot ->
                if (currentTransport === wt) _telemetry.value = snapshot
            }.launchIn(scope)

            wt.displayFrameFlow.onEach { frame ->
                if (currentTransport === wt) _displayFrame.value = frame
            }.launchIn(scope)
        }

        // Observe BLE transport flows
        bleTransport?.let { bt ->
            bt.connectionState.onEach { state ->
                if (currentTransport === bt) _connectionState.value = state
            }.launchIn(scope)

            bt.telemetryFlow.onEach { snapshot ->
                if (currentTransport === bt) _telemetry.value = snapshot
            }.launchIn(scope)
        }
    }

    suspend fun connectWifi(host: String = QLinkConstants.DEFAULT_HOTSPOT_IP): Result<Boolean> {
        val wt = wifiTransport ?: return Result.failure(IllegalStateException("QLinkClient not initialized"))
        currentTransport?.disconnect()
        currentTransport = wt
        return wt.connect(host)
    }

    suspend fun connectBle(macAddress: String): Result<Boolean> {
        val bt = bleTransport ?: return Result.failure(IllegalStateException("QLinkClient not initialized"))
        currentTransport?.disconnect()
        currentTransport = bt
        return bt.connect(macAddress)
    }

    suspend fun disconnect() {
        currentTransport?.disconnect()
        currentTransport = null
        _connectionState.value = ConnectionState.DISCONNECTED
    }

    fun isConnected(): Boolean {
        return currentTransport?.isConnected() == true
    }

    fun getTargetHost(): String {
        return wifiTransport?.getHost() ?: QLinkConstants.DEFAULT_HOTSPOT_IP
    }

    val lastError: StateFlow<String?>
        get() = wifiTransport?.lastError ?: MutableStateFlow(null)

    suspend fun getDeviceInfo(): Result<DeviceInfo> {
        return currentTransport?.getDeviceInfo() ?: Result.failure(IllegalStateException("Not connected"))
    }

    suspend fun injectButton(type: ButtonType, event: ButtonEventType = ButtonEventType.SHORT_PRESS): Result<Boolean> {
        return currentTransport?.injectButton(type, event) ?: Result.failure(IllegalStateException("Not connected"))
    }

    suspend fun sendNotification(payload: NotificationPayload): Result<Boolean> {
        return currentTransport?.sendNotification(payload) ?: Result.failure(IllegalStateException("Not connected"))
    }

    suspend fun syncCurrentTime(): Result<Boolean> {
        val epochSec = System.currentTimeMillis() / 1000
        val tzOffsetSec = TimeZone.getDefault().rawOffset / 1000
        return currentTransport?.syncTime(epochSec, tzOffsetSec) ?: Result.failure(IllegalStateException("Not connected"))
    }

    suspend fun syncWeather(
        city: String,
        tempC: Float,
        humidity: Int,
        code: Int,
        desc: String,
        hi: Float,
        lo: Float
    ): Result<Boolean> {
        return currentTransport?.syncWeather(city, tempC, humidity, code, desc, hi, lo)
            ?: Result.failure(IllegalStateException("Not connected"))
    }

    suspend fun setDisplayStreaming(enabled: Boolean, fps: Int = 20): Result<Boolean> {
        return currentTransport?.setDisplayStreaming(enabled, fps)
            ?: Result.failure(IllegalStateException("Not connected"))
    }

    suspend fun listFiles(path: String = "/"): Result<List<WatchFile>> {
        return currentTransport?.listFiles(path) ?: Result.failure(IllegalStateException("Not connected"))
    }

    suspend fun downloadFile(path: String): Result<ByteArray> {
        return currentTransport?.downloadFile(path) ?: Result.failure(IllegalStateException("Not connected"))
    }

    suspend fun uploadFile(path: String, data: ByteArray): Result<Boolean> {
        return currentTransport?.uploadFile(path, data) ?: Result.failure(IllegalStateException("Not connected"))
    }

    suspend fun deleteFile(path: String): Result<Boolean> {
        return currentTransport?.deleteFile(path) ?: Result.failure(IllegalStateException("Not connected"))
    }

    suspend fun installQApp(filename: String, data: ByteArray): Result<Boolean> {
        return currentTransport?.installQApp(filename, data) ?: Result.failure(IllegalStateException("Not connected"))
    }

    companion object {
        val instance: QLinkClient by lazy { QLinkClient() }
    }
}
