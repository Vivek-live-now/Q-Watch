package com.qwatch.qlink.ui.components

import android.view.HapticFeedbackConstants
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.model.ButtonEventType
import com.qwatch.qlink.model.ButtonType
import com.qwatch.qlink.ui.theme.*

@Composable
fun TacticalDPad(
    onButtonAction: (ButtonType, ButtonEventType) -> Unit,
    modifier: Modifier = Modifier
) {
    val view = LocalView.current

    fun performHaptic() {
        view.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
    }

    Column(
        modifier = modifier
            .fillMaxWidth()
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        // Row 1: UP
        DPadButton(
            label = "UP",
            icon = Icons.Default.KeyboardArrowUp,
            modifier = Modifier.width(130.dp).height(50.dp),
            onClick = {
                performHaptic()
                onButtonAction(ButtonType.UP, ButtonEventType.SHORT_PRESS)
            }
        )

        // Row 2: CANCEL | OK | DOWN
        Row(
            horizontalArrangement = Arrangement.spacedBy(12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            DPadButton(
                label = "BACK",
                icon = Icons.Default.ArrowBack,
                accentColor = TacticalRed,
                modifier = Modifier.width(96.dp).height(50.dp),
                onClick = {
                    performHaptic()
                    onButtonAction(ButtonType.CANCEL, ButtonEventType.SHORT_PRESS)
                }
            )

            DPadButton(
                label = "OK / SEL",
                accentColor = TacticalAmber,
                modifier = Modifier.width(110.dp).height(50.dp),
                onClick = {
                    performHaptic()
                    onButtonAction(ButtonType.OK, ButtonEventType.SHORT_PRESS)
                }
            )

            DPadButton(
                label = "DOWN",
                icon = Icons.Default.KeyboardArrowDown,
                modifier = Modifier.width(96.dp).height(50.dp),
                onClick = {
                    performHaptic()
                    onButtonAction(ButtonType.DOWN, ButtonEventType.SHORT_PRESS)
                }
            )
        }
    }
}

@Composable
private fun DPadButton(
    label: String,
    modifier: Modifier = Modifier,
    icon: ImageVector? = null,
    accentColor: androidx.compose.ui.graphics.Color = TacticalCyan,
    onClick: () -> Unit
) {
    val interactionSource = remember { MutableInteractionSource() }
    val isPressed by interactionSource.collectIsPressedAsState()

    val bg = if (isPressed) accentColor.copy(alpha = 0.25f) else TacticalSurface
    val border = if (isPressed) accentColor else TacticalBorder

    Box(
        modifier = modifier
            .clip(RoundedCornerShape(8.dp))
            .background(bg)
            .border(1.5.dp, border, RoundedCornerShape(8.dp))
            .clickable(interactionSource = interactionSource, indication = null, onClick = onClick),
        contentAlignment = Alignment.Center
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(4.dp)
        ) {
            if (icon != null) {
                Icon(
                    imageVector = icon,
                    contentDescription = label,
                    tint = if (isPressed) accentColor else TextPrimary,
                    modifier = Modifier.size(18.dp)
                )
            }
            Text(
                text = label,
                color = if (isPressed) accentColor else TextPrimary,
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.Bold,
                fontSize = 12.sp
            )
        }
    }
}
