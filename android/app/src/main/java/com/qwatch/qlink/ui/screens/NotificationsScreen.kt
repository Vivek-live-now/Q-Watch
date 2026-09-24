package com.qwatch.qlink.ui.screens

import android.content.Context
import android.widget.Toast
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.model.NotificationPayload
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.ui.components.TacticalCard
import com.qwatch.qlink.ui.theme.*
import kotlinx.coroutines.launch

@Composable
fun NotificationsScreen() {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val prefs = remember { context.getSharedPreferences("qlink_prefs", Context.MODE_PRIVATE) }

    var forwardMaster by remember { mutableStateOf(prefs.getBoolean("forward_notifications", true)) }
    var forwardCalls by remember { mutableStateOf(prefs.getBoolean("forward_calls", true)) }
    var forwardMessages by remember { mutableStateOf(prefs.getBoolean("forward_messages", true)) }
    var alertStyle by remember { mutableStateOf("CHIME") }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(TacticalBackground)
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        TacticalCard(title = "NOTIFICATION FORWARDING", accentColor = TacticalCyan) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "FORWARD TO WATCH",
                    color = TextPrimary,
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace
                )
                Switch(
                    checked = forwardMaster,
                    onCheckedChange = {
                        forwardMaster = it
                        prefs.edit().putBoolean("forward_notifications", it).apply()
                    },
                    colors = SwitchDefaults.colors(checkedThumbColor = TacticalCyan, checkedTrackColor = TacticalCyanDim)
                )
            }

            Spacer(modifier = Modifier.height(8.dp))
            Text(
                text = "Forwards incoming phone notifications over Bluetooth LE or Wi-Fi with custom buzzer chimes & LED flashes.",
                color = TextSecondary,
                fontSize = 11.sp,
                fontFamily = FontFamily.Monospace
            )
        }

        TacticalCard(title = "CATEGORY FILTERS", accentColor = TacticalAmber) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text("INCOMING CALLS", color = TextPrimary, fontSize = 12.sp, fontFamily = FontFamily.Monospace)
                Checkbox(checked = forwardCalls, onCheckedChange = { forwardCalls = it })
            }

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text("MESSAGES (SMS / CHAT)", color = TextPrimary, fontSize = 12.sp, fontFamily = FontFamily.Monospace)
                Checkbox(checked = forwardMessages, onCheckedChange = { forwardMessages = it })
            }
        }

        TacticalCard(title = "ALERT DISPATCH TEST", accentColor = TacticalGreen) {
            Button(
                onClick = {
                    scope.launch {
                        val testPayload = NotificationPayload(
                            appName = "Signal",
                            packageName = "org.thoughtcrime.securesms",
                            title = "Agent 007",
                            body = "Rendezvous at safehouse verified.",
                            category = "MESSAGE",
                            alertStyle = alertStyle,
                            ledColorHex = "#00E5FF"
                        )
                        val res = QLinkClient.instance.sendNotification(testPayload)
                        if (res.isSuccess) {
                            Toast.makeText(context, "Dispatched alert to Q-Watch!", Toast.LENGTH_SHORT).show()
                        } else {
                            Toast.makeText(context, "Dispatch failed (Check connection)", Toast.LENGTH_SHORT).show()
                        }
                    }
                },
                colors = ButtonDefaults.buttonColors(containerColor = TacticalCyan),
                shape = RoundedCornerShape(6.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                Text("TEST NOTIFICATION POPUP ON WATCH", color = OledBlack, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
            }
        }
    }
}
