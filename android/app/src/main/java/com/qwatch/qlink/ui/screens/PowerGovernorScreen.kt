package com.qwatch.qlink.ui.screens

import android.widget.Toast
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
import androidx.compose.material.icons.filled.BatteryChargingFull
import androidx.compose.material.icons.filled.Bolt
import androidx.compose.material.icons.filled.Power
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.ui.components.TacticalCard
import com.qwatch.qlink.ui.theme.*

enum class CpuGovernorMode(
    val title: String,
    val clockMhz: Int,
    val description: String,
    val runtimeEst: String,
    val color: Color
) {
    PERFORMANCE("PERFORMANCE", 240, "Dual-Core 240MHz, 60 FPS Raycasting", "~18h", TacticalAmber),
    BALANCED("BALANCED", 160, "Dual-Core 160MHz Dynamic Light Sleep", "~36h", TacticalCyan),
    ENDURANCE("ENDURANCE", 80, "Single-Core 80MHz Eco Polling", "~72h", TacticalGreen),
    ULP_SENTINEL("ULP SENTINEL", 18, "RISC-V Coprocessor 17.5MHz Sleep Hub", "~140h", TacticalGreen)
}

@Composable
fun PowerGovernorScreen(onBack: (() -> Unit)? = null) {
    val context = LocalContext.current
    val client = QLinkClient.instance
    val telemetry by client.telemetry.collectAsState()

    var activeGovernor by remember { mutableStateOf(CpuGovernorMode.BALANCED) }
    var rfAutoCut by remember { mutableStateOf(true) }
    var neoPixelEcoMute by remember { mutableStateOf(false) }
    var buzzerEcoMute by remember { mutableStateOf(false) }
    var oledDim by remember { mutableStateOf(false) }
    var statusFeedback by remember { mutableStateOf<String?>(null) }

    val batteryPct = telemetry.batteryPct.coerceIn(0, 100)
    val batteryV = if (telemetry.batteryMv > 0) telemetry.batteryMv / 1000f else 3.92f
    val activeSegments = (batteryPct / 10).coerceIn(0, 10)

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
                        text = "POWER GOVERNOR // ULP SENTINEL",
                        color = TacticalCyan,
                        fontSize = 15.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                    Text(
                        text = "PMU: AXP2101 // ESP32-S3 DUAL-CORE",
                        color = TextMuted,
                        fontSize = 9.sp,
                        fontFamily = FontFamily.Monospace
                    )
                }
            }

            Box(
                modifier = Modifier
                    .clip(RoundedCornerShape(4.dp))
                    .background(TacticalCyanDim)
                    .border(1.dp, TacticalCyan, RoundedCornerShape(4.dp))
                    .padding(horizontal = 8.dp, vertical = 4.dp)
            ) {
                Text(
                    text = "GOVERNOR: ${activeGovernor.name}",
                    color = TacticalCyan,
                    fontSize = 9.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold
                )
            }
        }

        // Section 1: Primary Cell Status & 10-Segment Bar
        TacticalCard(title = "PRIMARY CELL STATUS // LI-PO 1S", accentColor = TacticalCyan) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Large Battery Percentage Display
                Column {
                    Row(verticalAlignment = Alignment.Bottom) {
                        Text(
                            text = "$batteryPct",
                            color = TacticalCyan,
                            fontSize = 44.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.ExtraBold,
                            letterSpacing = (-1).sp
                        )
                        Text(
                            text = "%",
                            color = TacticalCyan,
                            fontSize = 20.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold,
                            modifier = Modifier.padding(bottom = 6.dp, start = 2.dp)
                        )
                    }
                    Text(
                        text = "VOLTAGE: ${String.format("%.2f", batteryV)}V // CURRENT: 2.8 mA",
                        color = TextMuted,
                        fontSize = 9.5.sp,
                        fontFamily = FontFamily.Monospace
                    )
                }

                // 10-Segment Illuminated Gauge
                Column(
                    modifier = Modifier
                        .width(130.dp)
                        .clip(RoundedCornerShape(4.dp))
                        .background(TacticalSurfaceVariant)
                        .border(1.dp, TacticalBorder, RoundedCornerShape(4.dp))
                        .padding(6.dp)
                ) {
                    Text(
                        text = "10-SEG BUS",
                        color = TextMuted,
                        fontSize = 8.sp,
                        fontFamily = FontFamily.Monospace,
                        modifier = Modifier.align(Alignment.End)
                    )
                    Spacer(modifier = Modifier.height(4.dp))
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(3.dp)
                    ) {
                        for (i in 0 until 10) {
                            val isActive = i < activeSegments
                            val segColor = when {
                                i < 2 -> TacticalRed
                                i < 5 -> TacticalAmber
                                else -> TacticalGreen
                            }
                            Box(
                                modifier = Modifier
                                    .weight(1f)
                                    .height(14.dp)
                                    .background(if (isActive) segColor else TacticalBorder)
                            )
                        }
                    }
                }
            }

            Spacer(modifier = Modifier.height(10.dp))

            // Estimated Runtime Banner
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(4.dp))
                    .background(TacticalSurfaceVariant)
                    .padding(horizontal = 10.dp, vertical = 6.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "ESTIMATED RUNTIME: ${activeGovernor.runtimeEst}",
                    color = TacticalGreen,
                    fontSize = 10.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold
                )
                Text(
                    text = "HEALTH: 100% NOMINAL",
                    color = TextSecondary,
                    fontSize = 8.5.sp,
                    fontFamily = FontFamily.Monospace
                )
            }
        }

        // Section 2: Dynamic CPU Frequency Governor Matrix
        TacticalCard(title = "DYNAMIC FREQUENCY GOVERNORS", accentColor = TacticalAmber) {
            Text(
                text = "ESP32-S3 DUAL XTENSA LX7 + RISC-V ULP COPROCESSOR",
                color = TextMuted,
                fontSize = 9.sp,
                fontFamily = FontFamily.Monospace
            )

            Spacer(modifier = Modifier.height(8.dp))

            CpuGovernorMode.values().forEach { mode ->
                val isSelected = activeGovernor == mode
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp)
                        .clip(RoundedCornerShape(6.dp))
                        .background(if (isSelected) TacticalSurfaceVariant else TacticalSurface)
                        .border(
                            1.dp,
                            if (isSelected) mode.color else TacticalBorder,
                            RoundedCornerShape(6.dp)
                        )
                        .clickable {
                            activeGovernor = mode
                            statusFeedback = "Applied ${mode.title} governor (${mode.clockMhz} MHz)"
                            Toast.makeText(context, "Governor: ${mode.title}", Toast.LENGTH_SHORT).show()
                        }
                        .padding(10.dp)
                ) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Column(modifier = Modifier.weight(1f)) {
                            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                Text(
                                    text = mode.title,
                                    color = if (isSelected) mode.color else TextPrimary,
                                    fontSize = 11.5.sp,
                                    fontFamily = FontFamily.Monospace,
                                    fontWeight = FontWeight.Bold
                                )
                                Text(
                                    text = "[${mode.clockMhz} MHz]",
                                    color = if (isSelected) mode.color else TextMuted,
                                    fontSize = 9.sp,
                                    fontFamily = FontFamily.Monospace
                                )
                            }
                            Text(
                                text = mode.description,
                                color = TextSecondary,
                                fontSize = 8.5.sp,
                                fontFamily = FontFamily.Monospace
                            )
                        }

                        Text(
                            text = mode.runtimeEst,
                            color = if (isSelected) mode.color else TextMuted,
                            fontSize = 11.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                    }
                }
            }
        }

        // Section 3: 24h Discharge Graph & Threshold Safety Line
        TacticalCard(title = "24-HOUR VOLTAGE CURVE & CUTOFF THRESHOLD", accentColor = TacticalCyan) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(100.dp)
                    .clip(RoundedCornerShape(4.dp))
                    .background(OledBlack)
                    .border(1.dp, TacticalBorder, RoundedCornerShape(4.dp))
                    .padding(8.dp)
            ) {
                Canvas(modifier = Modifier.fillMaxSize()) {
                    val w = size.width
                    val h = size.height

                    // Grid lines
                    drawLine(Color(0x22FFFFFF), Offset(0f, h * 0.25f), Offset(w, h * 0.25f), strokeWidth = 1f)
                    drawLine(Color(0x22FFFFFF), Offset(0f, h * 0.5f), Offset(w, h * 0.5f), strokeWidth = 1f)
                    drawLine(Color(0x22FFFFFF), Offset(0f, h * 0.75f), Offset(w, h * 0.75f), strokeWidth = 1f)

                    // Safe 3.20V Cutoff line (Crimson dashed)
                    val cutoffY = h * 0.85f
                    drawLine(TacticalRed, Offset(0f, cutoffY), Offset(w, cutoffY), strokeWidth = 1.5f)

                    // Discharge Path
                    val curve = Path().apply {
                        moveTo(0f, h * 0.15f)
                        lineTo(w * 0.25f, h * 0.20f)
                        lineTo(w * 0.50f, h * 0.25f)
                        lineTo(w * 0.75f, h * 0.32f)
                        lineTo(w, h * 0.38f)
                    }
                    drawPath(curve, TacticalCyan, style = Stroke(width = 2.5f))
                }

                Text(
                    text = "4.20V (100%)",
                    color = TextMuted,
                    fontSize = 7.sp,
                    fontFamily = FontFamily.Monospace,
                    modifier = Modifier.align(Alignment.TopStart)
                )

                Text(
                    text = "3.20V SAFE CUTOFF",
                    color = TacticalRed,
                    fontSize = 7.5.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold,
                    modifier = Modifier.align(Alignment.BottomEnd)
                )
            }
        }

        // Section 4: ESP32-S3 ULP RISC-V Slow-SRAM Lab
        TacticalCard(title = "ULP RISC-V COP-PROCESSOR // SLOW SRAM", accentColor = TacticalGreen) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Column {
                    Text("SLOW SRAM ALLOCATION", color = TextMuted, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                    Text("2048 / 8192 BYTES [25%]", color = TacticalGreen, fontSize = 12.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }
                Column(horizontalAlignment = Alignment.End) {
                    Text("WAKE EVENTS", color = TextMuted, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                    Text("142 INTERRUPTS", color = TacticalCyan, fontSize = 12.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }
            }

            Spacer(modifier = Modifier.height(8.dp))

            // Slow SRAM visual memory blocks
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(4.dp)
            ) {
                // 4 blocks of 2KB
                Box(modifier = Modifier.weight(1f).height(10.dp).background(TacticalGreen)) // Used
                Box(modifier = Modifier.weight(1f).height(10.dp).background(TacticalBorder)) // Free
                Box(modifier = Modifier.weight(1f).height(10.dp).background(TacticalBorder)) // Free
                Box(modifier = Modifier.weight(1f).height(10.dp).background(TacticalBorder)) // Free
            }
        }

        // Section 5: Peripheral Load-Shedding Switches
        TacticalCard(title = "TACTICAL LOAD-SHEDDING SWITCHES", accentColor = TacticalCyan) {
            // Switch 1: RF Auto-Kill
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text("RF AUTO-KILL ON LOCK", color = TextPrimary, fontSize = 11.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    Text("Kills BLE/Wi-Fi modem 30s after screen sleep", color = TextMuted, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                }
                Switch(
                    checked = rfAutoCut,
                    onCheckedChange = { rfAutoCut = it },
                    colors = SwitchDefaults.colors(checkedThumbColor = TacticalCyan, checkedTrackColor = TacticalCyanDim)
                )
            }

            Divider(color = TacticalBorder, modifier = Modifier.padding(vertical = 6.dp))

            // Switch 2: NeoPixel Eco Mute
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text("NEOPIXEL RGB ECO-MUTE", color = TextPrimary, fontSize = 11.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    Text("Disables WS2812 ring (saves ~18mA at peak glow)", color = TextMuted, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                }
                Switch(
                    checked = neoPixelEcoMute,
                    onCheckedChange = { neoPixelEcoMute = it },
                    colors = SwitchDefaults.colors(checkedThumbColor = TacticalCyan, checkedTrackColor = TacticalCyanDim)
                )
            }

            Divider(color = TacticalBorder, modifier = Modifier.padding(vertical = 6.dp))

            // Switch 3: Buzzer Mute
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text("BUZZER ACOUSTIC LIMITER", color = TextPrimary, fontSize = 11.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    Text("Caps piezo chime amplitude to 20% PWM", color = TextMuted, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                }
                Switch(
                    checked = buzzerEcoMute,
                    onCheckedChange = { buzzerEcoMute = it },
                    colors = SwitchDefaults.colors(checkedThumbColor = TacticalCyan, checkedTrackColor = TacticalCyanDim)
                )
            }
        }
    }
}
