package com.qwatch.qlink.ui.components

import android.view.HapticFeedbackConstants
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.irdb.IrParsedButton
import com.qwatch.qlink.irdb.IrRemoteFile
import com.qwatch.qlink.ui.theme.*
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

enum class RemoteCategory(val displayName: String, val icon: ImageVector) {
    TV("TV REMOTE", Icons.Default.Tv),
    AC("AC CLIMATE", Icons.Default.AcUnit),
    RGB_LED("RGB LED LIGHT", Icons.Default.Lightbulb)
}

fun detectRemoteCategory(remote: IrRemoteFile?): RemoteCategory {
    if (remote == null) return RemoteCategory.TV
    val searchStr = "${remote.entry.path} ${remote.entry.deviceType} ${remote.entry.filename}".lowercase()
    val btnNames = remote.buttons.map { it.name.lowercase() }

    // 1. Air Conditioner detection
    if (searchStr.contains("air") || searchStr.contains("ac") || searchStr.contains("hvac") ||
        searchStr.contains("climate") || btnNames.any {
            it.contains("temp") || it.contains("cool") || it.contains("heat") || it.contains("vane") || it.contains("swing")
        }
    ) {
        return RemoteCategory.AC
    }

    // 2. RGB LED Light detection
    if (searchStr.contains("led") || searchStr.contains("light") || searchStr.contains("rgb") ||
        searchStr.contains("strip") || searchStr.contains("bulb") || searchStr.contains("lamp") ||
        btnNames.any { it.contains("flash") || it.contains("strobe") || it.contains("fade") || it.contains("smooth") }
    ) {
        return RemoteCategory.RGB_LED
    }

    // 3. Default to TV / Media
    return RemoteCategory.TV
}

fun findSignal(buttons: List<IrParsedButton>, vararg aliases: String): IrParsedButton? {
    for (alias in aliases) {
        val cleanAlias = alias.lowercase().replace("[_\\s\\-]".toRegex(), "")
        // Exact normalized match
        val exact = buttons.firstOrNull {
            it.name.lowercase().replace("[_\\s\\-]".toRegex(), "") == cleanAlias
        }
        if (exact != null) return exact

        // Substring match
        val partial = buttons.firstOrNull {
            it.name.lowercase().replace("[_\\s\\-]".toRegex(), "").contains(cleanAlias)
        }
        if (partial != null) return partial
    }
    return null
}

