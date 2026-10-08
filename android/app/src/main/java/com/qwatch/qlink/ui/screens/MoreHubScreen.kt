package com.qwatch.qlink.ui.screens

import android.view.HapticFeedbackConstants
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.ui.components.TacticalCard
import com.qwatch.qlink.ui.theme.*

@Composable
fun MoreHubScreen(
    onNavigateToSigint: () -> Unit,
    onNavigateToIrdb: () -> Unit,
    onNavigateToMochi: () -> Unit,
    onNavigateToAnimMarket: () -> Unit,
    onNavigateToApps: () -> Unit,
    onNavigateToFiles: () -> Unit,
    onNavigateToNotifications: () -> Unit,
    onNavigateToSettings: () -> Unit,
    onBack: (() -> Unit)? = null
) {
    val scrollState = rememberScrollState()
    val view = LocalView.current

    fun performHaptic() {
        view.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(TacticalBackground)
            .padding(16.dp)
            .verticalScroll(scrollState),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // 0. Header with Back Button (if provided) & Title
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column {
                Text(
                    text = "OPERATIONAL HUB // MORE",
                    color = TacticalCyan,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold,
                    fontSize = 15.sp,
                    letterSpacing = 1.sp
                )
                Text(
                    text = "ADVANCED TACTICAL MODULES & SUBSYSTEMS",
                    color = TextMuted,
                    fontFamily = FontFamily.Monospace,
                    fontSize = 9.sp,
                    letterSpacing = 0.5.sp
                )
            }

            if (onBack != null) {
                IconButton(
                    onClick = {
                        performHaptic()
                        onBack()
                    },
                    modifier = Modifier
                        .size(36.dp)
                        .clip(RoundedCornerShape(6.dp))
                        .background(TacticalSurfaceVariant)
                        .border(1.dp, TacticalBorder, RoundedCornerShape(6.dp))
                ) {
                    Icon(
                        imageVector = Icons.Default.ArrowBack,
                        contentDescription = "Back",
                        tint = TacticalCyan,
                        modifier = Modifier.size(18.dp)
                    )
                }
            }
        }

        // 1. RF & Electronic Warfare
        TacticalCard(title = "RF & ELECTRONIC WARFARE", accentColor = TacticalGreen) {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                MoreModuleCard(
                    title = "TACTICAL SIGINT // RF WARFARE",
                    subtitle = "360° Rotating Radar, 13-Ch 2.4GHz Waterfall & IDS Sentry",
                    badge = "RADAR / IDS",
                    icon = Icons.Default.Radar,
                    accentColor = TacticalGreen,
                    onClick = {
                        performHaptic()
                        onNavigateToSigint()
                    }
                )
            }
        }

        // 2. Infrared & Remote Control
        TacticalCard(title = "INFRARED & REMOTE CONTROL", accentColor = TacticalCyan) {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                MoreModuleCard(
                    title = "ONLINE IRDB // FLIPPER REPOSITORY",
                    subtitle = "2,700+ Remotes (TV, AC, Audio) • Cloud Catalog & BLE Flashing",
                    badge = "CLOUD IR",
                    icon = Icons.Default.Sensors,
                    accentColor = TacticalCyan,
                    onClick = {
                        performHaptic()
                        onNavigateToIrdb()
                    }
                )
            }
        }

        // 3. Virtual Companions & Media
        TacticalCard(title = "VIRTUAL COMPANIONS & MEDIA", accentColor = TacticalAmber) {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                MoreModuleCard(
                    title = "DASAI MOCHI // CYBER-PET",
                    subtitle = "128x64 OLED Cyber-Pet, 17 Vector Emotes & Care Deck",
                    badge = "EMULATOR",
                    icon = Icons.Default.Face,
                    accentColor = TacticalAmber,
                    onClick = {
                        performHaptic()
                        onNavigateToMochi()
                    }
                )

                MoreModuleCard(
                    title = "ANIMATION MARKET // 128x64 OLED",
                    subtitle = "38 Offline Animations, Live Canvas Studio & Sideload",
                    badge = "38 ASSETS",
                    icon = Icons.Default.Storefront,
                    accentColor = TacticalAmber,
                    onClick = {
                        performHaptic()
                        onNavigateToAnimMarket()
                    }
                )
            }
        }

        // 4. Apps & Flash Storage
        TacticalCard(title = "APPLICATION ECOSYSTEM & STORAGE", accentColor = TacticalCyan) {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                MoreModuleCard(
                    title = "Q-APP STORE // BINARY ECOSYSTEM",
                    subtitle = "Curated App Catalog (Space Invaders, Dice) • Wireless ELF Sideload",
                    badge = "Q-APPS",
                    icon = Icons.Default.Apps,
                    accentColor = TacticalCyan,
                    onClick = {
                        performHaptic()
                        onNavigateToApps()
                    }
                )

                MoreModuleCard(
                    title = "FLASH STORAGE // LITTLEFS EXPLORER",
                    subtitle = "File System Browser (/apps, /ir, /anims, /sounds) & Uploads",
                    badge = "STORAGE",
                    icon = Icons.Default.Folder,
                    accentColor = TacticalCyan,
                    onClick = {
                        performHaptic()
                        onNavigateToFiles()
                    }
                )
            }
        }

        // 5. System Administration & Diagnostics
        TacticalCard(title = "SYSTEM ADMINISTRATION & DIAGNOSTICS", accentColor = TacticalRed) {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                MoreModuleCard(
                    title = "TACTICAL ALERTS // SENTRY LOGS",
                    subtitle = "IDS Security Events, BLE Beacons & Telemetry Push History",
                    badge = "SECURITY",
                    icon = Icons.Default.Notifications,
                    accentColor = TacticalRed,
                    onClick = {
                        performHaptic()
                        onNavigateToNotifications()
                    }
                )

                MoreModuleCard(
                    title = "DEVICE SETTINGS // HARDWARE CONFIG",
                    subtitle = "BLE Auto-Connect, Device Pairing, Wi-Fi Host & RF TX Power",
                    badge = "CONFIG",
                    icon = Icons.Default.Settings,
                    accentColor = TextSecondary,
                    onClick = {
                        performHaptic()
                        onNavigateToSettings()
                    }
                )
            }
        }
    }
}

