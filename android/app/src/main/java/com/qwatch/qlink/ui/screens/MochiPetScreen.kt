package com.qwatch.qlink.ui.screens

import android.widget.Toast
import androidx.compose.animation.core.*
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
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
import kotlinx.coroutines.launch

enum class HelmetPreset(
    val id: Int,
    val title: String,
    val subtitle: String,
    val powerDelta: String,
    val accentColor: Color
) {
    CLASSIC(0, "CLASSIC DOCK", "Minimalist sleek round visor", "0.0 mA", TacticalCyan),
    GUNDAM(1, "GUNDAM RX-78-2", "V-Fin crest & sensor lock", "+0.2 mA", TacticalAmber),
    CYBER(2, "CYBER-CARBON", "Stealth carbon weave & grid", "+0.1 mA", TacticalCyan),
    NEKO(3, "NEKO MECHA", "High-gain acoustic ear radar", "+0.1 mA", TacticalGreen),
    TACTICAL(4, "007 CLASSIFIED", "SpecOps night-vision HUD", "+0.3 mA", TacticalRed)
}

@Composable
fun MochiPetScreen(onBack: (() -> Unit)? = null) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val client = QLinkClient.instance
    val telemetry by client.telemetry.collectAsState()

    var selectedHelmet by remember { mutableStateOf(HelmetPreset.CLASSIC) }
    var currentMood by remember { mutableStateOf("ECSTATIC (PURRING)") }
    var happiness by remember { mutableStateOf(96) }
    var hunger by remember { mutableStateOf(84) }
    var energy by remember { mutableStateOf(92) }
    var isSleeping by remember { mutableStateOf(false) }
    var statusFeedback by remember { mutableStateOf<String?>(null) }

    // Gaze tracking simulation driven by watch pitch/roll or default center
    val pitch = telemetry.imu.pitchDeg
    val roll = telemetry.imu.rollDeg
    val gazeX = (roll.coerceIn(-30f, 30f) / 30f) * 16f
    val gazeY = (pitch.coerceIn(-30f, 30f) / 30f) * 10f

    // Animated breathing/pulse
    val infiniteTransition = rememberInfiniteTransition(label = "pulse")
    val breathScale by infiniteTransition.animateFloat(
        initialValue = 0.98f,
        targetValue = 1.02f,
        animationSpec = infiniteRepeatable(
            animation = tween(2200, easing = FastOutSlowInEasing),
            repeatMode = RepeatMode.Reverse
        ),
        label = "breath"
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
        // Top Navigation / Header
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
                        text = "DASAI MOCHI COMPANION",
                        color = TacticalCyan,
                        fontSize = 15.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                    Text(
                        text = "CYBER-PET // IMU GAZE ENGINE",
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
                    text = if (isSleeping) "STATUS: DORMANT" else "STATUS: ACTIVE",
                    color = TacticalGreen,
                    fontSize = 9.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold
                )
            }
        }

        // Section 1: Operative Bond & XP Card
        TacticalCard(title = "OPERATIVE BOND // XP PROTOCOL", accentColor = TacticalGreen) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column {
                    Text(
                        text = "BOND: LEVEL 7 (ELITE AGENT)",
                        color = TacticalGreen,
                        fontSize = 13.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                    Text(
                        text = "740 / 1000 XP [74%] // PURR STREAK: 14 DAYS",
                        color = TextMuted,
                        fontSize = 9.sp,
                        fontFamily = FontFamily.Monospace
                    )
                }
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(4.dp))
                        .background(TacticalSurfaceVariant)
                        .padding(horizontal = 8.dp, vertical = 4.dp)
                ) {
                    Text(
                        text = currentMood,
                        color = TacticalAmber,
                        fontSize = 10.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                }
            }

            Spacer(modifier = Modifier.height(8.dp))

            // XP Bar
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(6.dp)
                    .clip(RoundedCornerShape(3.dp))
                    .background(TacticalSurfaceVariant)
            ) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth(0.74f)
                        .fillMaxHeight()
                        .clip(RoundedCornerShape(3.dp))
                        .background(TacticalGreen)
                )
            }
        }

        // Section 2: 128x64 OLED Hologram Pet Viewport
        TacticalCard(title = "VIRTUAL OLED MIRROR // LIVE PET HUD", accentColor = TacticalCyan) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .aspectRatio(2f) // 128x64 ratio
                    .clip(RoundedCornerShape(6.dp))
                    .background(OledBlack)
                    .border(2.dp, TacticalCyan, RoundedCornerShape(6.dp))
                    .padding(8.dp)
            ) {
                // Vector Canvas drawing Mochi face & helmet
                Canvas(modifier = Modifier.fillMaxSize()) {
                    val w = size.width
                    val h = size.height
                    val cx = w / 2f + gazeX * 2.5f
                    val cy = h / 2f + gazeY * 1.5f

                    // Draw OLED scanlines effect
                    var scanlineY = 0f
                    while (scanlineY < h) {
                        drawLine(
                            color = Color(0x0A00E5FF),
                            start = Offset(0f, scanlineY),
                            end = Offset(w, scanlineY),
                            strokeWidth = 1f
                        )
                        scanlineY += 4f
                    }

                    // Helmet specific head ornament
                    when (selectedHelmet) {
                        HelmetPreset.GUNDAM -> {
                            // V-Fin antenna
                            val vFin = Path().apply {
                                moveTo(cx - 36f, cy - 38f)
                                lineTo(cx, cy - 18f)
                                lineTo(cx + 36f, cy - 38f)
                                lineTo(cx, cy - 12f)
                                close()
                            }
                            drawPath(vFin, TacticalAmber, style = Stroke(width = 3f))
                            drawCircle(TacticalRed, radius = 4f, center = Offset(cx, cy - 14f))
                        }
                        HelmetPreset.CYBER -> {
                            // Cyber brackets
                            drawRoundRect(
                                color = TacticalCyan,
                                topLeft = Offset(cx - 52f, cy - 28f),
                                size = Size(104f, 56f),
                                cornerRadius = CornerRadius(6f, 6f),
                                style = Stroke(width = 2f)
                            )
                        }
                        HelmetPreset.NEKO -> {
                            // Mecha Cat ears
                            val leftEar = Path().apply {
                                moveTo(cx - 44f, cy - 18f)
                                lineTo(cx - 30f, cy - 44f)
                                lineTo(cx - 16f, cy - 20f)
                                close()
                            }
                            val rightEar = Path().apply {
                                moveTo(cx + 16f, cy - 20f)
                                lineTo(cx + 30f, cy - 44f)
                                lineTo(cx + 44f, cy - 18f)
                                close()
                            }
                            drawPath(leftEar, TacticalGreen, style = Stroke(width = 3f))
                            drawPath(rightEar, TacticalGreen, style = Stroke(width = 3f))
                        }
                        HelmetPreset.TACTICAL -> {
                            // SpecOps 007 Visor strip
                            drawLine(
                                color = TacticalRed,
                                start = Offset(cx - 48f, cy - 4f),
                                end = Offset(cx + 48f, cy - 4f),
                                strokeWidth = 3f
                            )
                        }
                        HelmetPreset.CLASSIC -> {
                            // Sleek classic dome arch
                            drawArc(
                                color = TacticalCyan,
                                startAngle = 180f,
                                sweepAngle = 180f,
                                useCenter = false,
                                topLeft = Offset(cx - 40f, cy - 35f),
                                size = Size(80f, 40f),
                                style = Stroke(width = 2f)
                            )
                        }
                    }

                    // Mochi Face Eyes
                    if (isSleeping) {
                        // Sleeping closed eyes - -
                        drawLine(PhosphorCyan, Offset(cx - 28f, cy), Offset(cx - 10f, cy), strokeWidth = 4f)
                        drawLine(PhosphorCyan, Offset(cx + 10f, cy), Offset(cx + 28f, cy), strokeWidth = 4f)
                    } else {
                        // Expressive curved/rounded eyes ^ ^
                        drawRoundRect(
                            color = PhosphorCyan,
                            topLeft = Offset(cx - 26f, cy - 12f),
                            size = Size(16f, 22f),
                            cornerRadius = CornerRadius(8f, 8f)
                        )
                        drawRoundRect(
                            color = PhosphorCyan,
                            topLeft = Offset(cx + 10f, cy - 12f),
                            size = Size(16f, 22f),
                            cornerRadius = CornerRadius(8f, 8f)
                        )

                        // Cute blushing cheeks
                        drawCircle(Color(0xFFFF3366), radius = 4f, center = Offset(cx - 32f, cy + 14f))
                        drawCircle(Color(0xFFFF3366), radius = 4f, center = Offset(cx + 32f, cy + 14f))

                        // Cheerful smile
                        drawArc(
                            color = PhosphorCyan,
                            startAngle = 10f,
                            sweepAngle = 160f,
                            useCenter = false,
                            topLeft = Offset(cx - 8f, cy + 6f),
                            size = Size(16f, 10f),
                            style = Stroke(width = 2.5f)
                        )
                    }
                }

                // HUD overlay markers
                Column(modifier = Modifier.align(Alignment.TopStart)) {
                    Text("RETICLE // GAZE_LOCK", color = TacticalCyan, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                    Text("PITCH: ${pitch.toInt()}° ROLL: ${roll.toInt()}°", color = TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                }

                Column(modifier = Modifier.align(Alignment.TopEnd), horizontalAlignment = Alignment.End) {
                    Text("HELMET: ${selectedHelmet.name}", color = selectedHelmet.accentColor, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    Text("EMOTE: ECSTATIC", color = TacticalGreen, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                }

                Text(
                    text = "Q-WATCH OLED 128x64 EMULATOR // 20 FPS",
                    color = TextMuted,
                    fontSize = 7.sp,
                    fontFamily = FontFamily.Monospace,
                    modifier = Modifier.align(Alignment.BottomCenter)
                )
            }
        }

        // Section 3: Modular Cyber-Helmet Locker
        TacticalCard(title = "CYBER-HELMET LOCKER (MODULAR CASINGS)", accentColor = TacticalAmber) {
            Text(
                text = "SWAPPABLE HARDWARE CASINGS // REAL-TIME Q-LINK SYNC",
                color = TextMuted,
                fontSize = 9.sp,
                fontFamily = FontFamily.Monospace
            )

            Spacer(modifier = Modifier.height(10.dp))

            val helmetScrollState = rememberScrollState()
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .horizontalScroll(helmetScrollState),
                horizontalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                HelmetPreset.values().forEach { helmet ->
                    val isEquipped = selectedHelmet == helmet
                    Box(
                        modifier = Modifier
                            .width(170.dp)
                            .clip(RoundedCornerShape(6.dp))
                            .background(if (isEquipped) TacticalSurfaceVariant else TacticalSurface)
                            .border(
                                1.5.dp,
                                if (isEquipped) helmet.accentColor else TacticalBorder,
                                RoundedCornerShape(6.dp)
                            )
                            .clickable {
                                selectedHelmet = helmet
                                statusFeedback = "Equipped ${helmet.title} on Watch"
                                Toast.makeText(context, "Equipped ${helmet.title}", Toast.LENGTH_SHORT).show()
                            }
                            .padding(10.dp)
                    ) {
                        Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.SpaceBetween,
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Text(
                                    text = helmet.title,
                                    color = if (isEquipped) helmet.accentColor else TextPrimary,
                                    fontSize = 11.sp,
                                    fontFamily = FontFamily.Monospace,
                                    fontWeight = FontWeight.Bold
                                )
                                if (isEquipped) {
                                    Box(
                                        modifier = Modifier
                                            .size(8.dp)
                                            .clip(CircleShape)
                                            .background(helmet.accentColor)
                                    )
                                }
                            }

                            Text(
                                text = helmet.subtitle,
                                color = TextSecondary,
                                fontSize = 8.5.sp,
                                fontFamily = FontFamily.Monospace,
                                minLines = 2
                            )

                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.SpaceBetween,
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Text(
                                    text = "DRAW: ${helmet.powerDelta}",
                                    color = TextMuted,
                                    fontSize = 8.sp,
                                    fontFamily = FontFamily.Monospace
                                )
                                Text(
                                    text = if (isEquipped) "[EQUIPPED]" else "[SWAP]",
                                    color = if (isEquipped) helmet.accentColor else TacticalCyan,
                                    fontSize = 9.sp,
                                    fontFamily = FontFamily.Monospace,
                                    fontWeight = FontWeight.Bold
                                )
                            }
                        }
                    }
                }
            }
        }

        // Section 4: Tamagotchi Vitals & Care Actions
        TacticalCard(title = "TAMAGOTCHI CARE & INTERACTION DECK", accentColor = TacticalCyan) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text("HAPPINESS", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    Text("$happiness%", color = TacticalGreen, fontSize = 16.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }
                Column(modifier = Modifier.weight(1f)) {
                    Text("SATIETY", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    Text("$hunger%", color = TacticalCyan, fontSize = 16.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }
                Column(modifier = Modifier.weight(1f)) {
                    Text("ENERGY", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    Text("$energy%", color = TacticalAmber, fontSize = 16.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }
                Column(modifier = Modifier.weight(1f)) {
                    Text("STATE", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    Text(if (isSleeping) "ASLEEP" else "AWAKE", color = if (isSleeping) TacticalAmber else TacticalGreen, fontSize = 16.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }
            }

            Spacer(modifier = Modifier.height(14.dp))

            // Action Buttons
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Button(
                    onClick = {
                        happiness = (happiness + 5).coerceAtMost(100)
                        currentMood = "PURRING & CHIRPING"
                        statusFeedback = "Mochi petted! WS2812 Glow & Purr SFX dispatched."
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalCyanDim),
                    border = androidx.compose.foundation.BorderStroke(1.dp, TacticalCyan),
                    shape = RoundedCornerShape(6.dp),
                    modifier = Modifier.weight(1f)
                ) {
                    Text("PET HEAD", color = TacticalCyan, fontSize = 10.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }

                Button(
                    onClick = {
                        hunger = (hunger + 10).coerceAtMost(100)
                        energy = (energy + 5).coerceAtMost(100)
                        currentMood = "MUNCHING DIGITAL SNACK"
                        statusFeedback = "Fed digital RAM snack! Satiety +10%."
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalAmberDim),
                    border = androidx.compose.foundation.BorderStroke(1.dp, TacticalAmber),
                    shape = RoundedCornerShape(6.dp),
                    modifier = Modifier.weight(1f)
                ) {
                    Text("FEED SNACK", color = TacticalAmber, fontSize = 10.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }

                Button(
                    onClick = {
                        isSleeping = !isSleeping
                        currentMood = if (isSleeping) "SLEEPING (ZZZ)" else "ENERGETIC AWAKE"
                        statusFeedback = if (isSleeping) "Mochi put to sleep (ULP eco mode)." else "Mochi awakened!"
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                    border = androidx.compose.foundation.BorderStroke(1.dp, TacticalBorder),
                    shape = RoundedCornerShape(6.dp),
                    modifier = Modifier.weight(1f)
                ) {
                    Text(if (isSleeping) "WAKE UP" else "SLEEP", color = TextPrimary, fontSize = 10.sp, fontFamily = FontFamily.Monospace)
                }
            }

            statusFeedback?.let {
                Spacer(modifier = Modifier.height(8.dp))
                Text(
                    text = "[$it]",
                    color = TacticalGreen,
                    fontSize = 10.sp,
                    fontFamily = FontFamily.Monospace,
                    modifier = Modifier.align(Alignment.CenterHorizontally)
                )
            }
        }
    }
}