@Composable
fun AdaptiveVirtualRemote(
    remoteFile: IrRemoteFile?,
    onTransmit: (IrParsedButton) -> Unit,
    onFlashToWatch: (() -> Unit)? = null,
    onClose: (() -> Unit)? = null,
    modifier: Modifier = Modifier
) {
    val view = LocalView.current
    val scope = rememberCoroutineScope()

    var activeCategory by remember(remoteFile) {
        mutableStateOf(detectRemoteCategory(remoteFile))
    }
    var isBlasting by remember { mutableStateOf(false) }
    var lastBlastedSignal by remember { mutableStateOf<IrParsedButton?>(null) }
    var blastFeedbackText by remember { mutableStateOf<String?>(null) }

    // AC Remote state simulation
    var acTemp by remember { mutableIntStateOf(24) }
    var acPower by remember { mutableStateOf(true) }
    var acMode by remember { mutableStateOf("COOL") }
    var acFan by remember { mutableStateOf("AUTO") }
    var acSwing by remember { mutableStateOf(true) }

    // RGB LED state simulation
    var ledActiveColor by remember { mutableStateOf(Color(0xFF00F0FF)) }
    var ledBrightness by remember { mutableIntStateOf(100) }

    val buttons = remoteFile?.buttons ?: emptyList()

    fun triggerBlast(btn: IrParsedButton?, syntheticLabel: String) {
        view.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
        scope.launch {
            isBlasting = true
            if (btn != null) {
                lastBlastedSignal = btn
                blastFeedbackText = "TX: ${btn.name} [${if (btn.protocol.isNotEmpty()) "${btn.protocol} ${btn.command}" else "RAW"}]"
                onTransmit(btn)
            } else {
                blastFeedbackText = "TX: $syntheticLabel (Simulated)"
            }
            delay(250)
            isBlasting = false
        }
    }

    Column(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(TacticalSurface)
            .border(1.5.dp, TacticalBorder, RoundedCornerShape(12.dp))
            .padding(14.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        // Top Header: Title, Category Selector, and Close
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = remoteFile?.entry?.cleanDisplayName ?: "VIRTUAL REMOTE // TACTICAL",
                    color = TacticalCyan,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold,
                    fontSize = 13.sp,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                Text(
                    text = "ADAPTIVE ENGINE • ${buttons.size} SIGNALS LOADED",
                    color = TacticalAmber,
                    fontFamily = FontFamily.Monospace,
                    fontSize = 8.5.sp
                )
            }

            if (onClose != null) {
                IconButton(onClick = onClose, modifier = Modifier.size(30.dp)) {
                    Icon(Icons.Default.Close, contentDescription = "Close", tint = TextMuted)
                }
            }
        }

        // Category Mode Tabs (Auto-selected, with manual override option)
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(6.dp))
                .background(TacticalSurfaceLow)
                .border(1.dp, TacticalBorder, RoundedCornerShape(6.dp))
                .padding(3.dp),
            horizontalArrangement = Arrangement.spacedBy(4.dp)
        ) {
            RemoteCategory.values().forEach { cat ->
                val isSelected = activeCategory == cat
                Box(
                    modifier = Modifier
                        .weight(1f)
                        .clip(RoundedCornerShape(4.dp))
                        .background(if (isSelected) TacticalCyan.copy(alpha = 0.2f) else Color.Transparent)
                        .border(
                            1.dp,
                            if (isSelected) TacticalCyan else Color.Transparent,
                            RoundedCornerShape(4.dp)
                        )
                        .clickable {
                            view.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                            activeCategory = cat
                        }
                        .padding(vertical = 5.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(4.dp)
                    ) {
                        Icon(
                            imageVector = cat.icon,
                            contentDescription = cat.displayName,
                            tint = if (isSelected) TacticalCyan else TextMuted,
                            modifier = Modifier.size(13.dp)
                        )
                        Text(
                            text = cat.displayName.split(" ")[0],
                            color = if (isSelected) TacticalCyan else TextMuted,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold,
                            fontSize = 9.sp
                        )
                    }
                }
            }
        }

        // Top Physical IR Emitter Bezel & Blasting Status
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(6.dp))
                .background(TacticalSurfaceVariant)
                .border(1.dp, if (isBlasting) TacticalCyan else TacticalBorder, RoundedCornerShape(6.dp))
                .padding(horizontal = 10.dp, vertical = 6.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                // Glowing IR LED Diode
                Box(
                    modifier = Modifier
                        .size(10.dp)
                        .clip(CircleShape)
                        .background(if (isBlasting) Color(0xFF00FFFF) else Color(0xFF1E2638))
                        .border(1.dp, if (isBlasting) Color.White else TacticalBorder, CircleShape)
                )
                Text(
                    text = if (isBlasting) "BLASTING..." else "IR EMITTER READY",
                    color = if (isBlasting) TacticalCyan else TextMuted,
                    fontSize = 8.5.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold
                )
            }

            Text(
                text = blastFeedbackText ?: "TOUCH BUTTON TO BLAST",
                color = if (blastFeedbackText != null) TacticalAmber else TextSecondary,
                fontSize = 8.5.sp,
                fontFamily = FontFamily.Monospace,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        }

        // Remote Body Canvas based on Category
        when (activeCategory) {
            RemoteCategory.TV -> VirtualTvRemoteLayout(
                buttons = buttons,
                onTrigger = { btn, label -> triggerBlast(btn, label) }
            )
            RemoteCategory.AC -> VirtualAcRemoteLayout(
                buttons = buttons,
                temp = acTemp,
                power = acPower,
                mode = acMode,
                fan = acFan,
                swing = acSwing,
                onTempChange = { acTemp = it },
                onPowerToggle = { acPower = !acPower },
                onModeCycle = {
                    acMode = when (acMode) {
                        "COOL" -> "HEAT"
                        "HEAT" -> "DRY"
                        "DRY" -> "FAN"
                        "FAN" -> "AUTO"
                        else -> "COOL"
                    }
                },
                onFanCycle = {
                    acFan = when (acFan) {
                        "AUTO" -> "LOW"
                        "LOW" -> "MED"
                        "MED" -> "HIGH"
                        "HIGH" -> "TURBO"
                        else -> "AUTO"
                    }
                },
                onSwingToggle = { acSwing = !acSwing },
                onTrigger = { btn, label -> triggerBlast(btn, label) }
            )
            RemoteCategory.RGB_LED -> VirtualRgbLedRemoteLayout(
                buttons = buttons,
                activeColor = ledActiveColor,
                brightness = ledBrightness,
                onColorSelected = { ledActiveColor = it },
                onBrightnessChange = { ledBrightness = it },
                onTrigger = { btn, label -> triggerBlast(btn, label) }
            )
        }

        // Flash to Q-Watch Action Button
        if (onFlashToWatch != null && remoteFile != null) {
            Button(
                onClick = onFlashToWatch,
                colors = ButtonDefaults.buttonColors(containerColor = TacticalCyan),
                shape = RoundedCornerShape(6.dp),
                modifier = Modifier.fillMaxWidth().height(42.dp)
            ) {
                Icon(Icons.Default.Bluetooth, contentDescription = null, tint = OledBlack, modifier = Modifier.size(16.dp))
                Spacer(modifier = Modifier.width(6.dp))
                Text(
                    text = "FLASH REMOTE TO Q-WATCH (/ir/)",
                    color = OledBlack,
                    fontSize = 11.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold
                )
            }
        }
    }
}