@Composable
private fun MoreModuleCard(
    title: String,
    subtitle: String,
    badge: String,
    icon: ImageVector,
    accentColor: Color,
    onClick: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(6.dp))
            .background(TacticalSurfaceVariant)
            .border(1.dp, TacticalBorder, RoundedCornerShape(6.dp))
            .clickable(onClick = onClick)
            .padding(12.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Row(
            modifier = Modifier.weight(1f),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            Box(
                modifier = Modifier
                    .size(36.dp)
                    .clip(RoundedCornerShape(6.dp))
                    .background(TacticalSurfaceLow)
                    .border(1.dp, accentColor.copy(alpha = 0.5f), RoundedCornerShape(6.dp)),
                contentAlignment = Alignment.Center
            ) {
                Icon(
                    imageVector = icon,
                    contentDescription = title,
                    tint = accentColor,
                    modifier = Modifier.size(20.dp)
                )
            }

            Column(modifier = Modifier.weight(1f)) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    Text(
                        text = title,
                        color = accentColor,
                        fontSize = 11.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(2.dp))
                            .background(accentColor.copy(alpha = 0.15f))
                            .padding(horizontal = 4.dp, vertical = 1.dp)
                    ) {
                        Text(
                            text = badge,
                            color = accentColor,
                            fontSize = 7.5.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                    }
                }
                Spacer(modifier = Modifier.height(2.dp))
                Text(
                    text = subtitle,
                    color = TextMuted,
                    fontSize = 8.5.sp,
                    fontFamily = FontFamily.Monospace
                )
            }
        }

        Icon(
            imageVector = Icons.Default.ChevronRight,
            contentDescription = "Open",
            tint = accentColor,
            modifier = Modifier
                .padding(start = 8.dp)
                .size(18.dp)
        )
    }
}
