package com.qwatch.qlink.ui.screens

import android.content.Context
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material.icons.filled.Wifi
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.model.ConnectionState
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.protocol.QLinkConstants
import com.qwatch.qlink.ui.components.*
import com.qwatch.qlink.ui.theme.*
import kotlinx.coroutines.launch

@Composable
fun DashboardScreen(
    onNavigateToMirror: () -> Unit,
    onNavigateToSensors: () -> Unit,
    onNavigateToMochi: () -> Unit = {},
    onNavigateToPower: () -> Unit = {},
    onNavigateToSigint: () -> Unit = {},
    onNavigateToApps: () -> Unit = {},
    onNavigateToAnimMarket: () -> Unit = {},
    onNavigateToFiles: () -> Unit = {},
    onNavigateToNotifications: () -> Unit = {},
    onNavigateToSettings: () -> Unit = {}
) {
    val client = QLinkClient.instance
    val connectionState by client.connectionState.collectAsState()
    val lastError by client.lastError.collectAsState()
    val telemetry by client.telemetry.collectAsState()
    val frame by client.displayFrame.collectAsState()
    val context = LocalContext.current
    val focusManager = LocalFocusManager.current
    val scope = rememberCoroutineScope()
    val prefs = remember { context.getSharedPreferences("qlink_prefs", Context.MODE_PRIVATE) }

    var wifiHost by remember {
        mutableStateOf(prefs.getString("wifi_host", QLinkConstants.DEFAULT_HOTSPOT_IP) ?: QLinkConstants.DEFAULT_HOTSPOT_IP)
    }
    var statusToast by remember { mutableStateOf<String?>(null) }
    var selectedPhosphor by remember { mutableStateOf(PhosphorAmber) }
    val isConnected = client.isConnected()
    val isConnecting = connectionState == ConnectionState.CONNECTING

    val scrollState = rememberScrollState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(TacticalBackground)
            .padding(16.dp)
            .verticalScroll(scrollState),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // 0. Stitch Cockpit Telemetry Banner Strip
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(4.dp))
                .background(TacticalSurfaceLow)
                .border(1.dp, TacticalBorder, RoundedCornerShape(4.dp))
                .padding(horizontal = 10.dp, vertical = 6.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Box(
                    modifier = Modifier
                        .size(8.dp)
                        .clip(androidx.compose.foundation.shape.CircleShape)
                        .background(if (isConnected) TacticalGreen else TacticalRed)
                )
                Text(
                    text = if (isConnected) "BLE 5.0 ACTIVE" else "DISCONNECTED",
                    color = if (isConnected) TacticalGreen else TacticalRed,
                    fontSize = 9.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold
                )
            }
            Text(
                text = "HOST: ${client.getTargetHost()}",
                color = TextSecondary,
                fontSize = 8.5.sp,
                fontFamily = FontFamily.Monospace
            )
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Text(
                    text = "BAT: ${telemetry.batteryPct}% (${telemetry.batteryMv}mV)",
                    color = TacticalAmber,
                    fontSize = 9.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold
                )
                Text(
                    text = "BALANCED",
                    color = TacticalCyan,
                    fontSize = 8.5.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold
                )
            }
        }

        // 1. Connection & Battery Status Header
        TacticalCard(title = "SYSTEM STATUS", accentColor = TacticalCyan) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column {
                    Text(
                        text = "LINK STATUS",
                        color = TextMuted,
                        fontSize = 9.sp,
                        fontFamily = FontFamily.Monospace
                    )
                    Text(
                        text = connectionState.name,
                        color = when (connectionState) {
                            ConnectionState.CONNECTED_WIFI, ConnectionState.CONNECTED_BLE -> TacticalGreen
                            ConnectionState.CONNECTING, ConnectionState.SCANNING -> TacticalAmber
                            else -> TacticalRed
                        },
                        fontSize = 14.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                    if (isConnected) {
                        Text(
                            text = "HOST: ${client.getTargetHost()}",
                            color = TextSecondary,
                            fontSize = 9.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }

                Column(horizontalAlignment = Alignment.End) {
                    Text(
                        text = "BATTERY",
                        color = TextMuted,
                        fontSize = 9.sp,
                        fontFamily = FontFamily.Monospace
                    )
                    Text(
                        text = "${telemetry.batteryPct}% (${telemetry.batteryMv}mV)",
                        color = if (telemetry.batteryPct > 20) TacticalCyan else TacticalRed,
                        fontSize = 14.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                    if (isConnected) {
                        Text(
                            text = "UPTIME: ${telemetry.uptimeSec}s",
                            color = TextSecondary,
                            fontSize = 9.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }
            }
        }

        // 2. Link Connection Assistant (Prominent when offline or error)
        if (!isConnected) {
            TacticalCard(
                title = "ESTABLISH LINK",
                accentColor = if (connectionState == ConnectionState.ERROR) TacticalRed else TacticalCyan
            ) {
                // Diagnostic banner
                if (connectionState == ConnectionState.ERROR) {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(6.dp))
                            .background(TacticalRedDim)
                            .border(1.dp, TacticalRed, RoundedCornerShape(6.dp))
                            .padding(10.dp)
                    ) {
                        Column {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Icon(
                                    imageVector = Icons.Default.Warning,
                                    contentDescription = null,
                                    tint = TacticalRed,
                                    modifier = Modifier.size(16.dp)
                                )
                                Spacer(modifier = Modifier.width(6.dp))
                                Text(
                                    text = "CANNOT REACH Q-WATCH",
                                    color = TacticalRed,
                                    fontFamily = FontFamily.Monospace,
                                    fontWeight = FontWeight.Bold,
                                    fontSize = 11.sp
                                )
                            }
                            lastError?.let { err ->
                                Spacer(modifier = Modifier.height(4.dp))
                                Text(
                                    text = err,
                                    color = TextPrimary,
                                    fontFamily = FontFamily.Monospace,
                                    fontSize = 10.sp
                                )
                            }
                        }
                    }
                    Spacer(modifier = Modifier.height(10.dp))
                } else if (isConnecting) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.Center
                    ) {
                        CircularProgressIndicator(
                            modifier = Modifier.size(14.dp),
                            color = TacticalAmber,
                            strokeWidth = 2.dp
                        )
                        Spacer(modifier = Modifier.width(8.dp))
                        Text(
                            text = "CONNECTING TO $wifiHost...",
                            color = TacticalAmber,
                            fontFamily = FontFamily.Monospace,
                            fontSize = 11.sp
                        )
                    }
                    Spacer(modifier = Modifier.height(10.dp))
                }

                // IP / Hostname input field
                OutlinedTextField(
                    value = wifiHost,
                    onValueChange = {
                        wifiHost = it
                        prefs.edit().putString("wifi_host", it).apply()
                    },
                    label = { Text("Target IP / Hostname", color = TextMuted, fontSize = 11.sp) },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedBorderColor = TacticalCyan,
                        unfocusedBorderColor = TacticalBorder,
                        focusedTextColor = TextPrimary,
                        unfocusedTextColor = TextPrimary,
                        focusedLabelColor = TacticalCyan,
                        unfocusedLabelColor = TextMuted
                    ),
                    keyboardOptions = KeyboardOptions(
                        keyboardType = KeyboardType.Uri,
                        imeAction = ImeAction.Done
                    ),
                    keyboardActions = KeyboardActions(onDone = {
                        focusManager.clearFocus()
                        scope.launch { client.connectWifi(wifiHost) }
                    })
                )

                Spacer(modifier = Modifier.height(8.dp))

                // Preset Chips Row
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Button(
                        onClick = {
                            wifiHost = QLinkConstants.DEFAULT_HOTSPOT_IP
                            prefs.edit().putString("wifi_host", wifiHost).apply()
                            scope.launch { client.connectWifi(wifiHost) }
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                        shape = RoundedCornerShape(6.dp),
                        modifier = Modifier.weight(1f)
                    ) {
                        Text(
                            text = "AP: 192.168.4.1",
                            color = TacticalCyan,
                            fontSize = 9.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                    }

                    Button(
                        onClick = {
                            wifiHost = QLinkConstants.DEFAULT_MDNS_HOST
                            prefs.edit().putString("wifi_host", wifiHost).apply()
                            scope.launch { client.connectWifi(wifiHost) }
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                        shape = RoundedCornerShape(6.dp),
                        modifier = Modifier.weight(1f)
                    ) {
                        Text(
                            text = "mDNS: q-watch.local",
                            color = TacticalCyan,
                            fontSize = 9.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                    }
                }

                Spacer(modifier = Modifier.height(8.dp))

                // Connect button
                Button(
                    onClick = {
                        focusManager.clearFocus()
                        scope.launch {
                            val res = client.connectWifi(wifiHost)
                            if (res.isFailure) {
                                statusToast = "Connection Failed"
                            }
                        }
                    },
                    enabled = !isConnecting,
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalCyan),
                    shape = RoundedCornerShape(6.dp),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text(
                        text = if (isConnecting) "LINKING..." else "CONNECT WI-FI LINK",
                        color = OledBlack,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold,
                        fontSize = 12.sp
                    )
                }

                Spacer(modifier = Modifier.height(10.dp))

                // Quick Setup Guide
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(6.dp))
                        .background(TacticalSurfaceVariant)
                        .padding(10.dp)
                ) {
                    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Icon(
                                imageVector = Icons.Default.Info,
                                contentDescription = null,
                                tint = TacticalCyan,
                                modifier = Modifier.size(14.dp)
                            )
                            Spacer(modifier = Modifier.width(6.dp))
                            Text(
                                text = "HOW TO CONNECT",
                                color = TacticalCyan,
                                fontFamily = FontFamily.Monospace,
                                fontWeight = FontWeight.Bold,
                                fontSize = 10.sp
                            )
                        }
                        Text(
                            text = "• DIRECT HOTSPOT: Turn on Wi-Fi on watch -> Phone joins 'Q-Watch-Setup' Wi-Fi -> Tap [AP: 192.168.4.1].",
                            color = TextSecondary,
                            fontFamily = FontFamily.Monospace,
                            fontSize = 9.sp
                        )
                        Text(
                            text = "• HOME WI-FI: Connect watch to home router -> Check IP on watch screen (Settings > Wi-Fi) -> Enter IP above -> Tap CONNECT.",
                            color = TextSecondary,
                            fontFamily = FontFamily.Monospace,
                            fontSize = 9.sp
                        )
                    }
                }
            }
        } else {
            // Online status bar with Disconnect option
            TacticalCard(title = "ACTIVE LINK", accentColor = TacticalGreen) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column {
                        Text(
                            text = "ONLINE via ${client.getTargetHost()}",
                            color = TacticalGreen,
                            fontSize = 11.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                        Text(
                            text = "PROTOCOL: Q-Link v${QLinkConstants.PROTOCOL_VERSION}",
                            color = TextMuted,
                            fontSize = 9.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }

                    Button(
                        onClick = { scope.launch { client.disconnect() } },
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalRed),
                        shape = RoundedCornerShape(6.dp)
                    ) {
                        Text(
                            text = "DISCONNECT",
                            color = TextPrimary,
                            fontSize = 10.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }
            }
        }

        // 3. Hero Section: Virtual OLED Display 128x64 & Hardware D-Pad Remote
        TacticalCard(title = "VIRTUAL OLED MIRROR // SSD1306", accentColor = TacticalAmber) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "FRAME #${frame.frameNumber} · 20 FPS · 128x64 I2C",
                    color = TextMuted,
                    fontSize = 8.sp,
                    fontFamily = FontFamily.Monospace
                )
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    listOf("AMBER" to PhosphorAmber, "CYAN" to PhosphorCyan, "GREEN" to PhosphorGreen).forEach { (label, col) ->
                        val isSelected = selectedPhosphor == col
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(3.dp))
                                .background(if (isSelected) col else TacticalSurfaceVariant)
                                .border(1.dp, if (isSelected) col else TacticalBorder, RoundedCornerShape(3.dp))
                                .clickable { selectedPhosphor = col }
                                .padding(horizontal = 6.dp, vertical = 2.dp)
                        ) {
                            Text(
                                text = label,
                                color = if (isSelected) OledBlack else TextMuted,
                                fontSize = 8.sp,
                                fontFamily = FontFamily.Monospace,
                                fontWeight = FontWeight.Bold
                            )
                        }
                    }
                }
            }

            Spacer(modifier = Modifier.height(6.dp))

            VirtualOledDisplay(
                frame = frame,
                isStreaming = isConnected,
                phosphorColor = selectedPhosphor,
                modifier = Modifier.fillMaxWidth()
            )

            Spacer(modifier = Modifier.height(8.dp))

            // Hardware D-Pad Directional Key Matrix
            TacticalDPad(
                onButtonAction = { btn, evt ->
                    scope.launch { client.injectButton(btn, evt) }
                },
                modifier = Modifier.fillMaxWidth()
            )

            Spacer(modifier = Modifier.height(6.dp))

            Button(
                onClick = onNavigateToMirror,
                colors = ButtonDefaults.buttonColors(containerColor = TacticalAmber),
                shape = RoundedCornerShape(4.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                Text(
                    text = "OPEN FULL VIRTUAL REMOTE DECK",
                    color = OledBlack,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold,
                    fontSize = 11.sp
                )
            }
        }

        // 4. Environmental & Biometrics Overview
        TacticalCard(title = "TELEMETRY SNAPSHOT", accentColor = TacticalGreen) {
            BmeReadoutGrid(bme = telemetry.bme)
            Spacer(modifier = Modifier.height(12.dp))
            HealthPulseGauge(health = telemetry.health)
            Spacer(modifier = Modifier.height(8.dp))
            Button(
                onClick = onNavigateToSensors,
                colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                shape = RoundedCornerShape(6.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                Text(
                    text = "VIEW SENSOR COCKPIT & HORIZON",
                    color = TacticalCyan,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold,
                    fontSize = 12.sp
                )
            }
        }

        // 5. Quick Action Dispatch
        TacticalCard(title = "TACTICAL ACTIONS", accentColor = TacticalCyan) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Button(
                    onClick = {
                        scope.launch {
                            val res = client.syncCurrentTime()
                            statusToast = if (res.isSuccess) "Clock Synced to Phone Time" else "Sync Failed"
                        }
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                    shape = RoundedCornerShape(6.dp),
                    modifier = Modifier.weight(1f)
                ) {
                    Text("SYNC TIME", color = TextPrimary, fontSize = 10.sp, fontFamily = FontFamily.Monospace)
                }

                Button(
                    onClick = {
                        scope.launch {
                            val res = client.syncWeather("Local", 26.0f, 55, 800, "Clear Sky", 28.0f, 19.0f)
                            statusToast = if (res.isSuccess) "Weather Relayed" else "Relay Failed"
                        }
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                    shape = RoundedCornerShape(6.dp),
                    modifier = Modifier.weight(1f)
                ) {
                    Text("PUSH WEATHER", color = TextPrimary, fontSize = 10.sp, fontFamily = FontFamily.Monospace)
                }
            }

            statusToast?.let {
                Spacer(modifier = Modifier.height(8.dp))
                Text(
                    text = "[$it]",
                    color = TacticalAmber,
                    fontSize = 11.sp,
                    fontFamily = FontFamily.Monospace,
                    modifier = Modifier.align(Alignment.CenterHorizontally)
                )
            }
        }

        // 6. Advanced Tactical Subsystems & Operations Cockpit
        TacticalCard(title = "ADVANCED TACTICAL MODULES", accentColor = TacticalCyan) {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                // Mochi Companion
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(6.dp))
                        .background(TacticalSurfaceVariant)
                        .clickable(onClick = onNavigateToMochi)
                        .padding(10.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column {
                        Text("DASAI MOCHI // CYBER-PET", color = TacticalCyan, fontSize = 11.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                        Text("128x64 OLED Emulator, IMU Gaze & Helmets", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    }
                    Text("LAUNCH >", color = TacticalCyan, fontSize = 10.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }

                // Mochi Animation Market
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(6.dp))
                        .background(TacticalSurfaceVariant)
                        .clickable(onClick = onNavigateToAnimMarket)
                        .padding(10.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column {
                        Text("ANIMATION MARKET // 128x64 OLED", color = TacticalAmber, fontSize = 11.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                        Text("38 Offline Animations, Live Canvas, 1-Tap Deploy & Delete", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    }
                    Text("MARKET >", color = TacticalAmber, fontSize = 10.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }

                // Power Governor & ULP Sentinel
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(6.dp))
                        .background(TacticalSurfaceVariant)
                        .clickable(onClick = onNavigateToPower)
                        .padding(10.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column {
                        Text("POWER GOVERNOR // ULP SENTINEL", color = TacticalAmber, fontSize = 11.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                        Text("240/160/80MHz Scaling, ULP Slow-SRAM Lab", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    }
                    Text("MANAGE >", color = TacticalAmber, fontSize = 10.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }

                // SIGINT Recon & Electronic Warfare
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(6.dp))
                        .background(TacticalSurfaceVariant)
                        .clickable(onClick = onNavigateToSigint)
                        .padding(10.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column {
                        Text("TACTICAL SIGINT // RF WARFARE", color = TacticalGreen, fontSize = 11.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                        Text("360° Rotating Radar, 2.4GHz Waterfall & IDS", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    }
                    Text("ENGAGE >", color = TacticalGreen, fontSize = 10.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }

                // Secondary row: APPS, FILES, CONFIG, ALERTS
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    Button(
                        onClick = onNavigateToApps,
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                        shape = RoundedCornerShape(4.dp),
                        modifier = Modifier.weight(1f)
                    ) {
                        Text("APPS", color = TextPrimary, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    }

                    Button(
                        onClick = onNavigateToFiles,
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                        shape = RoundedCornerShape(4.dp),
                        modifier = Modifier.weight(1f)
                    ) {
                        Text("FILES", color = TextPrimary, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    }

                    Button(
                        onClick = onNavigateToNotifications,
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                        shape = RoundedCornerShape(4.dp),
                        modifier = Modifier.weight(1f)
                    ) {
                        Text("ALERTS", color = TextPrimary, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    }

                    Button(
                        onClick = onNavigateToSettings,
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                        shape = RoundedCornerShape(4.dp),
                        modifier = Modifier.weight(1f)
                    ) {
                        Text("CONFIG", color = TextPrimary, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    }
                }
            }
        }
    }
}
