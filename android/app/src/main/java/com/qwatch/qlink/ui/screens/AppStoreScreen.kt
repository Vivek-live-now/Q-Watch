package com.qwatch.qlink.ui.screens

import android.widget.Toast
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
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
import com.qwatch.qlink.model.QAppMeta
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.ui.components.TacticalCard
import com.qwatch.qlink.ui.theme.*
import kotlinx.coroutines.launch

@Composable
fun AppStoreScreen() {
    val client = QLinkClient.instance
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    var installingAppId by remember { mutableStateOf<String?>(null) }

    val curatedApps = listOf(
        QAppMeta(
            id = "tilt_ball",
            title = "Tilt Ball Maze",
            version = "1.1",
            author = "Q-Watch Team",
            description = "Physics accelerometer rolling ball puzzle using MPU-6500 tilt steering.",
            category = "GAMES",
            filename = "tilt_ball.qapp",
            sizeBytes = 3480,
            iconSymbol = "🕹️"
        ),
        QAppMeta(
            id = "compass_hud",
            title = "Compass HUD",
            version = "1.0",
            author = "Q-Watch Team",
            description = "Tactical aviation heading tape with QMC5883P true-north tracking.",
            category = "UTILITY",
            filename = "compass_hud.qapp",
            sizeBytes = 4120,
            iconSymbol = "🧭"
        ),
        QAppMeta(
            id = "space_arcade",
            title = "007 Retro Invaders",
            version = "1.0",
            author = "MI6 Cyber",
            description = "Micro-ELF arcade space shooter with button & gyro steering, audio FX, and high scores.",
            category = "ARCADE",
            filename = "invaders.qapp",
            sizeBytes = 5620,
            iconSymbol = "🚀"
        ),
        QAppMeta(
            id = "dice_roller",
            title = "Tactical Dice & RNG",
            version = "1.0",
            author = "Community",
            description = "Hardware TRNG cryptographic dice roller and coin flipper.",
            category = "TOOLS",
            filename = "dice.qapp",
            sizeBytes = 2240,
            iconSymbol = "🎲"
        )
    )

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(TacticalBackground)
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        TacticalCard(title = "MICRO-ELF APP STORE", accentColor = TacticalAmber) {
            Text(
                text = "Browse relocatable .qapp binaries. Apps execute directly in IRAM/PSRAM with hardware sandboxing.",
                color = TextSecondary,
                fontSize = 11.sp,
                fontFamily = FontFamily.Monospace
            )
        }

        LazyColumn(
            modifier = Modifier.fillMaxSize(),
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            items(curatedApps) { app ->
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(8.dp))
                        .background(TacticalSurface)
                        .border(1.dp, TacticalBorder, RoundedCornerShape(8.dp))
                        .padding(12.dp),
                    verticalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            Text(text = app.iconSymbol, fontSize = 20.sp)
                            Column {
                                Text(
                                    text = app.title,
                                    color = TextPrimary,
                                    fontFamily = FontFamily.Monospace,
                                    fontWeight = FontWeight.Bold,
                                    fontSize = 14.sp
                                )
                                Text(
                                    text = "v${app.version} by ${app.author} | ${app.sizeBytes} B",
                                    color = TextMuted,
                                    fontFamily = FontFamily.Monospace,
                                    fontSize = 10.sp
                                )
                            }
                        }

                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(4.dp))
                                .background(TacticalAmberDim)
                                .padding(horizontal = 6.dp, vertical = 2.dp)
                        ) {
                            Text(
                                text = app.category,
                                color = TacticalAmber,
                                fontSize = 9.sp,
                                fontFamily = FontFamily.Monospace
                            )
                        }
                    }

                    Text(
                        text = app.description,
                        color = TextSecondary,
                        fontSize = 11.sp,
                        fontFamily = FontFamily.Monospace
                    )

                    Button(
                        onClick = {
                            scope.launch {
                                installingAppId = app.id
                                // In production, this downloads the binary from repo/assets and calls installQApp
                                val mockBinary = ByteArray(app.sizeBytes.toInt()) { 0x00 }
                                val res = client.installQApp(app.filename, mockBinary)
                                if (res.isSuccess) {
                                    Toast.makeText(context, "Successfully installed ${app.title} to /apps/${app.filename}", Toast.LENGTH_SHORT).show()
                                } else {
                                    Toast.makeText(context, "Installation failed", Toast.LENGTH_SHORT).show()
                                }
                                installingAppId = null
                            }
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalCyan),
                        shape = RoundedCornerShape(6.dp),
                        modifier = Modifier.fillMaxWidth().height(36.dp),
                        enabled = (installingAppId != app.id)
                    ) {
                        if (installingAppId == app.id) {
                            CircularProgressIndicator(color = OledBlack, modifier = Modifier.size(16.dp), strokeWidth = 2.dp)
                        } else {
                            Text(
                                text = "INSTALL TO WATCH",
                                color = OledBlack,
                                fontSize = 11.sp,
                                fontFamily = FontFamily.Monospace,
                                fontWeight = FontWeight.Bold
                            )
                        }
                    }
                }
            }
        }
    }
}
