package com.qwatch.qlink

import com.qwatch.qlink.protocol.QLinkConstants
import com.qwatch.qlink.protocol.parser.QLinkPacketParser
import com.qwatch.qlink.ui.components.exportFrameToBitmap
import org.junit.Assert.*
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder

class QLinkProtocolTest {

    @Test
    fun testCompactTelemetryBinaryParsing() {
        val buffer = ByteBuffer.allocate(32).order(ByteOrder.LITTLE_ENDIAN)
        buffer.putShort(0x514C.toShort()) // Magic "QL"
        buffer.put(12.toByte())          // Seq
        buffer.put(88.toByte())          // Battery 88%
        buffer.putShort(4020.toShort())  // Battery 4020mV
        buffer.putShort(255.toShort())   // Temp 25.5 C
        buffer.putShort(10132.toShort()) // Press 1013.2 hPa
        buffer.putShort(540.toShort())   // Hum 54.0%
        buffer.putShort(92.toShort())    // Alt 92m
        buffer.putShort(45.toShort())    // Pitch 4.5 deg
        buffer.putShort((-12).toShort()) // Roll -1.2 deg
        buffer.putShort(1800.toShort())  // Heading 180.0 deg
        buffer.put(74.toByte())          // HR 74 BPM
        buffer.put(99.toByte())          // SpO2 99%
        buffer.putInt(6450)              // Steps 6450
        buffer.putShort(0x02.toShort())  // Status flags (WiFi on)
        buffer.putInt(3600)              // Uptime 3600s

        val bytes = buffer.array()
        val snapshot = QLinkPacketParser.parseCompactTelemetry(bytes)

        assertNotNull("Compact telemetry snapshot should not be null", snapshot)
        snapshot?.let { s ->
            assertEquals(88, s.batteryPct)
            assertEquals(4020, s.batteryMv)
            assertEquals(25.5f, s.bme.temperatureC, 0.01f)
            assertEquals(1013.2f, s.bme.pressureHpa, 0.01f)
            assertEquals(54.0f, s.bme.humidityPct, 0.01f)
            assertEquals(92.0f, s.bme.altitudeM, 0.01f)
            assertEquals(4.5f, s.imu.pitchDeg, 0.01f)
            assertEquals(-1.2f, s.imu.rollDeg, 0.01f)
            assertEquals(180.0f, s.compass.headingDeg, 0.01f)
            assertEquals(74, s.health.heartRateBpm)
            assertEquals(99, s.health.spo2Pct)
            assertTrue(s.health.fingerDetected)
            assertEquals(6450L, s.pedometer.steps)
            assertEquals(3600L, s.uptimeSec)
        }
    }

    @Test
    fun testTelemetryJsonParsing() {
        val json = """
            {
              "bme280": {
                "temperature_c": 24.6,
                "pressure_hpa": 1014.2,
                "humidity_pct": 52.8,
                "altitude_m": 84.0
              },
              "imu": {
                "pitch_deg": 3.5,
                "roll_deg": -2.1,
                "heading_deg": 90.0
              },
              "health": {
                "heart_rate_bpm": 68,
                "spo2_pct": 98,
                "finger_detected": true
              },
              "pedometer": {
                "steps": 4120,
                "distance_km": 3.09,
                "calories_kcal": 164
              },
              "battery": {
                "percent": 82,
                "voltage_v": 3.96
              },
              "uptime_sec": 4200
            }
        """.trimIndent()

        val snapshot = QLinkPacketParser.parseTelemetryJson(json)
        assertEquals(82, snapshot.batteryPct)
        assertEquals(3960, snapshot.batteryMv)
        assertEquals(24.6f, snapshot.bme.temperatureC, 0.01f)
        assertEquals(1014.2f, snapshot.bme.pressureHpa, 0.01f)
        assertEquals(3.5f, snapshot.imu.pitchDeg, 0.01f)
        assertEquals(68, snapshot.health.heartRateBpm)
        assertEquals(4120L, snapshot.pedometer.steps)
    }

    @Test
    fun testDeviceInfoJsonParsing() {
        val json = """
            {
              "device": "Q-Watch",
              "model": "ESP32-S3-SuperMini",
              "firmware_version": "1.4.0",
              "protocol_version": "1.0.0",
              "mac": "CC:7B:5C:80:12:34",
              "ip": "192.168.1.150",
              "battery": { "percent": 90, "voltage_mv": 4100 },
              "memory": { "free_heap": 220000, "fs_total_bytes": 1441792, "fs_used_bytes": 450000 },
              "uptime_sec": 1200
            }
        """.trimIndent()

        val info = QLinkPacketParser.parseDeviceInfoJson(json)
        assertEquals("Q-Watch", info.deviceName)
        assertEquals("1.4.0", info.firmwareVersion)
        assertEquals(QLinkConstants.PROTOCOL_VERSION, info.protocolVersion)
        assertEquals("CC:7B:5C:80:12:34", info.macAddress)
        assertEquals(90, info.batteryPercent)
        assertEquals(220000L, info.freeHeapBytes)
    }

    @Test
    fun testFileListJsonParsing() {
        val json = """
            {
              "path": "/apps",
              "files": [
                { "name": "tilt_ball.qapp", "size": 3480, "is_dir": false },
                { "name": "games", "size": 0, "is_dir": true }
              ]
            }
        """.trimIndent()

        val files = QLinkPacketParser.parseFileListJson(json)
        assertEquals(2, files.size)
        assertEquals("tilt_ball.qapp", files[0].name)
        assertEquals("/apps/tilt_ball.qapp", files[0].path)
        assertEquals(3480L, files[0].sizeBytes)
        assertFalse(files[0].isDirectory)

        assertEquals("games", files[1].name)
        assertEquals("/apps/games", files[1].path)
        assertTrue(files[1].isDirectory)
    }
}
