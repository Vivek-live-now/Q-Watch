package com.qwatch.qlink.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.ui.components.*
import com.qwatch.qlink.ui.theme.*

@Composable
fun SensorsScreen() {
    val client = QLinkClient.instance
    val telemetry by client.telemetry.collectAsState()
    val scrollState = rememberScrollState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(TacticalBackground)
            .padding(16.dp)
            .verticalScroll(scrollState),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // 1. BME280 Environmental Station
        TacticalCard(title = "BME280 ATMOSPHERIC STATION", accentColor = TacticalCyan) {
            BmeReadoutGrid(bme = telemetry.bme)
        }

        // 2. Flight & Motion Dynamics: Artificial Horizon & Compass Rose
        TacticalCard(title = "FLIGHT & ATTITUDE COCKPIT", accentColor = TacticalAmber) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(text = "MPU-6500 HORIZON", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    Spacer(modifier = Modifier.height(4.dp))
                    ArtificialHorizonGauge(imu = telemetry.imu)
                }

                Column(modifier = Modifier.weight(1f)) {
                    Text(text = "QMC5883P COMPASS", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                    Spacer(modifier = Modifier.height(4.dp))
                    CompassRoseGauge(compass = telemetry.compass)
                }
            }
        }

        // 3. Biometric Suite: MAX30102 Health
        TacticalCard(title = "MAX30102 BIOMETRICS", accentColor = TacticalRed) {
            HealthPulseGauge(health = telemetry.health)
        }

        // 4. Pedometer & Activity Matrix
        TacticalCard(title = "ACTIVITY & PEDOMETER", accentColor = TacticalGreen) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                MetricItem(label = "STEPS", value = "${telemetry.pedometer.steps}", color = TacticalGreen)
                MetricItem(label = "DISTANCE", value = String.format("%.2f km", telemetry.pedometer.distanceKm), color = TacticalCyan)
                MetricItem(label = "CALORIES", value = "${telemetry.pedometer.caloriesKcal} kcal", color = TacticalAmber)
            }
        }
    }
}