// -------------------------------------------------------------
// 1. REAL TV REMOTE LAYOUT
// -------------------------------------------------------------
@Composable
private fun VirtualTvRemoteLayout(
    buttons: List<IrParsedButton>,
    onTrigger: (IrParsedButton?, String) -> Unit
) {
    var showNumpad by remember { mutableStateOf(false) }

    Column(
        modifier = Modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        // Row 1: Power, Mute, Source/Input
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            val powerBtn = findSignal(buttons, "power", "pwr", "power_on", "power_off", "on", "off")
            RemoteButton(
                label = "POWER",
                icon = Icons.Default.PowerSettingsNew,
                accentColor = TacticalRed,
                isMapped = powerBtn != null,
                modifier = Modifier.weight(1f).height(46.dp),
                onClick = { onTrigger(powerBtn, "POWER") }
            )

            Spacer(modifier = Modifier.width(8.dp))

            val muteBtn = findSignal(buttons, "mute", "muting", "vol_mute")
            RemoteButton(
                label = "MUTE",
                icon = Icons.Default.VolumeMute,
                accentColor = TacticalAmber,
                isMapped = muteBtn != null,
                modifier = Modifier.weight(1f).height(46.dp),
                onClick = { onTrigger(muteBtn, "MUTE") }
            )

            Spacer(modifier = Modifier.width(8.dp))

            val inputBtn = findSignal(buttons, "input", "source", "tv/av", "av", "hdmi")
            RemoteButton(
                label = "INPUT",
                icon = Icons.Default.Input,
                accentColor = TacticalCyan,
                isMapped = inputBtn != null,
                modifier = Modifier.weight(1f).height(46.dp),
                onClick = { onTrigger(inputBtn, "INPUT") }
            )
        }

        // Row 2: Rocker Columns (VOL +/- and CH +/-) with Center Menu Cluster
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            // VOL Column
            Column(
                modifier = Modifier.weight(1f),
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                val volUpBtn = findSignal(buttons, "vol_up", "vol+", "volume_up", "volume+", "volup")
                RemoteButton(
                    label = "VOL +",
                    icon = Icons.Default.Add,
                    isMapped = volUpBtn != null,
                    modifier = Modifier.fillMaxWidth().height(44.dp),
                    onClick = { onTrigger(volUpBtn, "VOL_UP") }
                )

                val volDownBtn = findSignal(buttons, "vol_down", "vol-", "volume_down", "volume-", "voldown")
                RemoteButton(
                    label = "VOL -",
                    icon = Icons.Default.Remove,
                    isMapped = volDownBtn != null,
                    modifier = Modifier.fillMaxWidth().height(44.dp),
                    onClick = { onTrigger(volDownBtn, "VOL_DOWN") }
                )
            }

            // Center Actions: Home, Guide, Numpad Toggle
            Column(
                modifier = Modifier.weight(1.2f),
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                val homeBtn = findSignal(buttons, "home", "menu", "smart", "settings")
                RemoteButton(
                    label = "HOME",
                    icon = Icons.Default.Home,
                    accentColor = TacticalCyan,
                    isMapped = homeBtn != null,
                    modifier = Modifier.fillMaxWidth().height(44.dp),
                    onClick = { onTrigger(homeBtn, "HOME") }
                )

                Button(
                    onClick = { showNumpad = !showNumpad },
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceLow),
                    shape = RoundedCornerShape(6.dp),
                    modifier = Modifier.fillMaxWidth().height(44.dp).border(1.dp, TacticalBorder, RoundedCornerShape(6.dp))
                ) {
                    Text(
                        text = if (showNumpad) "HIDE [0-9]" else "NUMPAD [0-9]",
                        color = TacticalAmber,
                        fontFamily = FontFamily.Monospace,
                        fontSize = 10.sp,
                        fontWeight = FontWeight.Bold
                    )
                }
            }

            // CH Column
            Column(
                modifier = Modifier.weight(1f),
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                val chUpBtn = findSignal(buttons, "ch_up", "ch+", "channel_up", "channel+", "chup")
                RemoteButton(
                    label = "CH +",
                    icon = Icons.Default.KeyboardArrowUp,
                    isMapped = chUpBtn != null,
                    modifier = Modifier.fillMaxWidth().height(44.dp),
                    onClick = { onTrigger(chUpBtn, "CH_UP") }
                )

                val chDownBtn = findSignal(buttons, "ch_down", "ch-", "channel_down", "channel-", "chdown")
                RemoteButton(
                    label = "CH -",
                    icon = Icons.Default.KeyboardArrowDown,
                    isMapped = chDownBtn != null,
                    modifier = Modifier.fillMaxWidth().height(44.dp),
                    onClick = { onTrigger(chDownBtn, "CH_DOWN") }
                )
            }
        }

        // Directional Tactical D-Pad
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(8.dp))
                .background(TacticalSurfaceLow)
                .border(1.dp, TacticalBorder, RoundedCornerShape(8.dp))
                .padding(10.dp),
            contentAlignment = Alignment.Center
        ) {
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                val upBtn = findSignal(buttons, "up", "arrow_up", "dpad_up")
                RemoteButton(
                    label = "UP",
                    icon = Icons.Default.KeyboardArrowUp,
                    isMapped = upBtn != null,
                    modifier = Modifier.width(110.dp).height(38.dp),
                    onClick = { onTrigger(upBtn, "UP") }
                )

                Row(
                    horizontalArrangement = Arrangement.spacedBy(6.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    val leftBtn = findSignal(buttons, "left", "arrow_left", "dpad_left")
                    RemoteButton(
                        label = "LEFT",
                        icon = Icons.Default.KeyboardArrowLeft,
                        isMapped = leftBtn != null,
                        modifier = Modifier.width(90.dp).height(38.dp),
                        onClick = { onTrigger(leftBtn, "LEFT") }
                    )

                    val okBtn = findSignal(buttons, "ok", "enter", "select")
                    RemoteButton(
                        label = "OK",
                        accentColor = TacticalAmber,
                        isMapped = okBtn != null,
                        modifier = Modifier.width(90.dp).height(38.dp),
                        onClick = { onTrigger(okBtn, "OK") }
                    )

                    val rightBtn = findSignal(buttons, "right", "arrow_right", "dpad_right")
                    RemoteButton(
                        label = "RIGHT",
                        icon = Icons.Default.KeyboardArrowRight,
                        isMapped = rightBtn != null,
                        modifier = Modifier.width(90.dp).height(38.dp),
                        onClick = { onTrigger(rightBtn, "RIGHT") }
                    )
                }

                val downBtn = findSignal(buttons, "down", "arrow_down", "dpad_down")
                RemoteButton(
                    label = "DOWN",
                    icon = Icons.Default.KeyboardArrowDown,
                    isMapped = downBtn != null,
                    modifier = Modifier.width(110.dp).height(38.dp),
                    onClick = { onTrigger(downBtn, "DOWN") }
                )
            }
        }

        // Navigation Footer: BACK & EXIT
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            val backBtn = findSignal(buttons, "back", "return", "previous")
            RemoteButton(
                label = "BACK",
                icon = Icons.Default.ArrowBack,
                accentColor = TacticalRed,
                isMapped = backBtn != null,
                modifier = Modifier.weight(1f).height(40.dp),
                onClick = { onTrigger(backBtn, "BACK") }
            )

            val exitBtn = findSignal(buttons, "exit", "cancel", "clear")
            RemoteButton(
                label = "EXIT",
                icon = Icons.Default.Close,
                accentColor = TextSecondary,
                isMapped = exitBtn != null,
                modifier = Modifier.weight(1f).height(40.dp),
                onClick = { onTrigger(exitBtn, "EXIT") }
            )
        }

        // Collapsible Numpad
        AnimatedVisibility(visible = showNumpad) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(6.dp))
                    .background(TacticalSurfaceLow)
                    .border(1.dp, TacticalBorder, RoundedCornerShape(6.dp))
                    .padding(8.dp),
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                val numGrid = listOf(
                    listOf("1", "2", "3"),
                    listOf("4", "5", "6"),
                    listOf("7", "8", "9"),
                    listOf("-", "0", "ENT")
                )
                for (row in numGrid) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        for (digit in row) {
                            val digitBtn = findSignal(buttons, digit, "num_$digit")
                            RemoteButton(
                                label = digit,
                                isMapped = digitBtn != null,
                                modifier = Modifier.weight(1f).height(36.dp),
                                onClick = { onTrigger(digitBtn, digit) }
                            )
                        }
                    }
                }
            }
        }

        // TV Color Keys: Red, Green, Yellow, Blue
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            val redBtn = findSignal(buttons, "red", "r")
            val greenBtn = findSignal(buttons, "green", "g")
            val yellowBtn = findSignal(buttons, "yellow", "y")
            val blueBtn = findSignal(buttons, "blue", "b")

            ColorPillButton("R", Color(0xFFE53935), redBtn != null, Modifier.weight(1f)) { onTrigger(redBtn, "RED") }
            ColorPillButton("G", Color(0xFF43A047), greenBtn != null, Modifier.weight(1f)) { onTrigger(greenBtn, "GREEN") }
            ColorPillButton("Y", Color(0xFFFDD835), yellowBtn != null, Modifier.weight(1f)) { onTrigger(yellowBtn, "YELLOW") }
            ColorPillButton("B", Color(0xFF1E88E5), blueBtn != null, Modifier.weight(1f)) { onTrigger(blueBtn, "BLUE") }
        }
    }
}

