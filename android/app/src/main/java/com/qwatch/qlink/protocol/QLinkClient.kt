package com.qwatch.qlink.protocol

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import com.qwatch.qlink.model.*
import com.qwatch.qlink.protocol.ble.QWatchBleScanner
import com.qwatch.qlink.protocol.transport.QLinkBleTransport
import com.qwatch.qlink.protocol.transport.QLinkTransport
import com.qwatch.qlink.protocol.transport.QLinkWifiTransport
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import java.util.TimeZone

class QLinkClient private constructor() {

    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    private var wifiTransport: QLinkWifiTransport? = null
    private var bleTransport: QLinkBleTransport? = null
    private var currentTransport: QLinkTransport? = null

    var bleScanner: QWatchBleScanner? = null
        private set

    private var appContext: Context? = null
    private var autoReconnectJob: Job? = null

    private val _connectionState = MutableStateFlow(ConnectionState.DISCONNECTED)
    val connectionState: StateFlow<ConnectionState> = _connectionState.asStateFlow()

    private val _telemetry = MutableStateFlow(TelemetrySnapshot())
    val telemetry: StateFlow<TelemetrySnapshot> = _telemetry.asStateFlow()

    private val _displayFrame = MutableStateFlow(DisplayFrame())
    val displayFrame: StateFlow<DisplayFrame> = _displayFrame.asStateFlow()

    private val _autoConnectBleFlow = MutableStateFlow(true)
    val autoConnectBleFlow: StateFlow<Boolean> = _autoConnectBleFlow.asStateFlow()

    private val _idleScanState = MutableStateFlow(false)
    private val _emptyScanResults = MutableStateFlow<List<com.qwatch.qlink.protocol.ble.DiscoveredBleDevice>>(emptyList())

    val isScanningBle: StateFlow<Boolean>
        get() = bleScanner?.isScanning ?: _idleScanState

    val bleScanResults: StateFlow<List<com.qwatch.qlink.protocol.ble.DiscoveredBleDevice>>
        get() = bleScanner?.scanResults ?: _emptyScanResults

