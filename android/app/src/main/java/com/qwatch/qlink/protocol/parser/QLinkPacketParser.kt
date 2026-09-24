package com.qwatch.qlink.protocol.parser

import com.qwatch.qlink.model.*
import com.qwatch.qlink.protocol.QLinkConstants
import org.json.JSONArray
import org.json.JSONObject
import java.nio.ByteBuffer
import java.nio.ByteOrder

object QLinkPacketParser {

    /**
     * Parses the 32-byte fixed compact binary telemetry packet from BLE characteristic notifications.
     */
    fun parseCompactTelemetry(bytes: ByteArray): TelemetrySnapshot? {
        if (bytes.size < QLinkConstants.COMPACT_TELEMETRY_SIZE) return null

        val buffer = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN)
        val magic = buffer.short
        if (magic != QLinkConstants.MAGIC) return null

        val seq = buffer.get().toInt() and 0xFF
        val batteryPct = buffer.get().toInt() and 0xFF
        val batteryMv = buffer.short.toInt() and 0xFFFF
        val tempX10 = buffer.short.toInt()
        val pressX10 = buffer.short.toInt() and 0xFFFF
        val humX10 = buffer.short.toInt() and 0xFFFF
        val altitudeM = buffer.short.toInt()
        val pitchX10 = buffer.short.toInt()
        val rollX10 = buffer.short.toInt()
        val headingX10 = buffer.short.toInt() and 0xFFFF
        val hrBpm = buffer.get().toInt() and 0xFF
        val spo2 = buffer.get().toInt() and 0xFF
        val steps = buffer.int.toLong() and 0xFFFFFFFFL
        val flags = buffer.short.toInt() and 0xFFFF
        val uptime = buffer.int.toLong() and 0xFFFFFFFFL