// -------------------------------------------------------------
// 2. REAL AC (AIR CONDITIONER) CLIMATE REMOTE LAYOUT
// -------------------------------------------------------------
@Composable
private fun VirtualAcRemoteLayout(
    buttons: List<IrParsedButton>,
    temp: Int,
    power: Boolean,
    mode: String,
    fan: String,
    swing: Boolean,
    onTempChange: (Int) -> Unit,
    onPowerToggle: () -> Unit,
    onModeCycle: () -> Unit,
    onFanCycle: () -> Unit,
    onSwingToggle: () -> Unit,
    onTrigger: (IrParsedButton?, String) -> Unit
) {
    Column(
        modifier = Modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        // Virtual LCD Digital Climate Display Screen
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(8.dp))
                .background(Color(0xFF071216))
                .border(1.5.dp, TacticalCyan.copy(alpha = 0.8f), RoundedCornerShape(8.dp))
                .padding(12.dp)
        ) {
            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = "AC INVERTER CLIMATE",
                        color = TacticalCyan,
                        fontSize = 9.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        Text(
                            text = if (power) "ON" else "OFF",
                            color = if (power) TacticalGreen else TacticalRed,
                            fontSize = 9.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                    }
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    // Big LCD 7-Segment Temperature
                    Text(
                        text = "$temp°C",
                        color = Color(0xFF00FFCC),
                        fontSize = 38.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold,
                        letterSpacing = 2.sp
                    )

                    // Mode & Fan telemetry
                    Column(
                        horizontalAlignment = Alignment.End,
                        verticalArrangement = Arrangement.spacedBy(2.dp)
                    ) {
                        Text(
                            text = "MODE: $mode",
                            color = TacticalAmber,
                            fontSize = 11.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                        Text(
                            text = "FAN: $fan",
                            color = TacticalCyan,
                            fontSize = 10.sp,
                            fontFamily = FontFamily.Monospace
                        )
                        Text(
                            text = "SWING: ${if (swing) "ON ↕" else "OFF"}",
                            color = if (swing) TacticalGreen else TextMuted,
                            fontSize = 9.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }
            }
        }

        // Main Controls Row: Temp +/- and Large Power
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(10.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Big Temperature Adjustment Buttons
            Column(
                modifier = Modifier.weight(1.5f),
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                val tempUpBtn = findSignal(buttons, "temp_up", "temp+", "temperature_up", "tempup", "up")
                RemoteButton(
                    label = "TEMP ▲",
                    icon = Icons.Default.KeyboardArrowUp,
                    accentColor = TacticalCyan,
                    isMapped = tempUpBtn != null,
                    modifier = Modifier.fillMaxWidth().height(48.dp),
                    onClick = {
                        onTempChange((temp + 1).coerceAtMost(32))
                        onTrigger(tempUpBtn, "TEMP_UP")
                    }
                )

                val tempDownBtn = findSignal(buttons, "temp_down", "temp-", "temperature_down", "tempdown", "down")
                RemoteButton(
                    label = "TEMP ▼",
                    icon = Icons.Default.KeyboardArrowDown,
                    accentColor = TacticalCyan,
                    isMapped = tempDownBtn != null,
                    modifier = Modifier.fillMaxWidth().height(48.dp),
                    onClick = {
                        onTempChange((temp - 1).coerceAtLeast(16))
                        onTrigger(tempDownBtn, "TEMP_DOWN")
                    }
                )
            }

            // Power ON/OFF Main Key
            val powerBtn = findSignal(buttons, "power", "on", "off", "power_on", "power_off")
            RemoteButton(
                label = "POWER\nON / OFF",
                icon = Icons.Default.PowerSettingsNew,
                accentColor = if (power) TacticalAmber else TacticalRed,
                isMapped = powerBtn != null,
                modifier = Modifier.weight(1f).height(102.dp),
                onClick = {
                    onPowerToggle()
                    onTrigger(powerBtn, "POWER")
                }
            )
        }

        // Functions Row: MODE, FAN, SWING, TURBO
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            val modeBtn = findSignal(buttons, "mode", "cool", "heat", "ac_mode")
            RemoteButton(
                label = "MODE",
                accentColor = TacticalAmber,
                isMapped = modeBtn != null,
                modifier = Modifier.weight(1f).height(42.dp),
                onClick = {
                    onModeCycle()
                    onTrigger(modeBtn, "MODE")
                }
            )

            val fanBtn = findSignal(buttons, "fan", "fan_speed", "speed")
            RemoteButton(
                label = "FAN",
                accentColor = TacticalCyan,
                isMapped = fanBtn != null,
                modifier = Modifier.weight(1f).height(42.dp),
                onClick = {
                    onFanCycle()
                    onTrigger(fanBtn, "FAN")
                }
            )

            val swingBtn = findSignal(buttons, "swing", "vane", "v_swing", "h_swing", "air_dir")
            RemoteButton(
                label = "SWING",
                accentColor = TacticalGreen,
                isMapped = swingBtn != null,
                modifier = Modifier.weight(1f).height(42.dp),
                onClick = {
                    onSwingToggle()
                    onTrigger(swingBtn, "SWING")
                }
            )

            val turboBtn = findSignal(buttons, "turbo", "boost", "super")
            RemoteButton(
                label = "TURBO",
                accentColor = TacticalRed,
                isMapped = turboBtn != null,
                modifier = Modifier.weight(1f).height(42.dp),
                onClick = { onTrigger(turboBtn, "TURBO") }
            )
        }

        // Bottom Auxiliary: SLEEP, ECO, TIMER, CLEAN
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            val sleepBtn = findSignal(buttons, "sleep")
            val ecoBtn = findSignal(buttons, "eco", "save", "energy")
            val timerBtn = findSignal(buttons, "timer", "timer_on", "timer_off")
            val cleanBtn = findSignal(buttons, "clean", "ion", "health", "light")

            RemoteButton(label = "SLEEP", isMapped = sleepBtn != null, modifier = Modifier.weight(1f).height(38.dp)) { onTrigger(sleepBtn, "SLEEP") }
            RemoteButton(label = "ECO", isMapped = ecoBtn != null, modifier = Modifier.weight(1f).height(38.dp)) { onTrigger(ecoBtn, "ECO") }
            RemoteButton(label = "TIMER", isMapped = timerBtn != null, modifier = Modifier.weight(1f).height(38.dp)) { onTrigger(timerBtn, "TIMER") }
            RemoteButton(label = "CLEAN", isMapped = cleanBtn != null, modifier = Modifier.weight(1f).height(38.dp)) { onTrigger(cleanBtn, "CLEAN") }
        }
    }
}

