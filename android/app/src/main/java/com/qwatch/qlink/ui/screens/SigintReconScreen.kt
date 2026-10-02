package com.qwatch.qlink.ui.screens

import android.widget.Toast
import androidx.compose.animation.core.*
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.Radar
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.Wifi
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.ui.components.TacticalCard
import com.qwatch.qlink.ui.theme.*
import kotlin.math.cos
import kotlin.math.sin

data class ReconTarget(
    val name: String,
    val mac: String,
    val rssi: Int,
    val distM: Float,
    val angleDeg: Float
)

@Composable
fun SigintReconScreen(onBack: (() -> Unit)? = null) {
    val context = LocalContext.current
    val client = QLinkClient.instance

    var geigerEnabled by remember { mutableStateOf(true) }
    var selectedTargetIndex by remember { mutableStateOf(0) }
    var statusFeedback by remember { mutableStateOf<String?>(null) }

    // Mock/live radar targets
    val targets = remember {
        listOf(
            ReconTarget("TARGET_ALPHA", "DC:54:75:A1:02:11", -48, 1.8f, 45f),
            ReconTarget("BEACON_BRAVO", "14:2D:27:E5:88:99", -68, 3.4f, 130f),
            ReconTarget("SURVEIL_NODE", "F4:12:FA:90:33:02", -82, 5.2f, 240f)
        )
    }

    val lockedTarget = targets.getOrNull(selectedTargetIndex)

    // Animated radar sweep beam
    val infiniteTransition = rememberInfiniteTransition(label = "radar")
    val sweepAngle by infiniteTransition.animateFloat(
        initialValue = 0f,
        targetValue = 360f,
        animationSpec = infiniteRepeatable(
            animation = tween(3000, easing = LinearEasing),
            repeatMode = RepeatMode.Restart
        ),
        label = "sweep"
    )

    val scrollState = rememberScrollState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(TacticalBackground)
            .padding(16.dp)
            .verticalScroll(scrollState),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // Top Navigation Header
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                if (onBack != null) {
                    IconButton(
                        onClick = onBack,
                        modifier = Modifier
                            .size(32.dp)
                            .background(TacticalSurfaceVariant, RoundedCornerShape(4.dp))
                    ) {
                        Icon(Icons.Default.ArrowBack, contentDescription = "Back", tint = TacticalCyan, modifier = Modifier.size(18.dp))
                    }
                }
                Column {
                    Text(
                        text = "TACTICAL SIGINT // RECON",
                        color = TacticalCyan,
                        fontSize = 15.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                    Text(
                        text = "2.4GHz PROMISCUOUS SNIFFER // BLE RADAR",
                        color = TextMuted,
                        fontSize = 9.sp,
                        fontFamily = FontFamily.Monospace
                    )
                }
            }

            Box(
                modifier = Modifier
                    .clip(RoundedCornerShape(4.dp))
                    .background(TacticalGreenDim)
                    .border(1.dp, TacticalGreen, RoundedCornerShape(4.dp))
                    .padding(horizontal = 8.dp, vertical = 4.dp)
            ) {
                Text(
                    text = "IDS: NOMINAL",
                    color = TacticalGreen,
                    fontSize = 9.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold
                )
            }
        }

        // Section 1: 360° Rotating BLE Radar HUD
        TacticalCard(title = "360° ROTATING BLE RADAR // PROXIMITY HUD", accentColor = TacticalGreen) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(220.dp)
                    .clip(RoundedCornerShape(6.dp))
                    .background(OledBlack)
                    .border(1.5.dp, TacticalGreen, RoundedCornerShape(6.dp))
                    .padding(8.dp)
            ) {
                Canvas(modifier = Modifier.fillMaxSize()) {
                    val w = size.width
                    val h = size.height
                    val cx = w / 2f
                    val cy = h / 2f
                    val maxRadius = minOf(cx, cy) - 10f

                    // Range Rings (0.5m, 1.5m, 3.0m, 5.0m)
                    drawCircle(Color(0x2200FF66), radius = maxRadius * 0.25f, center = Offset(cx, cy), style = Stroke(1f))
                    drawCircle(Color(0x3300FF66), radius = maxRadius * 0.50f, center = Offset(cx, cy), style = Stroke(1f))
                    drawCircle(Color(0x4400FF66), radius = maxRadius * 0.75f, center = Offset(cx, cy), style = Stroke(1f))
                    drawCircle(TacticalGreen, radius = maxRadius, center = Offset(cx, cy), style = Stroke(1.5f))

                    // Crosshair axes
                    drawLine(Color(0x4400FF66), Offset(cx - maxRadius, cy), Offset(cx + maxRadius, cy), strokeWidth = 1f)
                    drawLine(Color(0x4400FF66), Offset(cx, cy - maxRadius), Offset(cx, cy + maxRadius), strokeWidth = 1f)

                    // Sweeping radar beam line
                    val rad = Math.toRadians(sweepAngle.toDouble())
                    val sweepX = cx + (maxRadius * cos(rad)).toFloat()
                    val sweepY = cy + (maxRadius * sin(rad)).toFloat()
                    drawLine(TacticalGreen, Offset(cx, cy), Offset(sweepX, sweepY), strokeWidth = 2f)

                    // Draw plotted target blips
                    targets.forEachIndexed { idx, tgt ->
                        val isLocked = idx == selectedTargetIndex
                        val tRad = Math.toRadians(tgt.angleDeg.toDouble())
                        val tDistFrac = (tgt.distM / 6.0f).coerceIn(0.15f, 0.95f)
                        val blipX = cx + (maxRadius * tDistFrac * cos(tRad)).toFloat()
                        val blipY = cy + (maxRadius * tDistFrac * sin(tRad)).toFloat()

                        val blipColor = if (isLocked) TacticalAmber else TacticalCyan
                        drawCircle(blipColor, radius = if (isLocked) 6f else 4f, center = Offset(blipX, blipY))
                        if (isLocked) {
                            drawCircle(blipColor, radius = 10f, center = Offset(blipX, blipY), style = Stroke(1.5f))
                        }
                    }
                }

                // Radar Corner Overlays
                Column(modifier = Modifier.align(Alignment.TopStart)) {
                    Text("RANGE: 6.0m", color = TacticalGreen, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                    Text("SCAN: BLE 5.0", color = TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                }

                Column(modifier = Modifier.align(Alignment.BottomEnd), horizontalAlignment = Alignment.End) {
                    Text("TARGETS DETECTED: ${targets.size}", color = TacticalCyan, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                    Text(if (geigerEnabled) "GEIGER: ACTIVE" else "GEIGER: MUTED", color = TacticalAmber, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                }
            }

            Spacer(modifier = Modifier.height(10.dp))

            // Locked Target HUD Card
            lockedTarget?.let { tgt ->
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(4.dp))
                        .background(TacticalSurfaceVariant)
                        .border(1.dp, TacticalAmber, RoundedCornerShape(4.dp))
                        .padding(10.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column {
                        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            Text(tgt.name, color = TacticalAmber, fontSize = 11.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                            Text("[LOCKED]", color = TacticalGreen, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                        }
                        Text(tgt.mac, color = TextMuted, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                        Text("RSSI: ${tgt.rssi} dBm // DIST: ${tgt.distM}m", color = TacticalCyan, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    }

                    Button(
                        onClick = {
                            selectedTargetIndex = (selectedTargetIndex + 1) % targets.size
                            Toast.makeText(context, "Cycling radar lock", Toast.LENGTH_SHORT).show()
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalAmberDim),
                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalAmber),
                        shape = RoundedCornerShape(4.dp)
                    ) {
                        Text("CYCLE LOCK", color = TacticalAmber, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    }
                }
            }
        }

        // Section 2: 13-Channel 2.4GHz Wi-Fi Spectrum Congestion
        TacticalCard(title = "2.4GHz RF SPECTRUM WATERFALL (CH 1-13)", accentColor = TacticalCyan) {
            // Optimal Channel Banner
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(4.dp))
                    .background(TacticalSurfaceVariant)
                    .padding(horizontal = 8.dp, vertical = 6.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text("OPTIMAL CHANNEL: CH 11 (LOW NOISE)", color = TacticalGreen, fontSize = 10.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                Text("TOTAL APs: 14", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
            }

            Spacer(modifier = Modifier.height(10.dp))

            // 13 Channel Histogram Bars
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(60.dp),
                horizontalArrangement = Arrangement.spacedBy(4.dp),
                verticalAlignment = Alignment.Bottom
            ) {
                val channelAps = listOf(3, 1, 0, 1, 2, 6, 2, 1, 0, 2, 1, 0, 0)
                channelAps.forEachIndexed { idx, aps ->
                    val ch = idx + 1
                    val heightFrac = (aps / 6f).coerceIn(0.1f, 1f)
                    val barColor = when {
                        aps >= 5 -> TacticalRed
                        aps >= 3 -> TacticalAmber
                        aps > 0 -> TacticalCyan
                        else -> TacticalGreen
                    }
                    Column(
                        modifier = Modifier.weight(1f),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.Bottom
                    ) {
                        Box(
                            modifier = Modifier
                                .fillMaxWidth()
                                .fillMaxHeight(heightFrac)
                                .clip(RoundedCornerShape(topStart = 2.dp, topEnd = 2.dp))
                                .background(barColor)
                        )
                        Spacer(modifier = Modifier.height(2.dp))
                        Text(
                            text = "$ch",
                            color = if (ch == 11) TacticalGreen else TextMuted,
                            fontSize = 7.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = if (ch == 11) FontWeight.Bold else FontWeight.Normal
                        )
                    }
                }
            }
        }

        // Section 3: 802.11 Deauth/Disassociation Attack Sentry (IDS)
        TacticalCard(title = "802.11 DEAUTH ATTACK SENTRY (IDS MONITOR)", accentColor = TacticalRed) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column {
                    Text("INTRUSION DETECTION SYSTEM", color = TextPrimary, fontSize = 11.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    Text("Scans 802.11 management frames for rogue spoofing", color = TextMuted, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                }
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(4.dp))
                        .background(TacticalGreenDim)
                        .border(1.dp, TacticalGreen, RoundedCornerShape(4.dp))
                        .padding(horizontal = 6.dp, vertical = 3.dp)
                ) {
                    Text("NO THREATS", color = TacticalGreen, fontSize = 9.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }
            }

            Spacer(modifier = Modifier.height(8.dp))

            // Attack Log telemetry item
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(4.dp))
                    .background(TacticalSurfaceVariant)
                    .padding(8.dp),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Column {
                    Text("LOGGED ATTACKS: 0", color = TacticalCyan, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    Text("RATE: 0 pkts/sec", color = TextMuted, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                }
                Column(horizontalAlignment = Alignment.End) {
                    Text("PROMISCUOUS: CH 6", color = TacticalAmber, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    Text("FILTER: MGMT_FRAMES", color = TextMuted, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                }
            }
        }

        // Section 4: Promiscuous Packet Sniffer Stream
        TacticalCard(title = "PROMISCUOUS PACKET STREAM // TAXONOMY", accentColor = TacticalCyan) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Column {
                    Text("THROUGHPUT", color = TextMuted, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                    Text("142 PKTS/S", color = TacticalCyan, fontSize = 14.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }
                Column {
                    Text("MANAGEMENT", color = TextMuted, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                    Text("35%", color = TacticalAmber, fontSize = 14.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }
                Column {
                    Text("CONTROL", color = TextMuted, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                    Text("25%", color = TacticalGreen, fontSize = 14.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }
                Column {
                    Text("DATA", color = TextMuted, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                    Text("40%", color = TacticalCyan, fontSize = 14.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }
            }
        }
    }
}
