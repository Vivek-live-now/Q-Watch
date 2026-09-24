package com.qwatch.qlink.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.rotate
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.model.*
import com.qwatch.qlink.ui.theme.*
import kotlin.math.cos
import kotlin.math.sin

@Composable
fun TacticalCard(
    title: String,
    modifier: Modifier = Modifier,
    accentColor: Color = TacticalCyan,
    content: @Composable ColumnScope.() -> Unit
) {
    Column(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
            .background(TacticalSurface)
            .border(1.dp, TacticalBorder, RoundedCornerShape(8.dp))
            .padding(12.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = title,
                color = accentColor,
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.Bold,
                fontSize = 12.sp,
                letterSpacing = 1.sp
            )
            Box(
                modifier = Modifier
                    .size(6.dp)
                    .clip(RoundedCornerShape(3.dp))
                    .background(accentColor)
            )
        }
        Spacer(modifier = Modifier.height(8.dp))
        content()
    }
}

@Composable
fun BmeReadoutGrid(bme: BmeData) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        MetricItem(label = "TEMP", value = String.format("%.1f°C", bme.temperatureC), color = TacticalAmber)
        MetricItem(label = "HUMIDITY", value = String.format("%.1f%%", bme.humidityPct), color = TacticalCyan)
        MetricItem(label = "PRESSURE", value = String.format("%.1f hPa", bme.pressureHpa), color = TacticalGreen)
        MetricItem(label = "ALTITUDE", value = String.format("%.0fm", bme.altitudeM), color = TextPrimary)
    }
}

@Composable
fun MetricItem(label: String, value: String, color: Color = TextPrimary) {
    Column {
        Text(text = label, color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
        Text(text = value, color = color, fontSize = 13.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
    }
}

@Composable
fun ArtificialHorizonGauge(imu: ImuData, modifier: Modifier = Modifier) {
    Box(
        modifier = modifier
            .aspectRatio(1f)
            .clip(RoundedCornerShape(8.dp))
            .background(OledBlack)
            .border(1.dp, TacticalBorder, RoundedCornerShape(8.dp)),
        contentAlignment = Alignment.Center
    ) {
        Canvas(modifier = Modifier.fillMaxSize().padding(8.dp)) {
            val cx = size.width / 2f
            val cy = size.height / 2f
            val radius = size.minDimension / 2f

            // Pitch displacement: 1 degree pitch = 1.5 pixels
            val pitchOffset = (imu.pitchDeg * 1.5f).coerceIn(-radius * 0.7f, radius * 0.7f)

            // Draw Artificial Horizon with Roll rotation
            rotate(degrees = -imu.rollDeg, pivot = Offset(cx, cy)) {
                // Sky & Ground divider line
                drawLine(
                    color = TacticalCyan,
                    start = Offset(cx - radius * 0.8f, cy + pitchOffset),
                    end = Offset(cx + radius * 0.8f, cy + pitchOffset),
                    strokeWidth = 2.dp.toPx()
                )

                // Pitch ladder markers
                for (step in listOf(-20, -10, 10, 20)) {
                    val ladderY = cy + pitchOffset - (step * 1.5f)
                    if (ladderY in (cy - radius * 0.8f)..(cy + radius * 0.8f)) {
                        val w = radius * 0.3f
                        drawLine(
                            color = TacticalCyan.copy(alpha = 0.5f),
                            start = Offset(cx - w / 2f, ladderY),
                            end = Offset(cx + w / 2f, ladderY),
                            strokeWidth = 1.dp.toPx()
                        )
                    }
                }
            }

            // Fixed aircraft reticle (Yellow V-bar & center dot)
            drawLine(
                color = TacticalAmber,
                start = Offset(cx - 24.dp.toPx(), cy),
                end = Offset(cx - 8.dp.toPx(), cy),
                strokeWidth = 3.dp.toPx()
            )
            drawLine(
                color = TacticalAmber,
                start = Offset(cx + 8.dp.toPx(), cy),
                end = Offset(cx + 24.dp.toPx(), cy),
                strokeWidth = 3.dp.toPx()
            )
            drawCircle(color = TacticalAmber, radius = 3.dp.toPx(), center = Offset(cx, cy))

            // Outer Reticle Circle
            drawCircle(color = TacticalBorder, radius = radius - 2.dp.toPx(), style = Stroke(width = 1.dp.toPx()))
        }

        Text(
            text = String.format("P:%.1f° R:%.1f°", imu.pitchDeg, imu.rollDeg),
            color = TextSecondary,
            fontSize = 9.sp,
            fontFamily = FontFamily.Monospace,
            modifier = Modifier.align(Alignment.BottomCenter).padding(bottom = 4.dp)
        )
    }
}

@Composable
fun CompassRoseGauge(compass: CompassData, modifier: Modifier = Modifier) {
    Box(
        modifier = modifier
            .aspectRatio(1f)
            .clip(RoundedCornerShape(8.dp))
            .background(OledBlack)
            .border(1.dp, TacticalBorder, RoundedCornerShape(8.dp)),
        contentAlignment = Alignment.Center
    ) {
        Canvas(modifier = Modifier.fillMaxSize().padding(8.dp)) {
            val cx = size.width / 2f
            val cy = size.height / 2f
            val radius = size.minDimension / 2f

            drawCircle(color = TacticalBorder, radius = radius - 2.dp.toPx(), style = Stroke(width = 1.dp.toPx()))

            // Rotating Compass Needle
            rotate(degrees = -compass.headingDeg, pivot = Offset(cx, cy)) {
                // North Pointer (Red / Amber Arrow)
                val northPath = Path().apply {
                    moveTo(cx, cy - radius * 0.8f)
                    lineTo(cx - 7.dp.toPx(), cy)
                    lineTo(cx + 7.dp.toPx(), cy)
                    close()
                }
                drawPath(path = northPath, color = TacticalRed)

                // South Pointer (White / Gray Arrow)
                val southPath = Path().apply {
                    moveTo(cx, cy + radius * 0.8f)
                    lineTo(cx - 7.dp.toPx(), cy)
                    lineTo(cx + 7.dp.toPx(), cy)
                    close()
                }
                drawPath(path = southPath, color = TextMuted)
            }

            drawCircle(color = OledBlack, radius = 5.dp.toPx(), center = Offset(cx, cy))
            drawCircle(color = TacticalCyan, radius = 3.dp.toPx(), center = Offset(cx, cy))
        }

        Text(
            text = String.format("%03.0f°", compass.headingDeg),
            color = TacticalCyan,
            fontSize = 12.sp,
            fontFamily = FontFamily.Monospace,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.align(Alignment.BottomCenter).padding(bottom = 4.dp)
        )
    }
}

@Composable
fun HealthPulseGauge(health: HealthData, modifier: Modifier = Modifier) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.SpaceAround,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text(text = "HEART RATE", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
            Text(
                text = if (health.heartRateBpm > 0) "${health.heartRateBpm} BPM" else "-- BPM",
                color = TacticalRed,
                fontSize = 18.sp,
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.Bold
            )
        }

        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text(text = "BLOOD OXYGEN", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
            Text(
                text = if (health.spo2Pct > 0) "${health.spo2Pct}% SpO2" else "--%",
                color = TacticalCyan,
                fontSize = 18.sp,
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.Bold
            )
        }

        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text(text = "SENSOR", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
            Text(
                text = if (health.fingerDetected) "[LOCKED]" else "[OFF]",
                color = if (health.fingerDetected) TacticalGreen else TextMuted,
                fontSize = 12.sp,
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.Bold
            )
        }
    }
}