// -------------------------------------------------------------
// 3. REAL RGB LED LIGHT REMOTE LAYOUT
// -------------------------------------------------------------
@Composable
private fun VirtualRgbLedRemoteLayout(
    buttons: List<IrParsedButton>,
    activeColor: Color,
    brightness: Int,
    onColorSelected: (Color) -> Unit,
    onBrightnessChange: (Int) -> Unit,
    onTrigger: (IrParsedButton?, String) -> Unit
) {
    Column(
        modifier = Modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        // Top Row: BRIGHT+, BRIGHT-, OFF, ON
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            val brightUpBtn = findSignal(buttons, "bright_up", "bright+", "brightness_up", "dimmer_up")
            RemoteButton(
                label = "BRIGHT +",
                icon = Icons.Default.BrightnessHigh,
                accentColor = TacticalAmber,
                isMapped = brightUpBtn != null,
                modifier = Modifier.weight(1f).height(44.dp),
                onClick = {
                    onBrightnessChange((brightness + 10).coerceAtMost(100))
                    onTrigger(brightUpBtn, "BRIGHT_UP")
                }
            )

            val brightDownBtn = findSignal(buttons, "bright_down", "bright-", "brightness_down", "dimmer_down")
            RemoteButton(
                label = "BRIGHT -",
                icon = Icons.Default.BrightnessLow,
                accentColor = TacticalAmber,
                isMapped = brightDownBtn != null,
                modifier = Modifier.weight(1f).height(44.dp),
                onClick = {
                    onBrightnessChange((brightness - 10).coerceAtLeast(10))
                    onTrigger(brightDownBtn, "BRIGHT_DOWN")
                }
            )

            val offBtn = findSignal(buttons, "off", "power_off")
            RemoteButton(
                label = "OFF",
                accentColor = TacticalRed,
                isMapped = offBtn != null,
                modifier = Modifier.weight(1f).height(44.dp),
                onClick = { onTrigger(offBtn, "OFF") }
            )

            val onBtn = findSignal(buttons, "on", "power_on", "power")
            RemoteButton(
                label = "ON",
                accentColor = TacticalGreen,
                isMapped = onBtn != null,
                modifier = Modifier.weight(1f).height(44.dp),
                onClick = { onTrigger(onBtn, "ON") }
            )
        }

        // Live Color Aura Bar
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(18.dp)
                .clip(RoundedCornerShape(4.dp))
                .background(activeColor)
                .border(1.dp, Color.White.copy(alpha = 0.5f), RoundedCornerShape(4.dp)),
            contentAlignment = Alignment.Center
        ) {
            Text(
                text = "ACTIVE HUE // BRI: $brightness%",
                color = Color.Black,
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.Bold,
                fontSize = 8.5.sp
            )
        }

        // 20-Key Membrane Color Matrix (4 columns x 5 rows)
        val colorGrid = listOf(
            // Row 1: Primary Colors + White
            listOf(
                ColorKeyDef("R", Color(0xFFFF1744), listOf("red", "r")),
                ColorKeyDef("G", Color(0xFF00E676), listOf("green", "g")),
                ColorKeyDef("B", Color(0xFF2979FF), listOf("blue", "b")),
                ColorKeyDef("W", Color(0xFFFFFFFF), listOf("white", "w"))
            ),
            // Row 2: Orange, Light Green, Deep Blue, FLASH
            listOf(
                ColorKeyDef("R1", Color(0xFFFF6D00), listOf("r1", "orange")),
                ColorKeyDef("G1", Color(0xFF76FF03), listOf("g1", "light_green")),
                ColorKeyDef("B1", Color(0xFF304FFE), listOf("b1", "deep_blue")),
                ColorKeyDef("FLASH", Color(0xFFE040FB), listOf("flash", "jump3"), isEffect = true)
            ),
            // Row 3: Amber, Cyan, Purple, STROBE
            listOf(
                ColorKeyDef("R2", Color(0xFFFFAB00), listOf("r2", "amber")),
                ColorKeyDef("G2", Color(0xFF00E5FF), listOf("g2", "cyan")),
                ColorKeyDef("B2", Color(0xFF7C4DFF), listOf("b2", "purple")),
                ColorKeyDef("STROBE", Color(0xFFFF4081), listOf("strobe", "jump7"), isEffect = true)
            ),
            // Row 4: Yellow, Teal, Magenta, FADE
            listOf(
                ColorKeyDef("R3", Color(0xFFFFD600), listOf("r3", "yellow")),
                ColorKeyDef("G3", Color(0xFF1DE9B6), listOf("g3", "teal")),
                ColorKeyDef("B3", Color(0xFFD500F9), listOf("b3", "magenta")),
                ColorKeyDef("FADE", Color(0xFF651FFF), listOf("fade", "fade3"), isEffect = true)
            ),
            // Row 5: Warm White, Sky Blue, Pink, SMOOTH
            listOf(
                ColorKeyDef("R4", Color(0xFFFFF9C4), listOf("r4", "warm_white")),
                ColorKeyDef("G4", Color(0xFF80D8FF), listOf("g4", "sky_blue")),
                ColorKeyDef("B4", Color(0xFFFF80AB), listOf("b4", "pink")),
                ColorKeyDef("SMOOTH", Color(0xFF00B0FF), listOf("smooth", "fade7"), isEffect = true)
            )
        )

        Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            for (row in colorGrid) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    for (key in row) {
                        val matchedBtn = findSignal(buttons, *key.aliases.toTypedArray())
                        MembraneColorButton(
                            def = key,
                            isMapped = matchedBtn != null,
                            modifier = Modifier.weight(1f).height(38.dp),
                            onClick = {
                                if (!key.isEffect) onColorSelected(key.color)
                                onTrigger(matchedBtn, key.name)
                            }
                        )
                    }
                }
            }
        }
    }
}

