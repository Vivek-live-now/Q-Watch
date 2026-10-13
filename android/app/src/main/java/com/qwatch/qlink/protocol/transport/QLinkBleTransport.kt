package com.qwatch.qlink.protocol.transport

import android.annotation.SuppressLint
import android.bluetooth.*
import android.content.Context
import android.os.Build
import com.qwatch.qlink.anim.TransferProgress
import com.qwatch.qlink.irdb.IrParsedButton
import com.qwatch.qlink.model.*
import com.qwatch.qlink.protocol.QLinkConstants
import com.qwatch.qlink.protocol.parser.QLinkPacketParser
import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
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

    private val _fileResponseFlow = MutableSharedFlow<String>(replay = 1, extraBufferCapacity = 64)
    private val _fileDownloadChunkFlow = MutableSharedFlow<ByteArray>(extraBufferCapacity = 256)
    private var activeDownloadChannel: Channel<ByteArray>? = null

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    _connectionState.value = ConnectionState.CONNECTING
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

                    // Fallback timer to transition to CONNECTED_BLE in case descriptors don't callback
                    scope.launch {
                        delay(1200)
                        if (_connectionState.value == ConnectionState.CONNECTING && cmdCharacteristic != null) {
                            _connectionState.value = ConnectionState.CONNECTED_BLE
                        }
                    }

                    // Strictly serialize descriptor writes via onDescriptorWrite
                    notificationSetupJob?.cancel()
                    notificationSetupJob = scope.launch {
                        if (!isActive || bluetoothGatt == null) return@launch
                        telemetryCharacteristic?.let { char ->
                            enableNotification(gatt, char)
                        } ?: fileCharacteristic?.let { char ->
                            enableNotification(gatt, char)
                        } ?: run {
                            _connectionState.value = ConnectionState.CONNECTED_BLE
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
                        } ?: run {
                            _connectionState.value = ConnectionState.CONNECTED_BLE
                        }
                    }
                } else if (descriptor.characteristic?.uuid == QLinkConstants.CHAR_FILE_UUID) {
                    _connectionState.value = ConnectionState.CONNECTED_BLE
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
                activeDownloadChannel?.trySend(bytes)
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
        activeDownloadChannel?.close()
        activeDownloadChannel = null
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
        _fileResponseFlow.resetReplayCache()
        if (!writeData(char, json.toString().toByteArray(Charsets.UTF_8))) {
            return@withContext Result.failure(IOException("Failed to send BLE list command"))
        }

        // Wait for list response with filter.first()
        try {
            val resp = withTimeout(4000) {
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
        val chunkChannel = Channel<ByteArray>(capacity = 512)
        activeDownloadChannel = chunkChannel

        try {
            val json = JSONObject().apply {
                put("cmd", "DOWNLOAD")
                put("path", path)
            }
            _fileResponseFlow.resetReplayCache()
            if (!writeData(char, json.toString().toByteArray(Charsets.UTF_8))) {
                return@withContext Result.failure(IOException("Failed to send BLE download request"))
            }

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
            withTimeout(35000) {
                while (outputStream.size() < totalSize) {
                    val chunk = withTimeout(4000) {
                        chunkChannel.receive()
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
        } finally {
            activeDownloadChannel = null
            chunkChannel.close()
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
        _fileResponseFlow.resetReplayCache()
        if (!writeData(char, startJson.toString().toByteArray(Charsets.UTF_8))) {
            return@withContext Result.failure(IOException("Failed to send BLE file START"))
        }

        try {
            val startResp = withTimeout(3000) {
                _fileResponseFlow.filter { it.contains("\"code\":200") || it.contains("\"status\":\"OK\"") || it.contains("\"code\":400") }.first()
            }
            if (startResp.contains("\"code\":400")) {
                return@withContext Result.failure(IOException("Watch rejected file upload START for $path"))
            }
        } catch (_: Exception) {
            // Proceed if response delayed
        }
        delay(20) // Allow watch LittleFS to open file handle

        // 2. Stream chunk packets
        // Maximum chunk size governed by MTU:
        // Framing header is 6 bytes [0xFE, 0x01, seq(2), len(2)]
        // ATT max write payload = currentMtu - 3
        // Therefore max chunkLen = currentMtu - 3 - 6 = currentMtu - 9.
        val maxChunk = (currentMtu - 9).coerceIn(12, 240)
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

            var written = false
            for (attempt in 0 until 5) {
                if (writeData(char, packet, withoutResponse = true)) {
                    written = true
                    break
                }
                delay(20L * (attempt + 1))
            }
            if (!written) {
                delay(30)
                if (!writeData(char, packet, withoutResponse = true)) {
                    return@withContext Result.failure(IOException("Failed to write BLE chunk $seq at offset $offset"))
                }
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

            // Inter-chunk pacing delay to prevent HCI buffer congestion and allow flash write
            delay(30)
        }

        // 3. Send FINISH packet
        val finishJson = JSONObject().apply {
            put("cmd", "FINISH")
            put("path", path)
            put("size", data.size)
        }
        _fileResponseFlow.resetReplayCache()
        writeData(char, finishJson.toString().toByteArray(Charsets.UTF_8))

        try {
            val finishResp = withTimeout(2000) {
                _fileResponseFlow.filter { it.contains("\"code\":200") || it.contains("\"code\":500") || it.contains("\"status\":\"OK\"") }.first()
            }
            if (finishResp.contains("\"code\":500")) {
                return@withContext Result.failure(IOException("Watch reported error finishing upload for $path"))
            }
        } catch (_: Exception) {
            // Succeeded on chunk stream completion
        }

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
        _fileResponseFlow.resetReplayCache()
        val success = writeData(char, json.toString().toByteArray(Charsets.UTF_8))
        if (!success) return@withContext Result.failure(IOException("Failed to send BLE delete"))
        try {
            val resp = withTimeout(2500) {
                _fileResponseFlow.filter { it.contains("\"deleted\"") || it.contains("\"code\":404") || it.contains("\"code\":500") }.first()
            }
            Result.success(!resp.contains("\"code\":404") && !resp.contains("\"code\":500"))
        } catch (_: Exception) {
            Result.success(true)
        }
    }

    override suspend fun createDirectory(path: String): Result<Boolean> = withContext(Dispatchers.IO) {
        val char = fileCharacteristic ?: return@withContext Result.failure(IOException("BLE File characteristic not ready"))
        val json = JSONObject().apply {
            put("cmd", "MKDIR")
            put("path", path)
        }
        _fileResponseFlow.resetReplayCache()
        val success = writeData(char, json.toString().toByteArray(Charsets.UTF_8))
        if (!success) return@withContext Result.failure(IOException("Failed to send BLE mkdir"))
        try {
            val resp = withTimeout(2500) {
                _fileResponseFlow.filter { it.contains("\"created\"") || it.contains("\"code\":500") }.first()
            }
            Result.success(!resp.contains("\"code\":500"))
        } catch (_: Exception) {
            Result.success(true)
        }
    }

    override suspend fun renameFile(oldPath: String, newPath: String): Result<Boolean> = withContext(Dispatchers.IO) {
        val char = fileCharacteristic ?: return@withContext Result.failure(IOException("BLE File characteristic not ready"))
        val json = JSONObject().apply {
            put("cmd", "RENAME")
            put("from", oldPath)
            put("to", newPath)
        }
        _fileResponseFlow.resetReplayCache()
        val success = writeData(char, json.toString().toByteArray(Charsets.UTF_8))
        if (!success) return@withContext Result.failure(IOException("Failed to send BLE rename"))
        try {
            val resp = withTimeout(2500) {
                _fileResponseFlow.filter { it.contains("\"from\"") || it.contains("\"code\":500") }.first()
            }
            Result.success(!resp.contains("\"code\":500"))
        } catch (_: Exception) {
            Result.success(true)
        }
    }

    override suspend fun copyFile(sourcePath: String, destPath: String): Result<Boolean> = withContext(Dispatchers.IO) {
        val char = fileCharacteristic ?: return@withContext Result.failure(IOException("BLE File characteristic not ready"))
        val json = JSONObject().apply {
            put("cmd", "COPY")
            put("src", sourcePath)
            put("dst", destPath)
        }
        _fileResponseFlow.resetReplayCache()
        val success = writeData(char, json.toString().toByteArray(Charsets.UTF_8))
        if (!success) return@withContext Result.failure(IOException("Failed to send BLE copy"))
        try {
            val resp = withTimeout(2500) {
                _fileResponseFlow.filter { it.contains("\"copied\"") || it.contains("\"code\":500") }.first()
            }
            Result.success(!resp.contains("\"code\":500"))
        } catch (_: Exception) {
            Result.success(true)
        }
    }

    override suspend fun getStorageTelemetry(): Result<StorageTelemetry> = withContext(Dispatchers.IO) {
        val char = fileCharacteristic ?: return@withContext Result.success(StorageTelemetry())
        val json = JSONObject().apply {
            put("cmd", "STORAGE")
        }
        _fileResponseFlow.resetReplayCache()
        if (!writeData(char, json.toString().toByteArray(Charsets.UTF_8))) {
            return@withContext Result.success(StorageTelemetry())
        }
        try {
            val resp = withTimeout(2500) {
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
        if (!isConnected()) {
            return@withContext Result.failure(IOException("Q-Watch BLE is not connected"))
        }
        val addrVal = IrParsedButton.parseHexOrFlipperBytes(address)
        val cmdVal = IrParsedButton.parseHexOrFlipperBytes(command)
        val effectiveBits = if (nbits > 0) nbits else IrParsedButton.getDefaultBitsForProtocol(protocol)

        val fileChar = fileCharacteristic
        if (fileChar != null) {
            val json = JSONObject().apply {
                put("cmd", "IR_TRANSMIT")
                put("protocol", protocol)
                put("address", addrVal)
                put("command", cmdVal)
                put("nbits", effectiveBits)
            }
            _fileResponseFlow.resetReplayCache()
            val sent = writeData(fileChar, json.toString().toByteArray(Charsets.UTF_8))
            if (sent) {
                try {
                    val resp = withTimeout(2500) {
                        _fileResponseFlow.filter {
                            it.contains("IR_TRANSMIT") || it.contains("\"code\":200") ||
                            it.contains("\"code\":400") || it.contains("\"status\":\"OK\"") ||
                            it.contains("\"status\":\"ERROR\"")
                        }.first()
                    }
                    if (resp.contains("\"code\":200") || resp.contains("\"status\":\"OK\"")) {
                        return@withContext Result.success(true)
                    } else {
                        return@withContext Result.failure(IOException("Watch rejected IR transmission: $resp"))
                    }
                } catch (_: Exception) {
                    // If timeout waiting for notify, check fallback or return failure
                    return@withContext Result.failure(IOException("Timeout waiting for watch confirmation"))
                }
            }
        }

        // Fallback to command characteristic
        val char = cmdCharacteristic ?: return@withContext Result.failure(IOException("BLE Command characteristic not ready"))
        val json = JSONObject().apply {
            put("action", "IR_TRANSMIT")
            put("protocol", protocol)
            put("address", addrVal)
            put("command", cmdVal)
            put("nbits", effectiveBits)
        }
        val ok = writeData(char, json.toString().toByteArray(Charsets.UTF_8))
        if (ok) Result.success(true) else Result.failure(IOException("Failed to send IR command via BLE"))
    }

    override suspend fun transmitRawIr(timings: List<Int>, frequency: Int, dutyCycle: Float): Result<Boolean> = withContext(Dispatchers.IO) {
        if (!isConnected()) {
            return@withContext Result.failure(IOException("Q-Watch BLE is not connected"))
        }
        val char = fileCharacteristic ?: return@withContext Result.failure(IOException("BLE File characteristic not ready"))
        if (timings.isEmpty()) {
            return@withContext Result.failure(IllegalArgumentException("Raw timings sequence is empty"))
        }
        if (timings.size > 1024) {
            return@withContext Result.failure(IllegalArgumentException("Raw timings count (${timings.size}) exceeds watch capacity (1024)"))
        }
        if (timings.any { it <= 0 || it > 65535 }) {
            return@withContext Result.failure(IllegalArgumentException("Raw timings contain invalid duration values"))
        }

        val freq = if (frequency > 0) frequency else 38000

        // 1. Send IR_RAW_START
        val startJson = JSONObject().apply {
            put("cmd", "IR_RAW_START")
            put("count", timings.size)
            put("frequency", freq)
            put("duty_cycle", dutyCycle)
        }
        _fileResponseFlow.resetReplayCache()
        if (!writeData(char, startJson.toString().toByteArray(Charsets.UTF_8))) {
            return@withContext Result.failure(IOException("Failed to send IR_RAW_START"))
        }
        try {
            val startResp = withTimeout(2500) {
                _fileResponseFlow.filter {
                    it.contains("IR_RAW_START") || it.contains("\"code\":200") ||
                    it.contains("\"code\":400") || it.contains("\"status\":\"ERROR\"")
                }.first()
            }
            if (startResp.contains("\"code\":400") || startResp.contains("\"code\":413") || startResp.contains("\"status\":\"ERROR\"")) {
                return@withContext Result.failure(IOException("Watch rejected raw IR: $startResp"))
            }
        } catch (e: Exception) {
            return@withContext Result.failure(IOException("Timeout waiting for IR_RAW_START ack: ${e.message}"))
        }
        delay(15)

        // 2. Stream chunk packets with magic 0xFE, 0x03
        val rawBytes = ByteArray(timings.size * 2)
        for (i in timings.indices) {
            val t = timings[i]
            rawBytes[i * 2] = (t and 0xFF).toByte()
            rawBytes[i * 2 + 1] = ((t shr 8) and 0xFF).toByte()
        }

        val maxChunk = ((currentMtu - 9) and -2).coerceIn(12, 240)
        var offset = 0
        var seq = 0
        while (offset < rawBytes.size) {
            val chunkLen = minOf(maxChunk, rawBytes.size - offset)
            val packet = ByteArray(6 + chunkLen)
            packet[0] = 0xFE.toByte()
            packet[1] = 0x03.toByte() // 0x03: Raw IR chunk packet
            packet[2] = ((seq shr 8) and 0xFF).toByte()
            packet[3] = (seq and 0xFF).toByte()
            packet[4] = ((chunkLen shr 8) and 0xFF).toByte()
            packet[5] = (chunkLen and 0xFF).toByte()
            System.arraycopy(rawBytes, offset, packet, 6, chunkLen)

            var written = false
            for (attempt in 0 until 5) {
                if (writeData(char, packet, withoutResponse = true)) {
                    written = true
                    break
                }
                delay(15L * (attempt + 1))
            }
            if (!written) {
                return@withContext Result.failure(IOException("Failed to write raw IR chunk $seq at offset $offset"))
            }
            offset += chunkLen
            seq++
            delay(12)
        }

        // 3. Send IR_RAW_FINISH
        val finishJson = JSONObject().apply {
            put("cmd", "IR_RAW_FINISH")
            put("count", timings.size)
        }
        _fileResponseFlow.resetReplayCache()
        if (!writeData(char, finishJson.toString().toByteArray(Charsets.UTF_8))) {
            return@withContext Result.failure(IOException("Failed to send IR_RAW_FINISH"))
        }

        try {
            val finishResp = withTimeout(3000) {
                _fileResponseFlow.filter {
                    it.contains("IR_RAW_TRANSMIT") || it.contains("\"action\":\"IR_RAW_TRANSMIT\"") ||
                    it.contains("\"code\":200") || it.contains("\"code\":500") || it.contains("\"code\":400") ||
                    it.contains("\"status\":\"OK\"") || it.contains("\"status\":\"ERROR\"")
                }.first()
            }
            if (finishResp.contains("\"code\":200") || finishResp.contains("\"status\":\"OK\"")) {
                Result.success(true)
            } else {
                Result.failure(IOException("Watch reported error transmitting raw IR: $finishResp"))
            }
        } catch (e: Exception) {
            Result.failure(IOException("Timeout waiting for raw IR transmission completion: ${e.message}"))
        }
    }
}
