package com.qwatch.qlink.ui.screens

import android.widget.Toast
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.ui.components.TacticalDPad
import com.qwatch.qlink.ui.components.VirtualOledDisplay
import com.qwatch.qlink.ui.components.exportFrameToBitmap
import com.qwatch.qlink.ui.theme.*
import kotlinx.coroutines.launch

@Composable
fun LiveMirrorScreen() {
    val client = QLinkClient.instance
    val frame by client.displayFrame.collectAsState()
    val scope = rememberCoroutineScope()
    val context = LocalContext.current

    var isStreaming by remember { mutableStateOf(false) }
    var selectedPhosphor by remember { mutableStateOf(PhosphorCyan) }
    var fpsTarget by remember { mutableIntStateOf(20) }

    val phosphorOptions = listOf(
        "CYAN" to PhosphorCyan,
        "AMBER" to PhosphorAmber,
        "GREEN" to PhosphorGreen,
        "WHITE" to PhosphorWhite
    )

    val scrollState = rememberScrollState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(TacticalBackground)
            .padding(16.dp)
            .verticalScroll(scrollState),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // 1. Top Controls Bar: Stream Toggle & Screenshot
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    text = "STREAM:",
                    color = TextMuted,
                    fontSize = 11.sp,
                    fontFamily = FontFamily.Monospace,
                    modifier = Modifier.padding(end = 8.dp)
                )
                Switch(
                    checked = isStreaming,
                    onCheckedChange = { enable ->
                        isStreaming = enable
                        scope.launch {
                            client.setDisplayStreaming(enable, fpsTarget)
                        }
                    },
                    colors = SwitchDefaults.colors(
                        checkedThumbColor = TacticalCyan,
                        checkedTrackColor = TacticalCyanDim,
                        uncheckedThumbColor = TextMuted,
                        uncheckedTrackColor = TacticalSurface
                    )
                )
            }

            Button(
                onClick = {
                    val bmp = exportFrameToBitmap(frame.buffer)
                    Toast.makeText(context, "Captured 128x64 Frame #${frame.frameNumber}", Toast.LENGTH_SHORT).show()
                },
                colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                shape = RoundedCornerShape(6.dp)
            ) {
                Text(
                    text = "SCREENSHOT",
                    color = TacticalCyan,
                    fontSize = 10.sp,
                    fontFamily = FontFamily.Monospace
                )
            }
        }

        // 2. Virtual OLED Display Simulation (128x64 Canvas)
        VirtualOledDisplay(
            frame = frame,
            isStreaming = isStreaming,
            phosphorColor = selectedPhosphor,
            modifier = Modifier.fillMaxWidth()
        )

        // Frame Metrics & Phosphor Selector
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "FRAME #${frame.frameNumber} | $fpsTarget FPS",
                color = TextSecondary,
                fontSize = 10.sp,
                fontFamily = FontFamily.Monospace
            )

            // Phosphor Pills
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                phosphorOptions.forEach { (name, color) ->
                    Box(
                        modifier = Modifier
                            .size(18.dp)
                            .clip(RoundedCornerShape(4.dp))
                            .background(color)
                            .border(
                                width = if (selectedPhosphor == color) 2.dp else 0.dp,
                                color = if (selectedPhosphor == color) TextPrimary else Color.Transparent,
                                shape = RoundedCornerShape(4.dp)
                            )
                            .clickable { selectedPhosphor = color }
                    )
                }
            }
        }

        Divider(color = TacticalBorder, thickness = 1.dp)

        // 3. Tactical Virtual Remote (D-Pad)
        Text(
            text = "[ VIRTUAL CONTROLLER ]",
            color = TacticalAmber,
            fontSize = 11.sp,
            fontFamily = FontFamily.Monospace,
            fontWeight = FontWeight.Bold
        )

        TacticalDPad(
            onButtonAction = { type, event ->
                scope.launch {
                    client.injectButton(type, event)
                }
            }
        )
    }
}
