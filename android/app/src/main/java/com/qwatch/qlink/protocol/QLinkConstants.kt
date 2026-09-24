package com.qwatch.qlink.protocol

import java.util.UUID

object QLinkConstants {
    const val PROTOCOL_VERSION = "1.0.0"
    const val MAGIC: Short = 0x514C // "QL"
    const val COMPACT_TELEMETRY_SIZE = 32
    const val OLED_WIDTH = 128
    const val OLED_HEIGHT = 64
    const val OLED_FRAME_BYTES = 1024 // 128 * 64 / 8

    // BLE GATT UUIDs (128-bit)
    val SERVICE_UUID: UUID = UUID.fromString("0000FE50-0000-1000-8000-00805F9B34FB")
    val CHAR_COMMAND_UUID: UUID = UUID.fromString("0000FE51-0000-1000-8000-00805F9B34FB")
    val CHAR_TELEMETRY_UUID: UUID = UUID.fromString("0000FE52-0000-1000-8000-00805F9B34FB")
    val CHAR_DISPLAY_UUID: UUID = UUID.fromString("0000FE53-0000-1000-8000-00805F9B34FB")
    val CHAR_FILE_UUID: UUID = UUID.fromString("0000FE54-0000-1000-8000-00805F9B34FB")

    // Client Characteristic Configuration Descriptor (CCCD) for Enable Notification
    val CLIENT_CONFIG_DESCRIPTOR_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

    // Default Network Endpoints
    const val DEFAULT_HOTSPOT_IP = "192.168.4.1"
    const val DEFAULT_MDNS_HOST = "q-watch.local"
    const val HTTP_PORT = 80

    // REST API Routes (v1)
    const val PATH_INFO = "/api/v1/info"
    const val PATH_TELEMETRY = "/api/v1/telemetry"
    const val PATH_DISPLAY_FRAME = "/api/v1/display/frame"
    const val PATH_DISPLAY_CONTROL = "/api/v1/display/control"
    const val PATH_BUTTON = "/api/v1/button"
    const val PATH_NOTIFICATION = "/api/v1/notification"
    const val PATH_SYNC_TIME = "/api/v1/sync/time"
    const val PATH_SYNC_WEATHER = "/api/v1/sync/weather"
    const val PATH_FS_LIST = "/api/v1/fs/list"
    const val PATH_FS_DOWNLOAD = "/api/v1/fs/download"
    const val PATH_FS_UPLOAD = "/api/v1/fs/upload"
    const val PATH_FS_DELETE = "/api/v1/fs/delete"
    const val PATH_APP_INSTALL = "/api/v1/app/install"
}
