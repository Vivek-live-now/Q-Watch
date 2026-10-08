package com.qwatch.qlink

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.qwatch.qlink.model.ConnectionState
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.protocol.QLinkConstants
import com.qwatch.qlink.ui.screens.*
import com.qwatch.qlink.ui.theme.*
import kotlinx.coroutines.launch

enum class Screen(val title: String, val icon: ImageVector, val inBottomBar: Boolean = true) {
    DASHBOARD("HOME", Icons.Default.Dashboard, true),
    MIRROR("MIRROR", Icons.Default.Tv, true),
    SENSORS("SENSORS", Icons.Default.Speed, true),
    POWER("POWER", Icons.Default.BatteryChargingFull, true),
    MORE("MORE", Icons.Default.MoreHoriz, true),
    MOCHI("MOCHI", Icons.Default.Face, false),
    SIGINT("SIGINT", Icons.Default.Radar, false),
    IRDB("IRDB", Icons.Default.Sensors, false),
    APPS("APPS", Icons.Default.Apps, false),
    ANIM_MARKET("MARKET", Icons.Default.Storefront, false),
    FILES("FILES", Icons.Default.Folder, false),
    NOTIFICATIONS("ALERTS", Icons.Default.Notifications, false),
    SETTINGS("CONFIG", Icons.Default.Settings, false)
}

class MainActivity : ComponentActivity() {

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { _ -> }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requestRequiredPermissions()

        val prefs = getSharedPreferences("qlink_prefs", Context.MODE_PRIVATE)
        val savedHost = prefs.getString("wifi_host", QLinkConstants.DEFAULT_HOTSPOT_IP) ?: QLinkConstants.DEFAULT_HOTSPOT_IP

        // Auto-connect to saved Q-Watch target (BLE priority if Bluetooth is on, Wi-Fi fallback)
        lifecycleScope.launch {
            val client = QLinkClient.instance
            if (client.isBluetoothOn() && client.isBleAutoConnectEnabled()) {
                val ok = client.autoConnectBleIfEnabled()
                if (!ok) {
                    client.connectWifi(savedHost)
                }
            } else {
                client.connectWifi(savedHost)
            }
        }

