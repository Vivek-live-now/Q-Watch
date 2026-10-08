package com.qwatch.qlink.protocol.transport

import android.annotation.SuppressLint
import android.bluetooth.*
import android.content.Context
import android.os.Build
import com.qwatch.qlink.anim.TransferProgress
import com.qwatch.qlink.model.*
import com.qwatch.qlink.protocol.QLinkConstants
import com.qwatch.qlink.protocol.parser.QLinkPacketParser
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.first
import org.json.JSONArray
import org.json.JSONObject
import java.io.IOException
import java.util.concurrent.atomic.AtomicInteger

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
    private var fileCharacteristic: BluetoothGattCharacteristic? = null

    private var currentMtu = 23
    private var serviceDiscoveryJob: Job? = null
    private var notificationSetupJob: Job? = null

    private val _connectionState = MutableStateFlow(ConnectionState.DISCONNECTED)
    override val connectionState: Flow<ConnectionState> = _connectionState.asStateFlow()

    private val _telemetryFlow = MutableSharedFlow<TelemetrySnapshot>(extraBufferCapacity = 16)
    override val telemetryFlow: Flow<TelemetrySnapshot> = _telemetryFlow.asSharedFlow()

    private val _displayFrameFlow = MutableSharedFlow<DisplayFrame>(extraBufferCapacity = 4)
    override val displayFrameFlow: Flow<DisplayFrame> = _displayFrameFlow.asSharedFlow()

    private val _fileResponseFlow = MutableSharedFlow<String>(extraBufferCapacity = 16)
    private val _fileDownloadChunkFlow = MutableSharedFlow<ByteArray>(extraBufferCapacity = 64)

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    _connectionState.value = ConnectionState.CONNECTED_BLE
                    gatt?.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH)
                    val requested = gatt?.requestMtu(512) ?: false
                    // Fallback to service discovery if MTU request not supported or delayed
                    serviceDiscoveryJob?.cancel()
                    serviceDiscoveryJob = scope.launch {
                        delay(1000)
                        if (cmdCharacteristic == null && gatt != null) {
                            gatt.discoverServices()
                        }
                    }
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    _connectionState.value = ConnectionState.DISCONNECTED
                    cleanGatt()
                }
            }
        }

        override fun onMtuChanged(gatt: BluetoothGatt?, mtu: Int, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                currentMtu = mtu
            }
            gatt?.discoverServices()
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
            serviceDiscoveryJob?.cancel()
            if (status == BluetoothGatt.GATT_SUCCESS && gatt != null) {
                val service = gatt.getService(QLinkConstants.SERVICE_UUID)
                if (service != null) {
                    cmdCharacteristic = service.getCharacteristic(QLinkConstants.CHAR_COMMAND_UUID)
                    telemetryCharacteristic = service.getCharacteristic(QLinkConstants.CHAR_TELEMETRY_UUID)
                    fileCharacteristic = service.getCharacteristic(QLinkConstants.CHAR_FILE_UUID)

                    // Strictly serialize descriptor writes via onDescriptorWrite
                    notificationSetupJob?.cancel()
                    notificationSetupJob = scope.launch {
                        if (!isActive || bluetoothGatt == null) return@launch
                        telemetryCharacteristic?.let { char ->
                            enableNotification(gatt, char)
                        }
                    }
                }
            }
        }

        override fun onDescriptorWrite(gatt: BluetoothGatt?, descriptor: BluetoothGattDescriptor?, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS && descriptor != null && gatt != null) {
                if (descriptor.characteristic?.uuid == QLinkConstants.CHAR_TELEMETRY_UUID) {
                    notificationSetupJob?.cancel()
                    notificationSetupJob = scope.launch {
                        if (!isActive || bluetoothGatt == null) return@launch
                        fileCharacteristic?.let { char ->
                            enableNotification(gatt, char)
                        }
                    }
                }
            }
        }

        // Modern Android 13+ (API 33+) callback
        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            handleCharacteristicBytes(characteristic.uuid, value)
        }

        // Legacy callback for Android 12 and below
        @Deprecated("Deprecated in Java")
        override fun onCharacteristicChanged(gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?) {
            if (characteristic != null) {
                val bytes = characteristic.value ?: return
                handleCharacteristicBytes(characteristic.uuid, bytes)
            }
        }
    }

    private fun handleCharacteristicBytes(uuid: java.util.UUID, bytes: ByteArray) {
        if (uuid == QLinkConstants.CHAR_TELEMETRY_UUID) {
            val snapshot = QLinkPacketParser.parseCompactTelemetry(bytes)
            if (snapshot != null) {
                scope.launch { _telemetryFlow.emit(snapshot) }
            }
        } else if (uuid == QLinkConstants.CHAR_FILE_UUID) {
            if (bytes.size >= 6 && bytes[0] == 0xFE.toByte() && bytes[1] == 0x02.toByte()) {
                scope.launch { _fileDownloadChunkFlow.emit(bytes) }
            } else {
                val text = String(bytes, Charsets.UTF_8)
                scope.launch { _fileResponseFlow.emit(text) }
            }
        }
    }

    private fun enableNotification(gatt: BluetoothGatt, char: BluetoothGattCharacteristic) {
        gatt.setCharacteristicNotification(char, true)
        val desc = char.getDescriptor(QLinkConstants.CLIENT_CONFIG_DESCRIPTOR_UUID)
        if (desc != null) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                gatt.writeDescriptor(desc, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
            } else {
                @Suppress("DEPRECATION")
                desc.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                @Suppress("DEPRECATION")
                gatt.writeDescriptor(desc)
            }
        }
    }

    private fun writeData(char: BluetoothGattCharacteristic, data: ByteArray, withoutResponse: Boolean = false): Boolean {
        val gatt = bluetoothGatt ?: return false
        val writeType = if (withoutResponse) {
            BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
        } else {
            BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        }

        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            gatt.writeCharacteristic(char, data, writeType) == BluetoothStatusCodes.SUCCESS
        } else {
            @Suppress("DEPRECATION")
            char.value = data
            @Suppress("DEPRECATION")
            char.writeType = writeType
            @Suppress("DEPRECATION")
            gatt.writeCharacteristic(char)
        }
    }

    override suspend fun connect(target: String): Result<Boolean> = withContext(Dispatchers.IO) {
        if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled) {
            return@withContext Result.failure(IOException("Bluetooth not available or disabled"))
        }

        _connectionState.value = ConnectionState.CONNECTING
        try {
            val device = bluetoothAdapter.getRemoteDevice(target)
            cleanGatt()
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
        serviceDiscoveryJob?.cancel()
        serviceDiscoveryJob = null
        notificationSetupJob?.cancel()
        notificationSetupJob = null
        try {
            bluetoothGatt?.disconnect()
            bluetoothGatt?.close()
        } catch (_: Exception) {}
        bluetoothGatt = null
        cmdCharacteristic = null
        telemetryCharacteristic = null
        fileCharacteristic = null
        currentMtu = 23
    }

    override fun isConnected(): Boolean {
        return _connectionState.value == ConnectionState.CONNECTED_BLE && bluetoothGatt != null
    }

    override suspend fun getDeviceInfo(): Result<DeviceInfo> {
        return Result.success(DeviceInfo(deviceName = "Q-Watch BLE"))
    }

    override suspend fun getTelemetry(): Result<TelemetrySnapshot> {
        return Result.success(TelemetrySnapshot())
    }

    override suspend fun injectButton(type: ButtonType, event: ButtonEventType): Result<Boolean> = withContext(Dispatchers.IO) {
        val char = cmdCharacteristic ?: return@withContext Result.failure(IOException("BLE Command characteristic not ready"))
        val json = JSONObject().apply {
            put("action", "BUTTON")
            put("button", type.name)
            put("event", event.name)
        }
        val success = writeData(char, json.toString().toByteArray(Charsets.UTF_8))
        Result.success(success)
    }

    override suspend fun sendNotification(payload: NotificationPayload): Result<Boolean> = withContext(Dispatchers.IO) {
        val char = cmdCharacteristic ?: return@withContext Result.failure(IOException("BLE Command characteristic not ready"))
        val json = JSONObject().apply {
            put("action", "NOTIFY")
            put("app_name", payload.appName)
            put("title", payload.title)
            put("body", payload.body)
            put("alert", payload.alertStyle)
        }
        val success = writeData(char, json.toString().toByteArray(Charsets.UTF_8))
        Result.success(success)
    }

    override suspend fun syncTime(epochSec: Long, tzOffsetSec: Int): Result<Boolean> = withContext(Dispatchers.IO) {
        val char = cmdCharacteristic ?: return@withContext Result.failure(IOException("BLE Command characteristic not ready"))
        val json = JSONObject().apply {
            put("action", "SYNC_TIME")
            put("epoch", epochSec)
            put("tz", tzOffsetSec)
        }
        val success = writeData(char, json.toString().toByteArray(Charsets.UTF_8))
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
        val json = JSONObject().apply {
            put("action", "SYNC_WEATHER")
            put("temp", tempC)
            put("hum", humidity)
            put("desc", desc)
        }
        val success = writeData(char, json.toString().toByteArray(Charsets.UTF_8))
        Result.success(success)
    }

    override suspend fun fetchDisplayFrame(): Result<DisplayFrame> {
        return Result.failure(UnsupportedOperationException("High-speed display streaming is optimized for Wi-Fi transport"))
    }

    override suspend fun setDisplayStreaming(enabled: Boolean, fps: Int): Result<Boolean> {
        return Result.failure(UnsupportedOperationException("High-speed display streaming is optimized for Wi-Fi transport"))
    }

    override suspend fun listFiles(path: String): Result<List<WatchFile>> = withContext(Dispatchers.IO) {
        val char = fileCharacteristic ?: return@withContext Result.failure(IOException("BLE File characteristic not ready"))
        val json = JSONObject().apply {
            put("cmd", "LIST")
            put("path", path)
        }
        writeData(char, json.toString().toByteArray(Charsets.UTF_8))

        // Wait for list response with filter.first()
        try {
            val resp = withTimeout(3500) {
                _fileResponseFlow.filter { it.contains("LIST_RESP") }.first()
            }
            val obj = JSONObject(resp)
            val filesArr = obj.optJSONArray("files") ?: JSONArray()
            val list = mutableListOf<WatchFile>()
            val normPath = if (path.endsWith("/")) path else "$path/"
            for (i in 0 until filesArr.length()) {
                val f = filesArr.getJSONObject(i)
                val fname = f.getString("name")
                list.add(
                    WatchFile(
                        name = fname,
                        path = f.optString("path", if (fname.startsWith("/")) fname else "${normPath.removeSuffix("/")}/$fname"),
                        sizeBytes = f.optLong("size", 0L),
                        isDirectory = f.optBoolean("is_dir", false)
                    )
                )
            }
            Result.success(list)
        } catch (_: Exception) {
            // Fallback list
            Result.success(emptyList())
        }
    }

    override suspend fun downloadFile(path: String): Result<ByteArray> = withContext(Dispatchers.IO) {
        val char = fileCharacteristic ?: return@withContext Result.failure(IOException("BLE File characteristic not ready"))
        val json = JSONObject().apply {
            put("cmd", "DOWNLOAD")
            put("path", path)
        }
        if (!writeData(char, json.toString().toByteArray(Charsets.UTF_8))) {
            return@withContext Result.failure(IOException("Failed to send BLE download request"))
        }

        try {
            val startResp = withTimeout(4000) {
                _fileResponseFlow.filter { it.contains("DOWNLOAD_START") || it.contains("\"code\":404") || it.contains("\"code\":400") }.first()
            }
            if (startResp.contains("\"code\":404") || startResp.contains("\"code\":400")) {
                return@withContext Result.failure(IOException("Watch reported error downloading $path: $startResp"))
            }

            val startObj = JSONObject(startResp)
            val totalSize = startObj.optLong("size", 0L)
            if (totalSize == 0L) {
                return@withContext Result.success(ByteArray(0))
            }

            val outputStream = java.io.ByteArrayOutputStream(totalSize.toInt().coerceAtLeast(1024))
            withTimeout(30000) {
                while (outputStream.size() < totalSize) {
                    val chunk = withTimeout(4000) {
                        _fileDownloadChunkFlow.first()
                    }
                    if (chunk.size >= 6) {
                        val chunkLen = ((chunk[4].toInt() and 0xFF) shl 8) or (chunk[5].toInt() and 0xFF)
                        val actualDataLen = minOf(chunkLen, chunk.size - 6)
                        if (actualDataLen > 0) {
                            outputStream.write(chunk, 6, actualDataLen)
                        }
                    }
                }
            }
            Result.success(outputStream.toByteArray())
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    override suspend fun uploadFile(path: String, data: ByteArray): Result<Boolean> {
        return uploadFileWithProgress(path, data) {}
    }

    override suspend fun uploadFileWithProgress(
        path: String,
        data: ByteArray,
        onProgress: (TransferProgress) -> Unit
    ): Result<Boolean> = withContext(Dispatchers.IO) {
        val char = fileCharacteristic ?: return@withContext Result.failure(IOException("BLE File characteristic not ready"))

        val totalSize = data.size.toLong()
        onProgress(
            TransferProgress(
                progress = 0f,
                percent = 0,
                bytesTransferred = 0L,
                totalBytes = totalSize
            )
        )

        // 1. Send START packet
        val startJson = JSONObject().apply {
            put("cmd", "START")
            put("path", path)
            put("size", data.size)
        }
        if (!writeData(char, startJson.toString().toByteArray(Charsets.UTF_8))) {
            return@withContext Result.failure(IOException("Failed to send BLE file START"))
        }
        delay(40) // Allow watch LittleFS to open file handle

        // 2. Stream chunk packets
        // Maximum chunk size governed by MTU (subtract 3 bytes for ATT header and 6 bytes for framing)
        val maxChunk = (if (currentMtu > 30) currentMtu - 12 else 20).coerceIn(20, 240)
        var offset = 0
        var seq = 0

        while (offset < data.size) {
            val chunkLen = minOf(maxChunk, data.size - offset)
            val packet = ByteArray(6 + chunkLen)
            packet[0] = 0xFE.toByte() // Magic Hi
            packet[1] = 0x01.toByte() // Magic Lo
            packet[2] = ((seq shr 8) and 0xFF).toByte()
            packet[3] = (seq and 0xFF).toByte()
            packet[4] = ((chunkLen shr 8) and 0xFF).toByte()
            packet[5] = (chunkLen and 0xFF).toByte()
            System.arraycopy(data, offset, packet, 6, chunkLen)

            val success = writeData(char, packet, withoutResponse = true)
            if (!success) {
                delay(20)
                writeData(char, packet, withoutResponse = false)
            }

            offset += chunkLen
            seq++
            val progressFrac = if (totalSize > 0L) (offset.toFloat() / totalSize.toFloat()) else 1f
            val pct = (progressFrac * 100f).toInt().coerceIn(0, 100)
            onProgress(
                TransferProgress(
                    progress = progressFrac,
                    percent = pct,
                    bytesTransferred = offset.toLong(),
                    totalBytes = totalSize
                )
            )

            // Gentle delay for BLE stack throughput stability
            delay(12)
        }

        // 3. Send FINISH packet
        val finishJson = JSONObject().apply {
            put("cmd", "FINISH")
            put("path", path)
            put("size", data.size)
        }
        writeData(char, finishJson.toString().toByteArray(Charsets.UTF_8))
        delay(30)
        onProgress(
            TransferProgress(
                progress = 1f,
                percent = 100,
                bytesTransferred = totalSize,
                totalBytes = totalSize
            )
        )

        Result.success(true)
    }

    override suspend fun deleteFile(path: String): Result<Boolean> = withContext(Dispatchers.IO) {
        val char = fileCharacteristic ?: return@withContext Result.failure(IOException("BLE File characteristic not ready"))
        val json = JSONObject().apply {
            put("cmd", "DELETE")
            put("path", path)
        }
        val success = writeData(char, json.toString().toByteArray(Charsets.UTF_8))
        Result.success(success)
    }

    override suspend fun createDirectory(path: String): Result<Boolean> = withContext(Dispatchers.IO) {
        val char = fileCharacteristic ?: return@withContext Result.failure(IOException("BLE File characteristic not ready"))
        val json = JSONObject().apply {
            put("cmd", "MKDIR")
            put("path", path)
        }
        val success = writeData(char, json.toString().toByteArray(Charsets.UTF_8))
        Result.success(success)
    }

    override suspend fun renameFile(oldPath: String, newPath: String): Result<Boolean> = withContext(Dispatchers.IO) {
        val char = fileCharacteristic ?: return@withContext Result.failure(IOException("BLE File characteristic not ready"))
        val json = JSONObject().apply {
            put("cmd", "RENAME")
            put("from", oldPath)
            put("to", newPath)
        }
        val success = writeData(char, json.toString().toByteArray(Charsets.UTF_8))
        Result.success(success)
    }

    override suspend fun copyFile(sourcePath: String, destPath: String): Result<Boolean> = withContext(Dispatchers.IO) {
        val char = fileCharacteristic ?: return@withContext Result.failure(IOException("BLE File characteristic not ready"))
        val json = JSONObject().apply {
            put("cmd", "COPY")
            put("src", sourcePath)
            put("dst", destPath)
        }
        val success = writeData(char, json.toString().toByteArray(Charsets.UTF_8))
        Result.success(success)
    }

    override suspend fun getStorageTelemetry(): Result<StorageTelemetry> = withContext(Dispatchers.IO) {
        val char = fileCharacteristic ?: return@withContext Result.success(StorageTelemetry())
        val json = JSONObject().apply {
            put("cmd", "STORAGE")
        }
        if (!writeData(char, json.toString().toByteArray(Charsets.UTF_8))) {
            return@withContext Result.success(StorageTelemetry())
        }
        try {
            val resp = withTimeout(2000) {
                _fileResponseFlow.filter { it.contains("fs_total_bytes") }.first()
            }
            val obj = JSONObject(resp)
            val total = obj.optLong("fs_total_bytes", 896L * 1024L)
            val used = obj.optLong("fs_used_bytes", 0L)
            val free = obj.optLong("fs_free_bytes", total - used)
            val pct = obj.optDouble("free_pct", 100.0).toFloat()
            val animCount = obj.optInt("anim_count", 0)
            val freeSlots = obj.optInt("free_anim_slots", (free / (20 * 1024)).toInt())
            Result.success(
                StorageTelemetry(
                    fsTotalBytes = total,
                    fsUsedBytes = used,
                    fsFreeBytes = free,
                    freePct = pct,
                    animCount = animCount,
                    freeAnimSlots = freeSlots
                )
            )
        } catch (_: Exception) {
            Result.success(StorageTelemetry())
        }
    }

    override suspend fun installQApp(filename: String, data: ByteArray): Result<Boolean> {
        return uploadFile("/apps/$filename", data)
    }

    override suspend fun transmitIr(protocol: String, address: String, command: String, nbits: Int): Result<Boolean> = withContext(Dispatchers.IO) {
        val char = cmdCharacteristic ?: return@withContext Result.failure(IOException("BLE Command characteristic not ready"))
        val json = JSONObject().apply {
            put("action", "IR_TRANSMIT")
            put("protocol", protocol)
            put("address", address.removePrefix("0x").toLongOrNull(16) ?: 0L)
            put("command", command.removePrefix("0x").toLongOrNull(16) ?: 0L)
            put("nbits", nbits)
        }
        val ok = writeData(char, json.toString().toByteArray(Charsets.UTF_8))
        Result.success(ok)
    }
}