private data class ColorKeyDef(
    val name: String,
    val color: Color,
    val aliases: List<String>,
    val isEffect: Boolean = false
)

@Composable
private fun MembraneColorButton(
    def: ColorKeyDef,
    isMapped: Boolean,
    modifier: Modifier = Modifier,
    onClick: () -> Unit
) {
    val interactionSource = remember { MutableInteractionSource() }
    val isPressed by interactionSource.collectIsPressedAsState()

    Box(
        modifier = modifier
            .clip(RoundedCornerShape(6.dp))
            .background(if (def.isEffect) TacticalSurfaceVariant else def.color)
            .border(
                1.5.dp,
                if (isPressed) Color.White else if (isMapped) def.color.copy(alpha = 0.8f) else TacticalBorder,
                RoundedCornerShape(6.dp)
            )
            .clickable(interactionSource = interactionSource, indication = null, onClick = onClick),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = def.name,
            color = if (def.isEffect) def.color else if (def.color == Color(0xFFFFFFFF) || def.color == Color(0xFFFFF9C4)) Color.Black else Color.White,
            fontFamily = FontFamily.Monospace,
            fontWeight = FontWeight.Bold,
            fontSize = if (def.isEffect) 8.5.sp else 10.sp
        )
    }
}

@Composable
private fun ColorPillButton(
    label: String,
    color: Color,
    isMapped: Boolean,
    modifier: Modifier = Modifier,
    onClick: () -> Unit
) {
    Box(
        modifier = modifier
            .height(28.dp)
            .clip(RoundedCornerShape(4.dp))
            .background(color.copy(alpha = if (isMapped) 0.85f else 0.35f))
            .border(1.dp, if (isMapped) color else TacticalBorder, RoundedCornerShape(4.dp))
            .clickable(onClick = onClick),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = label,
            color = Color.White,
            fontFamily = FontFamily.Monospace,
            fontWeight = FontWeight.Bold,
            fontSize = 10.sp
        )
    }
}

