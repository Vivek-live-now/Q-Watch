package com.qwatch.qlink.protocol.transport

import com.qwatch.qlink.model.*
import com.qwatch.qlink.protocol.QLinkConstants
import com.qwatch.qlink.protocol.parser.QLinkPacketParser
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import okhttp3.*
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import java.io.IOException
import java.util.concurrent.TimeUnit

class QLinkWifiTransport(
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO + SupervisorJob())
) : QLinkTransport {

    private val client = OkHttpClient.Builder()
        .connectTimeout(4, TimeUnit.SECONDS)
        .readTimeout(6, TimeUnit.SECONDS)
        .writeTimeout(10, TimeUnit.SECONDS)
        .build()

    private var hostAddress: String = QLinkConstants.DEFAULT_HOTSPOT_IP
    fun getHost(): String = hostAddress

    private val _connectionState = MutableStateFlow(ConnectionState.DISCONNECTED)
    override val connectionState: Flow<ConnectionState> = _connectionState.asStateFlow()

    private val _lastError = MutableStateFlow<String?>(null)
    val lastError: StateFlow<String?> = _lastError.asStateFlow()

    private val _telemetryFlow = MutableSharedFlow<TelemetrySnapshot>(extraBufferCapacity = 8)
    override val telemetryFlow: Flow<TelemetrySnapshot> = _telemetryFlow.asSharedFlow()

    private val _displayFrameFlow = MutableSharedFlow<DisplayFrame>(extraBufferCapacity = 4)
    override val displayFrameFlow: Flow<DisplayFrame> = _displayFrameFlow.asSharedFlow()

    private var streamingJob: Job? = null
    private var telemetryPollJob: Job? = null
    private var frameSequence: Long = 0L

    private val jsonMediaType = "application/json; charset=utf-8".toMediaType()

    private fun baseUrl(): String {
        return "http://$hostAddress:${QLinkConstants.HTTP_PORT}"
    }

    override suspend fun connect(target: String): Result<Boolean> = withContext(Dispatchers.IO) {
        hostAddress = if (target.isNotBlank()) target.trim() else QLinkConstants.DEFAULT_HOTSPOT_IP
        _connectionState.value = ConnectionState.CONNECTING
        _lastError.value = null

        try {
            val req = Request.Builder()
                .url("${baseUrl()}${QLinkConstants.PATH_INFO}")
                .build()

            client.newCall(req).execute().use { response ->
                if (response.isSuccessful) {
                    _connectionState.value = ConnectionState.CONNECTED_WIFI
                    _lastError.value = null
                    startTelemetryPolling()
                    Result.success(true)
                } else {
                    val msg = "HTTP ${response.code} from Q-Watch ($hostAddress)"
                    _lastError.value = msg
                    _connectionState.value = ConnectionState.ERROR
                    Result.failure(IOException(msg))
                }
            }
        } catch (e: Exception) {
            val msg = "Cannot reach $hostAddress (${e.message ?: "Unreachable"})"
            _lastError.value = msg
            _connectionState.value = ConnectionState.ERROR
            Result.failure(e)
        }
    }

    override suspend fun disconnect() = withContext(Dispatchers.IO) {
        streamingJob?.cancel()
        streamingJob = null
        telemetryPollJob?.cancel()
        telemetryPollJob = null
        _connectionState.value = ConnectionState.DISCONNECTED
        _lastError.value = null
    }

    override fun isConnected(): Boolean {
        return _connectionState.value == ConnectionState.CONNECTED_WIFI
    }

    private fun startTelemetryPolling() {
        telemetryPollJob?.cancel()
        telemetryPollJob = scope.launch {
            while (isActive && isConnected()) {
                try {
                    val telResult = getTelemetry()
                    telResult.getOrNull()?.let { _telemetryFlow.emit(it) }
                } catch (_: Exception) {}
                delay(1000) // 1 Hz polling in background
            }
        }
    }

    override suspend fun getDeviceInfo(): Result<DeviceInfo> = withContext(Dispatchers.IO) {
        try {
            val req = Request.Builder()
                .url("${baseUrl()}${QLinkConstants.PATH_INFO}")
                .build()
            client.newCall(req).execute().use { res ->
                val body = res.body?.string() ?: ""
                Result.success(QLinkPacketParser.parseDeviceInfoJson(body))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    override suspend fun getTelemetry(): Result<TelemetrySnapshot> = withContext(Dispatchers.IO) {
        try {
            val req = Request.Builder()
                .url("${baseUrl()}${QLinkConstants.PATH_TELEMETRY}")
                .build()
            client.newCall(req).execute().use { res ->
                val body = res.body?.string() ?: ""
                Result.success(QLinkPacketParser.parseTelemetryJson(body))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    override suspend fun injectButton(type: ButtonType, event: ButtonEventType): Result<Boolean> = withContext(Dispatchers.IO) {
        try {
            val json = JSONObject().apply {
                put("button", type.name)
                put("event", event.name)
            }
            val req = Request.Builder()
                .url("${baseUrl()}${QLinkConstants.PATH_BUTTON}")
                .post(json.toString().toRequestBody(jsonMediaType))
                .build()
            client.newCall(req).execute().use { res ->
                Result.success(res.isSuccessful)
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    override suspend fun sendNotification(payload: NotificationPayload): Result<Boolean> = withContext(Dispatchers.IO) {
        try {
            val json = JSONObject().apply {
                put("app_name", payload.appName)
                put("package", payload.packageName)
                put("title", payload.title)
                put("body", payload.body)
                put("category", payload.category)
                put("alert_style", payload.alertStyle)
                put("led_color", payload.ledColorHex)
            }
            val req = Request.Builder()
                .url("${baseUrl()}${QLinkConstants.PATH_NOTIFICATION}")
                .post(json.toString().toRequestBody(jsonMediaType))
                .build()
            client.newCall(req).execute().use { res ->
                Result.success(res.isSuccessful)
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    override suspend fun syncTime(epochSec: Long, tzOffsetSec: Int): Result<Boolean> = withContext(Dispatchers.IO) {
        try {
            val json = JSONObject().apply {
                put("epoch_sec", epochSec)
                put("timezone_offset_sec", tzOffsetSec)
            }
            val req = Request.Builder()
                .url("${baseUrl()}${QLinkConstants.PATH_SYNC_TIME}")
                .post(json.toString().toRequestBody(jsonMediaType))
                .build()
            client.newCall(req).execute().use { res ->
                Result.success(res.isSuccessful)
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
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
        try {
            val json = JSONObject().apply {
                put("city", city)
                put("temp_c", tempC)
                put("humidity_pct", humidity)
                put("condition_code", code)
                put("condition_str", desc)
                put("high_c", hi)
                put("low_c", lo)
            }
            val req = Request.Builder()
                .url("${baseUrl()}${QLinkConstants.PATH_SYNC_WEATHER}")
                .post(json.toString().toRequestBody(jsonMediaType))
                .build()
            client.newCall(req).execute().use { res ->
                Result.success(res.isSuccessful)
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    override suspend fun fetchDisplayFrame(): Result<DisplayFrame> = withContext(Dispatchers.IO) {
        try {
            val req = Request.Builder()
                .url("${baseUrl()}${QLinkConstants.PATH_DISPLAY_FRAME}")
                .build()
            client.newCall(req).execute().use { res ->
                val bytes = res.body?.bytes()
                if (bytes != null && bytes.size >= QLinkConstants.OLED_FRAME_BYTES) {
                    val frame = DisplayFrame(
                        buffer = bytes.copyOf(QLinkConstants.OLED_FRAME_BYTES),
                        frameNumber = ++frameSequence,
                        timestampMs = System.currentTimeMillis()
                    )
                    Result.success(frame)
                } else {
                    Result.failure(IOException("Incomplete frame buffer received"))
                }
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    override suspend fun setDisplayStreaming(enabled: Boolean, fps: Int): Result<Boolean> = withContext(Dispatchers.IO) {
        try {
            val json = JSONObject().apply {
                put("streaming", enabled)
                put("fps", fps)
            }
            val req = Request.Builder()
                .url("${baseUrl()}${QLinkConstants.PATH_DISPLAY_CONTROL}")
                .post(json.toString().toRequestBody(jsonMediaType))
                .build()
            client.newCall(req).execute().use { res ->
                if (enabled) {
                    startDisplayStreaming(fps)
                } else {
                    streamingJob?.cancel()
                    streamingJob = null
                }
                Result.success(res.isSuccessful)
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    private fun startDisplayStreaming(fps: Int) {
        streamingJob?.cancel()
        val delayIntervalMs = (1000 / fps.coerceIn(5, 40)).toLong()
        streamingJob = scope.launch {
            while (isActive && isConnected()) {
                val frameRes = fetchDisplayFrame()
                frameRes.getOrNull()?.let { _displayFrameFlow.emit(it) }
                delay(delayIntervalMs)
            }
        }
    }

    override suspend fun listFiles(path: String): Result<List<WatchFile>> = withContext(Dispatchers.IO) {
        try {
            val req = Request.Builder()
                .url("${baseUrl()}${QLinkConstants.PATH_FS_LIST}?path=$path")
                .build()
            client.newCall(req).execute().use { res ->
                val body = res.body?.string() ?: ""
                Result.success(QLinkPacketParser.parseFileListJson(body))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    override suspend fun downloadFile(path: String): Result<ByteArray> = withContext(Dispatchers.IO) {
        try {
            val req = Request.Builder()
                .url("${baseUrl()}${QLinkConstants.PATH_FS_DOWNLOAD}?path=$path")
                .build()
            client.newCall(req).execute().use { res ->
                val bytes = res.body?.bytes() ?: ByteArray(0)
                Result.success(bytes)
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    override suspend fun uploadFile(path: String, data: ByteArray): Result<Boolean> = withContext(Dispatchers.IO) {
        try {
            val formBody = MultipartBody.Builder()
                .setType(MultipartBody.FORM)
                .addFormDataPart(
                    "file",
                    path.substringAfterLast('/'),
                    data.toRequestBody("application/octet-stream".toMediaType())
                )
                .build()
            val req = Request.Builder()
                .url("${baseUrl()}${QLinkConstants.PATH_FS_UPLOAD}")
                .post(formBody)
                .build()
            client.newCall(req).execute().use { res ->
                Result.success(res.isSuccessful)
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    override suspend fun deleteFile(path: String): Result<Boolean> = withContext(Dispatchers.IO) {
        try {
            val req = Request.Builder()
                .url("${baseUrl()}${QLinkConstants.PATH_FS_DELETE}?path=$path")
                .delete()
                .build()
            client.newCall(req).execute().use { res ->
                Result.success(res.isSuccessful)
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    override suspend fun installQApp(filename: String, data: ByteArray): Result<Boolean> = withContext(Dispatchers.IO) {
        // Upload to /apps/<filename>
        uploadFile("/apps/$filename", data)
    }
}
