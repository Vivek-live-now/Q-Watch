package com.qwatch.qlink

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
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
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import com.qwatch.qlink.model.ConnectionState
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.ui.screens.*
import com.qwatch.qlink.ui.theme.*

enum class Screen(val title: String, val icon: ImageVector) {
    DASHBOARD("DASHBOARD", Icons.Default.Dashboard),
    MIRROR("OLED MIRROR", Icons.Default.Tv),
    SENSORS("SENSORS", Icons.Default.Speed),
    FILES("FILES", Icons.Default.Folder),
    APPS("APPS", Icons.Default.Apps),
    NOTIFICATIONS("ALERTS", Icons.Default.Notifications),
    SETTINGS("SETTINGS", Icons.Default.Settings)
}

class MainActivity : ComponentActivity() {

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { _ -> }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requestRequiredPermissions()

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
    var currentScreen by remember { mutableStateOf(Screen.DASHBOARD) }
    val client = QLinkClient.instance
    val connectionState by client.connectionState.collectAsState()

    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = "Q-LINK TACTICAL",
                            color = TacticalCyan,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold,
                            fontSize = 16.sp
                        )

                        // Connection State Indicator
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(6.dp),
                            modifier = Modifier.padding(end = 12.dp)
                        ) {
                            val dotColor = when (connectionState) {
                                ConnectionState.CONNECTED_WIFI, ConnectionState.CONNECTED_BLE -> TacticalGreen
                                ConnectionState.CONNECTING, ConnectionState.SCANNING -> TacticalAmber
                                else -> TacticalRed
                            }
                            Box(
                                modifier = Modifier
                                    .size(8.dp)
                                    .background(dotColor, shape = androidx.compose.foundation.shape.CircleShape)
                            )
                            Text(
                                text = connectionState.name.replace("CONNECTED_", ""),
                                color = dotColor,
                                fontSize = 10.sp,
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
                Screen.values().forEach { screen ->
                    val isSelected = currentScreen == screen
                    NavigationBarItem(
                        selected = isSelected,
                        onClick = { currentScreen = screen },
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
                                fontSize = 8.sp,
                                fontFamily = FontFamily.Monospace,
                                color = if (isSelected) TacticalCyan else TextMuted
                            )
                        },
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
                    onNavigateToMirror = { currentScreen = Screen.MIRROR },
                    onNavigateToSensors = { currentScreen = Screen.SENSORS }
                )
                Screen.MIRROR -> LiveMirrorScreen()
                Screen.SENSORS -> SensorsScreen()
                Screen.FILES -> FilesScreen()
                Screen.APPS -> AppStoreScreen()
                Screen.NOTIFICATIONS -> NotificationsScreen()
                Screen.SETTINGS -> SettingsScreen()
            }
        }
    }
}
