package com.qwatch.qlink.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.model.ConnectionState
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.ui.components.*
import com.qwatch.qlink.ui.theme.*
import kotlinx.coroutines.launch

@Composable
fun DashboardScreen(
    onNavigateToMirror: () -> Unit,
    onNavigateToSensors: () -> Unit
) {
    val client = QLinkClient.instance
    val connectionState by client.connectionState.collectAsState()
    val telemetry by client.telemetry.collectAsState()
    val frame by client.displayFrame.collectAsState()
    val scope = rememberCoroutineScope()
    var statusToast by remember { mutableStateOf<String?>(null) }

    val scrollState = rememberScrollState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(TacticalBackground)
            .padding(16.dp)
            .verticalScroll(scrollState),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
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
                }
            }
        }

        // 2. Mini OLED Simulation Preview
        TacticalCard(title = "LIVE OLED MIRROR", accentColor = TacticalAmber) {
            VirtualOledDisplay(
                frame = frame,
                isStreaming = client.isConnected(),
                modifier = Modifier.fillMaxWidth()
            )
            Spacer(modifier = Modifier.height(8.dp))
            Button(
                onClick = onNavigateToMirror,
                colors = ButtonDefaults.buttonColors(containerColor = TacticalAmber),
                shape = RoundedCornerShape(6.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                Text(
                    text = "OPEN FULL VIRTUAL REMOTE",
                    color = OledBlack,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold,
                    fontSize = 12.sp
                )
            }
        }

        // 3. Environmental & Biometrics Overview
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

        // 4. Quick Action Dispatch
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
    }
}