@Composable
private fun RemoteButton(
    label: String,
    modifier: Modifier = Modifier,
    icon: ImageVector? = null,
    accentColor: Color = TacticalCyan,
    isMapped: Boolean = true,
    onClick: () -> Unit
) {
    val interactionSource = remember { MutableInteractionSource() }
    val isPressed by interactionSource.collectIsPressedAsState()

    val bg = if (isPressed) {
        accentColor.copy(alpha = 0.35f)
    } else if (isMapped) {
        TacticalSurfaceVariant
    } else {
        TacticalSurfaceLow
    }

    val border = if (isPressed) {
        accentColor
    } else if (isMapped) {
        accentColor.copy(alpha = 0.5f)
    } else {
        TacticalBorder
    }

    val textColor = if (isPressed) {
        accentColor
    } else if (isMapped) {
        TextPrimary
    } else {
        TextMuted
    }

    Box(
        modifier = modifier
            .clip(RoundedCornerShape(6.dp))
            .background(bg)
            .border(1.dp, border, RoundedCornerShape(6.dp))
            .clickable(interactionSource = interactionSource, indication = null, onClick = onClick),
        contentAlignment = Alignment.Center
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(4.dp),
            modifier = Modifier.padding(horizontal = 4.dp)
        ) {
            if (icon != null) {
                Icon(
                    imageVector = icon,
                    contentDescription = label,
                    tint = if (isPressed) accentColor else if (isMapped) accentColor else TextMuted,
                    modifier = Modifier.size(16.dp)
                )
            }
            Text(
                text = label,
                color = textColor,
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.Bold,
                fontSize = 10.sp,
                textAlign = TextAlign.Center,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis
            )
        }
    }
}