        setContent {
            QLinkTheme {
                MainAppScaffold()
            }
        }
    }

    private fun requestRequiredPermissions() {
        val permissions = mutableListOf(
            Manifest.permission.ACCESS_FINE_LOCATION,
            Manifest.permission.ACCESS_COARSE_LOCATION
        )
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissions.add(Manifest.permission.BLUETOOTH_SCAN)
            permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            permissions.add(Manifest.permission.POST_NOTIFICATIONS)
        }

        val missing = permissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        if (missing.isNotEmpty()) {
            permissionLauncher.launch(missing.toTypedArray())
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainAppScaffold() {
    val backStack = remember { mutableStateListOf(Screen.DASHBOARD) }
    val currentScreen = backStack.lastOrNull() ?: Screen.DASHBOARD
    val client = QLinkClient.instance
    val connectionState by client.connectionState.collectAsState()

    fun navigateTo(screen: Screen) {
        if (screen == Screen.DASHBOARD) {
            backStack.clear()
            backStack.add(Screen.DASHBOARD)
        } else if (screen.inBottomBar) {
            backStack.clear()
            backStack.add(Screen.DASHBOARD)
            backStack.add(screen)
        } else {
            if (backStack.lastOrNull() != screen) {
                backStack.add(screen)
            }
        }
    }

    fun navigateBack() {
        if (backStack.size > 1) {
            backStack.removeAt(backStack.size - 1)
        }
    }

    // Intercept back button only when not on DASHBOARD
    BackHandler(enabled = backStack.size > 1 && currentScreen != Screen.DASHBOARD) {
        navigateBack()
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            Icon(
                                imageVector = Icons.Default.Devices,
                                contentDescription = "Watch",
                                tint = TacticalCyan,
                                modifier = Modifier.size(20.dp)
                            )
                            Column {
                                Text(
                                    text = "Q-LINK // ESP32-S3",
                                    color = TacticalCyan,
                                    fontFamily = FontFamily.Monospace,
                                    fontWeight = FontWeight.Bold,
                                    fontSize = 14.sp,
                                    letterSpacing = 1.sp
                                )
                                Text(
                                    text = "SUPERMINI EDITION v2.4",
                                    color = TextMuted,
                                    fontFamily = FontFamily.Monospace,
                                    fontSize = 8.sp,
                                    letterSpacing = 0.5.sp
                                )
                            }
                        }

                        // Connection State Indicator
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(6.dp),
                            modifier = Modifier.padding(end = 8.dp)
                        ) {
                            val dotColor = when (connectionState) {
                                ConnectionState.CONNECTED_WIFI, ConnectionState.CONNECTED_BLE -> TacticalGreen
                                ConnectionState.CONNECTING, ConnectionState.SCANNING -> TacticalAmber
                                else -> TacticalRed
                            }
                            Box(
                                modifier = Modifier
                                    .size(7.dp)
                                    .background(dotColor, shape = androidx.compose.foundation.shape.CircleShape)
                            )
                            Text(
                                text = connectionState.name.replace("CONNECTED_", ""),
                                color = dotColor,
                                fontSize = 9.sp,
                                fontFamily = FontFamily.Monospace,
                                fontWeight = FontWeight.Bold
                            )
                        }
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = TacticalBackground
                )
            )
        },
        bottomBar = {
            NavigationBar(
                containerColor = TacticalSurface,
                tonalElevation = 8.dp
            ) {
                Screen.values().filter { it.inBottomBar }.forEach { screen ->
                    val isSelected = currentScreen == screen
                    NavigationBarItem(
                        selected = isSelected,
                        onClick = { navigateTo(screen) },
                        icon = {
                            Icon(
                                imageVector = screen.icon,
                                contentDescription = screen.title,
                                tint = if (isSelected) TacticalCyan else TextSecondary,
                                modifier = Modifier.size(20.dp)
                            )
                        },
                        label = {
                            Text(
                                text = screen.title,
                                fontSize = 7.5.sp,
                                maxLines = 1,
                                softWrap = false,
                                overflow = TextOverflow.Ellipsis,
                                fontFamily = FontFamily.Monospace,
                                color = if (isSelected) TacticalCyan else TextMuted
                            )
                        },
                        alwaysShowLabel = true,
                        colors = NavigationBarItemDefaults.colors(
                            indicatorColor = TacticalCyanDim
                        )
                    )
                }
            }
        }
    ) { innerPadding ->
        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding)
                .background(TacticalBackground)
        ) {
            when (currentScreen) {
                Screen.DASHBOARD -> DashboardScreen(
                    onNavigateToMirror = { navigateTo(Screen.MIRROR) },
                    onNavigateToSensors = { navigateTo(Screen.SENSORS) },
                    onNavigateToMochi = { navigateTo(Screen.MOCHI) },
                    onNavigateToPower = { navigateTo(Screen.POWER) },
                    onNavigateToSigint = { navigateTo(Screen.SIGINT) },
                    onNavigateToApps = { navigateTo(Screen.APPS) },
                    onNavigateToAnimMarket = { navigateTo(Screen.ANIM_MARKET) },
                    onNavigateToIrdb = { navigateTo(Screen.IRDB) },
                    onNavigateToFiles = { navigateTo(Screen.FILES) },
                    onNavigateToNotifications = { navigateTo(Screen.NOTIFICATIONS) },
                    onNavigateToSettings = { navigateTo(Screen.SETTINGS) },
                    onNavigateToMore = { navigateTo(Screen.MORE) }
                )
                Screen.MIRROR -> LiveMirrorScreen()
                Screen.SENSORS -> SensorsScreen()
                Screen.POWER -> PowerGovernorScreen(onBack = { navigateBack() })
                Screen.MORE -> MoreHubScreen(
                    onNavigateToSigint = { navigateTo(Screen.SIGINT) },
                    onNavigateToIrdb = { navigateTo(Screen.IRDB) },
                    onNavigateToMochi = { navigateTo(Screen.MOCHI) },
                    onNavigateToAnimMarket = { navigateTo(Screen.ANIM_MARKET) },
                    onNavigateToApps = { navigateTo(Screen.APPS) },
                    onNavigateToFiles = { navigateTo(Screen.FILES) },
                    onNavigateToNotifications = { navigateTo(Screen.NOTIFICATIONS) },
                    onNavigateToSettings = { navigateTo(Screen.SETTINGS) },
                    onBack = { navigateBack() }
                )
                Screen.MOCHI -> MochiPetScreen(
                    onBack = { navigateBack() },
                    onNavigateToMarket = { navigateTo(Screen.ANIM_MARKET) }
                )
                Screen.ANIM_MARKET -> AnimMarketScreen(onBack = { navigateBack() })
                Screen.IRDB -> OnlineIrdbScreen(onBack = { navigateBack() })
                Screen.SIGINT -> SigintReconScreen(onBack = { navigateBack() })
                Screen.FILES -> FilesScreen()
                Screen.APPS -> AppStoreScreen()
                Screen.NOTIFICATIONS -> NotificationsScreen()
                Screen.SETTINGS -> SettingsScreen()
            }
        }
    }
}
