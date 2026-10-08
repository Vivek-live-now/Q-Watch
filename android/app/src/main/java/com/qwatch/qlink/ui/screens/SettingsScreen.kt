package com.qwatch.qlink.ui.screens

import android.content.Context
import android.widget.Toast
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.model.ConnectionState
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.protocol.QLinkConstants
import com.qwatch.qlink.protocol.ble.DiscoveredBleDevice
import com.qwatch.qlink.ui.components.TacticalCard
import com.qwatch.qlink.ui.theme.*
import kotlinx.coroutines.launch

@Composable
fun SettingsScreen() {
    val client = QLinkClient.instance
    val connectionState by client.connectionState.collectAsState()
    val autoConnectBle by client.autoConnectBleFlow.collectAsState()
    val scanner = client.bleScanner
    val isScanning by client.isScanningBle.collectAsState()
    val scanResults by client.bleScanResults.collectAsState()

    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val prefs = remember { context.getSharedPreferences("qlink_prefs", Context.MODE_PRIVATE) }

    var wifiHost by remember { mutableStateOf(prefs.getString("wifi_host", QLinkConstants.DEFAULT_HOTSPOT_IP) ?: QLinkConstants.DEFAULT_HOTSPOT_IP) }
    var bleMac by remember { mutableStateOf(client.getSavedBleMac()) }
    val scrollState = rememberScrollState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(TacticalBackground)
            .padding(16.dp)
            .verticalScroll(scrollState),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // BLE Transport & Auto-Connect Card
        TacticalCard(title = "BLUETOOTH LE AUTO-CONNECT & PAIRING", accentColor = TacticalAmber) {
            // Bluetooth Radio & Auto-Connect Controls
            val isBtOn = client.isBluetoothOn()
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Icon(
                        imageVector = Icons.Default.Bluetooth,
                        contentDescription = null,
                        tint = if (isBtOn) TacticalCyan else TextMuted
                    )
                    Column {
                        Text(
                            text = "RADIO: " + if (isBtOn) "ONLINE (ENABLED)" else "OFFLINE (DISABLED)",
                            color = if (isBtOn) TacticalGreen else TacticalRed,
                            fontSize = 11.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                        Text(
                            text = "AUTO-CONNECT WHEN BLUETOOTH ON",
                            color = TextMuted,
                            fontSize = 9.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }

                Switch(
                    checked = autoConnectBle,
                    onCheckedChange = { client.setBleAutoConnectEnabled(it) },
                    colors = SwitchDefaults.colors(
                        checkedThumbColor = OledBlack,
                        checkedTrackColor = TacticalCyan,
                        uncheckedThumbColor = TextMuted,
                        uncheckedTrackColor = TacticalSurfaceVariant
                    )
                )
            }

            Spacer(modifier = Modifier.height(10.dp))

            // Saved Target MAC input
            OutlinedTextField(
                value = bleMac,
                onValueChange = {
                    bleMac = it
                    client.setSavedBleMac(it)
                },
                label = { Text("Paired / Target Q-Watch MAC", color = TextMuted) },
                singleLine = true,
                modifier = Modifier.fillMaxWidth()
            )

            Spacer(modifier = Modifier.height(10.dp))

            // Action buttons: Connect / Disconnect / Scan
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
                Button(
                    onClick = {
                        scope.launch {
                            val res = client.connectBle(bleMac)
                            Toast.makeText(context, if (res.isSuccess) "BLE Connected!" else "BLE Connection Failed", Toast.LENGTH_SHORT).show()
                        }
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalAmber),
                    shape = RoundedCornerShape(6.dp),
                    modifier = Modifier.weight(1f)
                ) {
                    Text("CONNECT BLE", color = OledBlack, fontSize = 11.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }

                if (connectionState == ConnectionState.CONNECTED_BLE) {
                    Button(
                        onClick = { scope.launch { client.disconnect() } },
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalRed),
                        shape = RoundedCornerShape(6.dp)
                    ) {
                        Text("DISCONNECT", color = TextPrimary, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                    }
                }
            }

            Spacer(modifier = Modifier.height(12.dp))
            Divider(color = TacticalBorder)
            Spacer(modifier = Modifier.height(8.dp))

            // Scanner Section
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "DISCOVERED DEVICES (${scanResults.size})",
                    color = TacticalCyan,
                    fontSize = 11.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold
                )

                Button(
                    onClick = {
                        if (isScanning) scanner?.stopScan() else scanner?.startScan()
                    },
                    colors = ButtonDefaults.buttonColors(
                        containerColor = if (isScanning) TacticalRed else TacticalCyanDim
                    ),
                    shape = RoundedCornerShape(4.dp),
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 4.dp),
                    modifier = Modifier.height(28.dp)
                ) {
                    if (isScanning) {
                        CircularProgressIndicator(color = TextPrimary, modifier = Modifier.size(12.dp), strokeWidth = 2.dp)
                        Spacer(modifier = Modifier.width(4.dp))
                        Text("STOP", color = TextPrimary, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    } else {
                        Icon(Icons.Default.Radar, contentDescription = null, tint = TacticalCyan, modifier = Modifier.size(12.dp))
                        Spacer(modifier = Modifier.width(4.dp))
                        Text("SCAN BLE", color = TacticalCyan, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    }
                }
            }

            Spacer(modifier = Modifier.height(6.dp))

            if (scanResults.isEmpty()) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 12.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = if (isScanning) "Searching for Q-Watch devices..." else "Tap 'SCAN BLE' to search for Q-Watch",
                        color = TextMuted,
                        fontSize = 10.sp,
                        fontFamily = FontFamily.Monospace
                    )
                }
            } else {
                Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    for (dev in scanResults) {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(4.dp))
                                .background(TacticalSurfaceVariant)
                                .padding(8.dp),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Column(modifier = Modifier.weight(1f)) {
                                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                    Text(
                                        text = dev.name,
                                        color = if (dev.name.contains("Q-Watch", ignoreCase = true)) TacticalCyan else TextPrimary,
                                        fontSize = 11.sp,
                                        fontFamily = FontFamily.Monospace,
                                        fontWeight = FontWeight.Bold
                                    )
                                    if (dev.isBonded) {
                                        Box(
                                            modifier = Modifier
                                                .clip(RoundedCornerShape(2.dp))
                                                .background(TacticalGreenDim)
                                                .padding(horizontal = 4.dp, vertical = 1.dp)
                                        ) {
                                            Text("PAIRED", color = TacticalGreen, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                                        }
                                    }
                                }
                                Text(
                                    text = "${dev.address} • RSSI: ${dev.rssi} dBm",
                                    color = TextMuted,
                                    fontSize = 9.sp,
                                    fontFamily = FontFamily.Monospace
                                )
                            }

                            Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                                if (!dev.isBonded) {
                                    Button(
                                        onClick = {
                                            val ok = scanner?.createBond(dev.address) ?: false
                                            Toast.makeText(context, if (ok) "Pairing request sent..." else "Pairing failed", Toast.LENGTH_SHORT).show()
                                        },
                                        colors = ButtonDefaults.buttonColors(containerColor = TacticalAmberDim),
                                        contentPadding = PaddingValues(horizontal = 6.dp, vertical = 2.dp),
                                        shape = RoundedCornerShape(4.dp),
                                        modifier = Modifier.height(26.dp)
                                    ) {
                                        Text("PAIR", color = TacticalAmber, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                                    }
                                }

                                Button(
                                    onClick = {
                                        bleMac = dev.address
                                        client.setSavedBleMac(dev.address)
                                        scope.launch {
                                            val res = client.connectBle(dev.address)
                                            Toast.makeText(context, if (res.isSuccess) "Connected to ${dev.name}" else "Connection failed", Toast.LENGTH_SHORT).show()
                                        }
                                    },
                                    colors = ButtonDefaults.buttonColors(containerColor = TacticalCyanDim),
                                    contentPadding = PaddingValues(horizontal = 6.dp, vertical = 2.dp),
                                    shape = RoundedCornerShape(4.dp),
                                    modifier = Modifier.height(26.dp)
                                ) {
                                    Text("CONNECT", color = TacticalCyan, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                                }
                            }
                        }
                    }
                }
            }
        }

        // Wi-Fi Connection Card
        TacticalCard(title = "WI-FI TRANSPORT (HIGH SPEED)", accentColor = TacticalCyan) {
            OutlinedTextField(
                value = wifiHost,
                onValueChange = {
                    wifiHost = it
                    prefs.edit().putString("wifi_host", it).apply()
                },
                label = { Text("Q-Watch IP / Hostname", color = TextMuted) },
                singleLine = true,
                modifier = Modifier.fillMaxWidth()
            )

            Spacer(modifier = Modifier.height(10.dp))

            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(
                    onClick = {
                        scope.launch {
                            val res = client.connectWifi(wifiHost)
                            Toast.makeText(context, if (res.isSuccess) "Connected via Wi-Fi!" else "Connection failed", Toast.LENGTH_SHORT).show()
                        }
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalCyan),
                    shape = RoundedCornerShape(6.dp),
                    modifier = Modifier.weight(1f)
                ) {
                    Text("CONNECT WI-FI", color = OledBlack, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                }

                if (connectionState == ConnectionState.CONNECTED_WIFI) {
                    Button(
                        onClick = { scope.launch { client.disconnect() } },
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalRed),
                        shape = RoundedCornerShape(6.dp)
                    ) {
                        Text("DISCONNECT", color = TextPrimary, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                    }
                }
            }
        }

        // Protocol Info Card
        TacticalCard(title = "PROTOCOL CONTRACT", accentColor = TacticalGreen) {
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text(text = "PROTOCOL: Q-Link v${QLinkConstants.PROTOCOL_VERSION}", color = TextPrimary, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                Text(text = "FRAME BUFFER: 128x64 1-bit (1024 Bytes)", color = TextSecondary, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                Text(text = "GATT SERVICE: ${QLinkConstants.SERVICE_UUID}", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                Text(text = "AUTO-CONNECT: Background BLE + Reconnect Watchdog", color = TacticalGreen, fontSize = 10.sp, fontFamily = FontFamily.Monospace)
            }
        }
    }
}