        return TelemetrySnapshot(
            timestampMs = System.currentTimeMillis(),
            bme = BmeData(
                temperatureC = tempX10 / 10.0f,
                pressureHpa = pressX10 / 10.0f,
                humidityPct = humX10 / 10.0f,
                altitudeM = altitudeM.toFloat()
            ),
            imu = ImuData(
                pitchDeg = pitchX10 / 10.0f,
                rollDeg = rollX10 / 10.0f
            ),
            compass = CompassData(
                headingDeg = headingX10 / 10.0f
            ),
            health = HealthData(
                heartRateBpm = hrBpm,
                spo2Pct = spo2,
                fingerDetected = (hrBpm > 0)
            ),
            pedometer = PedometerData(
                steps = steps,
                distanceKm = steps * 0.00075f,
                caloriesKcal = (steps * 0.04f).toLong()
            ),
            batteryPct = batteryPct,
            batteryMv = batteryMv,
            uptimeSec = uptime
        )
    }

    /**
     * Parses the /api/v1/telemetry JSON payload from Wi-Fi REST query.
     */
    fun parseTelemetryJson(jsonString: String): TelemetrySnapshot {
        val root = JSONObject(jsonString)
        val bmeObj = root.optJSONObject("bme280")
        val imuObj = root.optJSONObject("imu")
        val compObj = root.optJSONObject("compass")
        val healthObj = root.optJSONObject("health")
        val pedObj = root.optJSONObject("pedometer")
        val batObj = root.optJSONObject("battery")

        return TelemetrySnapshot(
            timestampMs = System.currentTimeMillis(),
            bme = BmeData(
                temperatureC = bmeObj?.optDouble("temperature_c", 0.0)?.toFloat() ?: 0.0f,
                pressureHpa = bmeObj?.optDouble("pressure_hpa", 0.0)?.toFloat() ?: 0.0f,
                humidityPct = bmeObj?.optDouble("humidity_pct", 0.0)?.toFloat() ?: 0.0f,
                altitudeM = bmeObj?.optDouble("altitude_m", 0.0)?.toFloat() ?: 0.0f
            ),
            imu = ImuData(
                pitchDeg = imuObj?.optDouble("pitch_deg", 0.0)?.toFloat() ?: 0.0f,
                rollDeg = imuObj?.optDouble("roll_deg", 0.0)?.toFloat() ?: 0.0f,
                accelX = imuObj?.optDouble("accel_x", 0.0)?.toFloat() ?: 0.0f,
                accelY = imuObj?.optDouble("accel_y", 0.0)?.toFloat() ?: 0.0f,
                accelZ = imuObj?.optDouble("accel_z", 1.0)?.toFloat() ?: 1.0f
            ),
            compass = CompassData(
                headingDeg = compObj?.optDouble("heading_deg", 0.0)?.toFloat() ?: 0.0f,
                magneticFieldUt = compObj?.optDouble("field_ut", 0.0)?.toFloat() ?: 0.0f
            ),
            health = HealthData(
                heartRateBpm = healthObj?.optInt("heart_rate_bpm", 0) ?: 0,
                spo2Pct = healthObj?.optInt("spo2_pct", 0) ?: 0,
                fingerDetected = healthObj?.optBoolean("finger_detected", false) ?: false
            ),
            pedometer = PedometerData(
                steps = pedObj?.optLong("steps", 0L) ?: 0L,
                distanceKm = pedObj?.optDouble("distance_km", 0.0)?.toFloat() ?: 0.0f,
                caloriesKcal = pedObj?.optLong("calories_kcal", 0L) ?: 0L
            ),
            batteryPct = batObj?.optInt("percent", 0) ?: 0,
            batteryMv = ((batObj?.optDouble("voltage_v", 0.0) ?: 0.0) * 1000).toInt(),
            uptimeSec = root.optLong("uptime_sec", 0L)
        )
    }

    /**
     * Parses the /api/v1/info JSON payload.
     */
    fun parseDeviceInfoJson(jsonString: String): DeviceInfo {
        val root = JSONObject(jsonString)
        val batObj = root.optJSONObject("battery")
        val memObj = root.optJSONObject("memory")

        return DeviceInfo(
            deviceName = root.optString("device", "Q-Watch"),
            model = root.optString("model", "ESP32-S3-SuperMini"),
            firmwareVersion = root.optString("firmware_version", "1.4.0"),
            protocolVersion = root.optString("protocol_version", "1.0.0"),
            compileDate = root.optString("compile_date", ""),
            macAddress = root.optString("mac", ""),
            ipAddress = root.optString("ip", ""),
            rssi = root.optInt("rssi", 0),
            batteryPercent = batObj?.optInt("percent", 0) ?: 0,
            batteryVoltageMv = batObj?.optInt("voltage_mv", 0) ?: 0,
            freeHeapBytes = memObj?.optLong("free_heap", 0L) ?: 0L,
            fsTotalBytes = memObj?.optLong("fs_total_bytes", 0L) ?: 0L,
            fsUsedBytes = memObj?.optLong("fs_used_bytes", 0L) ?: 0L,
            uptimeSec = root.optLong("uptime_sec", 0L)
        )
    }

    /**
     * Parses the /api/v1/fs/list JSON payload into a list of WatchFiles.
     */
    fun parseFileListJson(jsonString: String): List<WatchFile> {
        val list = mutableListOf<WatchFile>()
        try {
            val root = JSONObject(jsonString)
            val basePath = root.optString("path", "/")
            val filesArr = root.optJSONArray("files") ?: JSONArray()
            for (i in 0 until filesArr.length()) {
                val fObj = filesArr.getJSONObject(i)
                val name = fObj.optString("name")
                val size = fObj.optLong("size", 0L)
                val isDir = fObj.optBoolean("is_dir", false)
                val fullPath = if (basePath.endsWith("/")) "$basePath$name" else "$basePath/$name"
                list.add(WatchFile(name, fullPath, size, isDir))
            }
        } catch (_: Exception) {
            // Return empty or partial list on error
        }
        return list
    }
}