    private val bluetoothStateReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            if (intent?.action == BluetoothAdapter.ACTION_STATE_CHANGED) {
                val state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR)
                if (state == BluetoothAdapter.STATE_ON) {
                    // Bluetooth was just turned on! Auto-connect if enabled
                    scope.launch {
                        delay(500)
                        autoConnectBleIfEnabled()
                    }
                }
            }
        }
    }

    fun init(context: Context) {
        val app = context.applicationContext
        appContext = app

        val prefs = app.getSharedPreferences("qlink_prefs", Context.MODE_PRIVATE)
        _autoConnectBleFlow.value = prefs.getBoolean("auto_connect_ble", true)

        wifiTransport = QLinkWifiTransport(scope)
        bleTransport = QLinkBleTransport(app, scope)
        bleScanner = QWatchBleScanner(app, scope)

        // Register bluetooth state listener for auto-connect
        try {
            val filter = IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED)
            app.registerReceiver(bluetoothStateReceiver, filter)
        } catch (_: Exception) {}

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
                if (currentTransport === bt) {
                    _connectionState.value = state
                    if (state == ConnectionState.DISCONNECTED) {
                        scheduleBleAutoReconnect()
                    } else if (state == ConnectionState.CONNECTED_BLE) {
                        autoReconnectJob?.cancel()
                    }
                }
            }.launchIn(scope)

            bt.telemetryFlow.onEach { snapshot ->
                if (currentTransport === bt) _telemetry.value = snapshot
            }.launchIn(scope)
        }
    }

    suspend fun connectWifi(host: String = QLinkConstants.DEFAULT_HOTSPOT_IP): Result<Boolean> {
        val wt = wifiTransport ?: return Result.failure(IllegalStateException("QLinkClient not initialized"))
        autoReconnectJob?.cancel()
        currentTransport?.disconnect()
        currentTransport = wt
        return wt.connect(host)
    }

    suspend fun connectBle(macAddress: String): Result<Boolean> {
        val bt = bleTransport ?: return Result.failure(IllegalStateException("QLinkClient not initialized"))
        autoReconnectJob?.cancel()
        currentTransport?.disconnect()
        currentTransport = bt

        // Save last connected MAC
        appContext?.let { ctx ->
            ctx.getSharedPreferences("qlink_prefs", Context.MODE_PRIVATE)
                .edit().putString("ble_mac", macAddress).apply()
        }

        return bt.connect(macAddress)
    }

    suspend fun disconnect() {
        autoReconnectJob?.cancel()
        currentTransport?.disconnect()
        currentTransport = null
        _connectionState.value = ConnectionState.DISCONNECTED
    }

    fun isConnected(): Boolean {
        return currentTransport?.isConnected() == true
    }

    fun isBleConnected(): Boolean {
        return currentTransport === bleTransport && bleTransport?.isConnected() == true
    }

    fun isBluetoothOn(): Boolean {
        val bm = appContext?.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager
        return bm?.adapter?.isEnabled == true
    }

    fun isBleAutoConnectEnabled(): Boolean {
        return _autoConnectBleFlow.value
    }

    fun setBleAutoConnectEnabled(enabled: Boolean) {
        _autoConnectBleFlow.value = enabled
        appContext?.let { ctx ->
            ctx.getSharedPreferences("qlink_prefs", Context.MODE_PRIVATE)
                .edit().putBoolean("auto_connect_ble", enabled).apply()
        }
        if (enabled && !isConnected() && isBluetoothOn()) {
            scope.launch { autoConnectBleIfEnabled() }
        }
    }

    fun getSavedBleMac(): String {
        val prefs = appContext?.getSharedPreferences("qlink_prefs", Context.MODE_PRIVATE)
        return prefs?.getString("ble_mac", "CC:7B:5C:80:12:34") ?: "CC:7B:5C:80:12:34"
    }

    fun setSavedBleMac(mac: String) {
        appContext?.let { ctx ->
            ctx.getSharedPreferences("qlink_prefs", Context.MODE_PRIVATE)
                .edit().putString("ble_mac", mac).apply()
        }
    }

    suspend fun autoConnectBleIfEnabled(): Boolean {
        if (!_autoConnectBleFlow.value || !isBluetoothOn() || isConnected()) return false
        val targetMac = getSavedBleMac()
        if (targetMac.isNotBlank()) {
            val res = connectBle(targetMac)
            return res.isSuccess
        }
        return false
    }

    private fun scheduleBleAutoReconnect() {
        if (!_autoConnectBleFlow.value || !isBluetoothOn()) return
        autoReconnectJob?.cancel()
        autoReconnectJob = scope.launch {
            delay(3000)
            if (!isConnected() && isBluetoothOn() && _autoConnectBleFlow.value) {
                autoConnectBleIfEnabled()
            }
        }
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

    suspend fun uploadFileWithProgress(
        path: String,
        data: ByteArray,
        onProgress: (com.qwatch.qlink.anim.TransferProgress) -> Unit
    ): Result<Boolean> {
        return currentTransport?.uploadFileWithProgress(path, data, onProgress) ?: Result.failure(IllegalStateException("Not connected"))
    }

    suspend fun getStorageTelemetry(): Result<StorageTelemetry> {
        return currentTransport?.getStorageTelemetry() ?: Result.failure(IllegalStateException("Not connected"))
    }

    suspend fun deleteFile(path: String): Result<Boolean> {
        return currentTransport?.deleteFile(path) ?: Result.failure(IllegalStateException("Not connected"))
    }

    suspend fun installQApp(filename: String, data: ByteArray): Result<Boolean> {
        return currentTransport?.installQApp(filename, data) ?: Result.failure(IllegalStateException("Not connected"))
    }

    suspend fun transmitIr(protocol: String, address: String, command: String, nbits: Int = 32): Result<Boolean> {
        return currentTransport?.transmitIr(protocol, address, command, nbits) ?: Result.success(true)
    }

    companion object {
        val instance: QLinkClient by lazy { QLinkClient() }
    }
}
