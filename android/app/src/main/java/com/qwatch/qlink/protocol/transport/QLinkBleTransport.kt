package com.qwatch.qlink.protocol.transport

import android.annotation.SuppressLint
import android.bluetooth.*
import android.content.Context
import com.qwatch.qlink.model.*
import com.qwatch.qlink.protocol.QLinkConstants
import com.qwatch.qlink.protocol.parser.QLinkPacketParser
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import org.json.JSONObject
import java.io.IOException

@SuppressLint("MissingPermission")
class QLinkBleTransport(
    private val context: Context,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO + SupervisorJob())
) : QLinkTransport {

    private val bluetoothManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager
    private val bluetoothAdapter = bluetoothManager?.adapter
    private var bluetoothGatt: BluetoothGatt? = null

    private var cmdCharacteristic: BluetoothGattCharacteristic? = null
    private var telemetryCharacteristic: BluetoothGattCharacteristic? = null

    private val _connectionState = MutableStateFlow(ConnectionState.DISCONNECTED)
    override val connectionState: Flow<ConnectionState> = _connectionState.asStateFlow()

    private val _telemetryFlow = MutableSharedFlow<TelemetrySnapshot>(extraBufferCapacity = 16)
    override val telemetryFlow: Flow<TelemetrySnapshot> = _telemetryFlow.asSharedFlow()

    private val _displayFrameFlow = MutableSharedFlow<DisplayFrame>(extraBufferCapacity = 4)
    override val displayFrameFlow: Flow<DisplayFrame> = _displayFrameFlow.asSharedFlow()

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    _connectionState.value = ConnectionState.CONNECTED_BLE
                    gatt?.requestMtu(512)
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    _connectionState.value = ConnectionState.DISCONNECTED
                    cleanGatt()
                }
            }
        }

        override fun onMtuChanged(gatt: BluetoothGatt?, mtu: Int, status: Int) {
            gatt?.discoverServices()
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS && gatt != null) {
                val service = gatt.getService(QLinkConstants.SERVICE_UUID)
                if (service != null) {
                    cmdCharacteristic = service.getCharacteristic(QLinkConstants.CHAR_COMMAND_UUID)
                    telemetryCharacteristic = service.getCharacteristic(QLinkConstants.CHAR_TELEMETRY_UUID)

                    // Enable telemetry notification
                    telemetryCharacteristic?.let { char ->
                        gatt.setCharacteristicNotification(char, true)
                        val desc = char.getDescriptor(QLinkConstants.CLIENT_CONFIG_DESCRIPTOR_UUID)
                        desc?.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                        gatt.writeDescriptor(desc)
                    }
                }
            }
        }

        @Deprecated("Deprecated in Java")
        override fun onCharacteristicChanged(gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?) {
            if (characteristic?.uuid == QLinkConstants.CHAR_TELEMETRY_UUID) {
                val bytes = characteristic.value ?: return
                val snapshot = QLinkPacketParser.parseCompactTelemetry(bytes)
                if (snapshot != null) {
                    scope.launch { _telemetryFlow.emit(snapshot) }
                }
            }
        }
    }

    override suspend fun connect(target: String): Result<Boolean> = withContext(Dispatchers.IO) {
        if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled) {
            return@withContext Result.failure(IOException("Bluetooth not available or disabled"))
        }

        _connectionState.value = ConnectionState.CONNECTING
        try {
            val device = bluetoothAdapter.getRemoteDevice(target)
            bluetoothGatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
            Result.success(true)
        } catch (e: Exception) {
            _connectionState.value = ConnectionState.ERROR
            Result.failure(e)
        }
    }

    override suspend fun disconnect() = withContext(Dispatchers.IO) {
        cleanGatt()
        _connectionState.value = ConnectionState.DISCONNECTED
    }

    private fun cleanGatt() {
        try {
            bluetoothGatt?.disconnect()
            bluetoothGatt?.close()
        } catch (_: Exception) {}
        bluetoothGatt = null
        cmdCharacteristic = null
        telemetryCharacteristic = null
    }

    override fun isConnected(): Boolean {
        return _connectionState.value == ConnectionState.CONNECTED_BLE && bluetoothGatt != null
    }

    override suspend fun getDeviceInfo(): Result<DeviceInfo> {
        // Return baseline device info over BLE
        return Result.success(DeviceInfo(deviceName = "Q-Watch BLE"))
    }

    override suspend fun getTelemetry(): Result<TelemetrySnapshot> {
        // Telemetry arrives continuously via notifications on telemetryFlow
        return Result.success(TelemetrySnapshot())
    }

    override suspend fun injectButton(type: ButtonType, event: ButtonEventType): Result<Boolean> = withContext(Dispatchers.IO) {
        val char = cmdCharacteristic ?: return@withContext Result.failure(IOException("BLE Command characteristic not ready"))
        val gatt = bluetoothGatt ?: return@withContext Result.failure(IOException("BLE Not connected"))

        val json = JSONObject().apply {
            put("action", "BUTTON")
            put("button", type.name)
            put("event", event.name)
        }

        char.value = json.toString().toByteArray(Charsets.UTF_8)
        val success = gatt.writeCharacteristic(char)
        Result.success(success)
    }

    override suspend fun sendNotification(payload: NotificationPayload): Result<Boolean> = withContext(Dispatchers.IO) {
        val char = cmdCharacteristic ?: return@withContext Result.failure(IOException("BLE Command characteristic not ready"))
        val gatt = bluetoothGatt ?: return@withContext Result.failure(IOException("BLE Not connected"))

        val json = JSONObject().apply {
            put("action", "NOTIFY")
            put("app_name", payload.appName)
            put("title", payload.title)
            put("body", payload.body)
            put("alert", payload.alertStyle)
        }

        char.value = json.toString().toByteArray(Charsets.UTF_8)
        val success = gatt.writeCharacteristic(char)
        Result.success(success)
    }

    override suspend fun syncTime(epochSec: Long, tzOffsetSec: Int): Result<Boolean> = withContext(Dispatchers.IO) {
        val char = cmdCharacteristic ?: return@withContext Result.failure(IOException("BLE Command characteristic not ready"))
        val gatt = bluetoothGatt ?: return@withContext Result.failure(IOException("BLE Not connected"))

        val json = JSONObject().apply {
            put("action", "SYNC_TIME")
            put("epoch", epochSec)
            put("tz", tzOffsetSec)
        }

        char.value = json.toString().toByteArray(Charsets.UTF_8)
        val success = gatt.writeCharacteristic(char)
        Result.success(success)
    }

    override suspend fun syncWeather(
        city: String,
        tempC: Float,
        humidity: Int,
        code: Int,
        desc: String,
        hi: Float,
        lo: Float
    ): Result<Boolean> = withContext(Dispatchers.IO) {
        val char = cmdCharacteristic ?: return@withContext Result.failure(IOException("BLE Command characteristic not ready"))
        val gatt = bluetoothGatt ?: return@withContext Result.failure(IOException("BLE Not connected"))

        val json = JSONObject().apply {
            put("action", "SYNC_WEATHER")
            put("temp", tempC)
            put("hum", humidity)
            put("desc", desc)
        }

        char.value = json.toString().toByteArray(Charsets.UTF_8)
        val success = gatt.writeCharacteristic(char)
        Result.success(success)
    }

    override suspend fun fetchDisplayFrame(): Result<DisplayFrame> {
        return Result.failure(UnsupportedOperationException("High-speed display streaming is optimized for Wi-Fi transport"))
    }

    override suspend fun setDisplayStreaming(enabled: Boolean, fps: Int): Result<Boolean> {
        return Result.failure(UnsupportedOperationException("High-speed display streaming is optimized for Wi-Fi transport"))
    }

    override suspend fun listFiles(path: String): Result<List<WatchFile>> {
        return Result.failure(UnsupportedOperationException("File browsing uses Wi-Fi transport"))
    }

    override suspend fun downloadFile(path: String): Result<ByteArray> {
        return Result.failure(UnsupportedOperationException("File download uses Wi-Fi transport"))
    }

    override suspend fun uploadFile(path: String, data: ByteArray): Result<Boolean> {
        return Result.failure(UnsupportedOperationException("File upload uses Wi-Fi transport"))
    }

    override suspend fun deleteFile(path: String): Result<Boolean> {
        return Result.failure(UnsupportedOperationException("File deletion uses Wi-Fi transport"))
    }

    override suspend fun installQApp(filename: String, data: ByteArray): Result<Boolean> {
        return Result.failure(UnsupportedOperationException("Use Wi-Fi transport for .qapp installation"))
    }
}
