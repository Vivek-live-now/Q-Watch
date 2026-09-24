package com.qwatch.qlink.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable

private val DarkColorScheme = darkColorScheme(
    primary = TacticalCyan,
    onPrimary = OledBlack,
    primaryContainer = TacticalCyanDim,
    onPrimaryContainer = TacticalCyan,
    secondary = TacticalAmber,
    onSecondary = OledBlack,
    secondaryContainer = TacticalAmberDim,
    onSecondaryContainer = TacticalAmber,
    tertiary = TacticalGreen,
    background = TacticalBackground,
    onBackground = TextPrimary,
    surface = TacticalSurface,
    onSurface = TextPrimary,
    surfaceVariant = TacticalSurfaceVariant,
    onSurfaceVariant = TextSecondary,
    outline = TacticalBorder
)

@Composable
fun QLinkTheme(
    content: @Composable () -> Unit
) {
    MaterialTheme(
        colorScheme = DarkColorScheme,
        typography = Typography,
        content = content
    )
}
