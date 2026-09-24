package com.qwatch.qlink.ui.screens

import android.content.Context
import android.widget.Toast
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.model.ConnectionState
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.protocol.QLinkConstants
import com.qwatch.qlink.ui.components.TacticalCard
import com.qwatch.qlink.ui.theme.*
import kotlinx.coroutines.launch

@Composable
fun SettingsScreen() {
    val client = QLinkClient.instance
    val connectionState by client.connectionState.collectAsState()
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val prefs = remember { context.getSharedPreferences("qlink_prefs", Context.MODE_PRIVATE) }

    var wifiHost by remember { mutableStateOf(prefs.getString("wifi_host", QLinkConstants.DEFAULT_HOTSPOT_IP) ?: QLinkConstants.DEFAULT_HOTSPOT_IP) }
    var bleMac by remember { mutableStateOf(prefs.getString("ble_mac", "CC:7B:5C:80:12:34") ?: "CC:7B:5C:80:12:34") }
    val scrollState = rememberScrollState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(TacticalBackground)
            .padding(16.dp)
            .verticalScroll(scrollState),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
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

        // BLE Transport Card
        TacticalCard(title = "BLUETOOTH LE (LOW POWER)", accentColor = TacticalAmber) {
            OutlinedTextField(
                value = bleMac,
                onValueChange = {
                    bleMac = it
                    prefs.edit().putString("ble_mac", it).apply()
                },
                label = { Text("Q-Watch BLE MAC Address", color = TextMuted) },
                singleLine = true,
                modifier = Modifier.fillMaxWidth()
            )

            Spacer(modifier = Modifier.height(10.dp))

            Button(
                onClick = {
                    scope.launch {
                        val res = client.connectBle(bleMac)
                        Toast.makeText(context, if (res.isSuccess) "BLE Link Initiated" else "BLE Failed", Toast.LENGTH_SHORT).show()
                    }
                },
                colors = ButtonDefaults.buttonColors(containerColor = TacticalAmber),
                shape = RoundedCornerShape(6.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                Text("CONNECT BLE", color = OledBlack, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
            }
        }

        // Protocol Info Card
        TacticalCard(title = "PROTOCOL CONTRACT", accentColor = TacticalGreen) {
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text(text = "PROTOCOL: Q-Link v${QLinkConstants.PROTOCOL_VERSION}", color = TextPrimary, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                Text(text = "FRAME BUFFER: 128x64 1-bit (1024 Bytes)", color = TextSecondary, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                Text(text = "GATT SERVICE: ${QLinkConstants.SERVICE_UUID}", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                Text(text = "ARCHITECTURE: Clean Layered / Non-Interference", color = TacticalGreen, fontSize = 10.sp, fontFamily = FontFamily.Monospace)
            }
        }
    }
}
