package com.qwatch.qlink.ui.components

import android.graphics.Bitmap
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
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.model.DisplayFrame
import com.qwatch.qlink.protocol.QLinkConstants
import com.qwatch.qlink.ui.theme.*

@Composable
fun VirtualOledDisplay(
    frame: DisplayFrame,
    isStreaming: Boolean,
    modifier: Modifier = Modifier,
    phosphorColor: Color = PhosphorCyan,
    showPixelGrid: Boolean = true
) {
    Box(
        modifier = modifier
            .aspectRatio(128f / 64f)
            .clip(RoundedCornerShape(8.dp))
            .background(PhosphorOff)
            .border(2.dp, if (isStreaming) TacticalCyan else TacticalBorder, RoundedCornerShape(8.dp))
            .padding(4.dp)
    ) {
        if (!isStreaming && frame.frameNumber == 0L) {
            Box(
                modifier = Modifier.fillMaxSize(),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = "[ OLED STREAM STANDBY ]",
                    color = TextMuted,
                    fontSize = 11.sp,
                    fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                )
            }
        } else {
            Canvas(modifier = Modifier.fillMaxSize()) {
                val canvasW = size.width
                val canvasH = size.height

                val pixelW = canvasW / QLinkConstants.OLED_WIDTH
                val pixelH = canvasH / QLinkConstants.OLED_HEIGHT

                val buffer = frame.buffer
                if (buffer.size >= QLinkConstants.OLED_FRAME_BYTES) {
                    // SSD1306/SH1106 Page Layout: 8 pages (rows of 8 pixels), 128 columns
                    for (page in 0 until 8) {
                        val pageOffset = page * QLinkConstants.OLED_WIDTH
                        val baseY = page * 8

                        for (x in 0 until QLinkConstants.OLED_WIDTH) {
                            val byteVal = buffer[pageOffset + x].toInt() and 0xFF
                            if (byteVal == 0) continue

                            for (bit in 0 until 8) {
                                if ((byteVal and (1 shl bit)) != 0) {
                                    val y = baseY + bit
                                    val left = x * pixelW
                                    val top = y * pixelH

                                    drawRect(
                                        color = phosphorColor,
                                        topLeft = Offset(left, top),
                                        size = Size(
                                            width = if (showPixelGrid) (pixelW * 0.9f) else pixelW,
                                            height = if (showPixelGrid) (pixelH * 0.9f) else pixelH
                                        )
                                    )
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
 * Utility to convert raw 1024-byte OLED buffer to Android Bitmap for screenshot sharing.
 */
fun exportFrameToBitmap(buffer: ByteArray, onColor: Int = android.graphics.Color.CYAN, offColor: Int = android.graphics.Color.BLACK): Bitmap {
    val bmp = Bitmap.createBitmap(128, 64, Bitmap.Config.ARGB_8888)
    if (buffer.size < 1024) return bmp

    for (page in 0 until 8) {
        val pageOffset = page * 128
        val baseY = page * 8
        for (x in 0 until 128) {
            val byteVal = buffer[pageOffset + x].toInt() and 0xFF
            for (bit in 0 until 8) {
                val y = baseY + bit
                val isSet = (byteVal and (1 shl bit)) != 0
                bmp.setPixel(x, y, if (isSet) onColor else offColor)
            }
        }
    }
    return bmp
}
