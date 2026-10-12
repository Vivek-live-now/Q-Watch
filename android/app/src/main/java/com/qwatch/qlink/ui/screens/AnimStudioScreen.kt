package com.qwatch.qlink.ui.screens

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.anim.*
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.ui.components.TacticalCard
import com.qwatch.qlink.ui.theme.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AnimStudioScreen(
    onBack: () -> Unit,
    initialAnimBytes: ByteArray? = null,
    initialAnimName: String? = null
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val client = remember { QLinkClient.instance }

    // Studio Project State
    var animName by remember { mutableStateOf(initialAnimName?.removeSuffix(".anim") ?: "custom_anim") }
    var frames by remember {
        val initialList = mutableListOf<StudioFrame>()
        if (initialAnimBytes != null) {
            val parsed = AnimParser.parseAnim("initial", initialAnimBytes)
            if (parsed != null && parsed.frames.isNotEmpty()) {
                val isMsb = AnimParser.detectLikelyMsb(parsed.frames[0])
                for (fb in parsed.frames) {
                    val px = AnimParser.decodeFramePixels(fb, isMsb)
                    initialList.add(StudioFrame(pixels = px, delayMs = parsed.header.frameDelayMs))
                }
            }
        }
        if (initialList.isEmpty()) {
            initialList.add(StudioFrame())
        }
        mutableStateOf(initialList)
    }

    var currentFrameIndex by remember { mutableIntStateOf(0) }
    var selectedTool by remember { mutableStateOf(DrawTool.PENCIL) }
    var phosphorColor by remember { mutableStateOf(PhosphorAmber) }
    var showGrid by remember { mutableStateOf(true) }
    var onionSkin by remember { mutableStateOf(false) }
    var zoomLevel by remember { mutableFloatStateOf(1f) } // 1f = fit, 2f = zoom, 4f = super zoom
    var isPlaying by remember { mutableStateOf(false) }
    var playbackFps by remember { mutableIntStateOf(20) }
    var isLooping by remember { mutableStateOf(true) }
    var isInverted by remember { mutableStateOf(false) }

    // Fast Bitmap Rendering & Drag Tracking
    var canvasRevision by remember { mutableIntStateOf(0) }
    val displayBitmap = remember { Bitmap.createBitmap(128, 64, Bitmap.Config.ARGB_8888) }
    val pixelBuffer = remember { IntArray(128 * 64) }
    var lastTouchCoord by remember { mutableStateOf<Pair<Int, Int>?>(null) }

    // Interactive Drag Shape State & Telemetry
    var dragStartPos by remember { mutableStateOf<Pair<Int, Int>?>(null) }
    var dragCurrentPos by remember { mutableStateOf<Pair<Int, Int>?>(null) }
    var isDraggingShape by remember { mutableStateOf(false) }
    var hoverCoord by remember { mutableStateOf<Pair<Int, Int>?>(null) }

    // Undo / Redo History Stacks (Per-Frame)
    val undoStack = remember { mutableStateListOf<BooleanArray>() }
    val redoStack = remember { mutableStateListOf<BooleanArray>() }

    // Deployment & Status State
    var isDeploying by remember { mutableStateOf(false) }
    var deployProgress by remember { mutableFloatStateOf(0f) }
    var statusMessage by remember { mutableStateOf<String?>(null) }

    // Static Image Import Dialog State
    var showImportDialog by remember { mutableStateOf(false) }
    var pendingImportBitmap by remember { mutableStateOf<Bitmap?>(null) }
    var importDitherMode by remember { mutableStateOf(DitherMode.FLOYD_STEINBERG) }
    var importThreshold by remember { mutableFloatStateOf(128f) }
    var importBrightness by remember { mutableFloatStateOf(0f) }
    var importContrast by remember { mutableFloatStateOf(0f) }
    var importInvert by remember { mutableStateOf(false) }
    var importFitMode by remember { mutableStateOf(AspectFitMode.CROP) }

    // Animated GIF Import Dialog State
    var showGifImportDialog by remember { mutableStateOf(false) }
    var pendingGifFrames by remember { mutableStateOf<List<Pair<Bitmap, Int>>>(emptyList()) }
    var pendingGifFileName by remember { mutableStateOf("imported_gif") }
    var gifDitherMode by remember { mutableStateOf(DitherMode.FLOYD_STEINBERG) }
    var gifFitMode by remember { mutableStateOf(AspectFitMode.CROP) }
    var gifBrightness by remember { mutableFloatStateOf(0f) }
    var gifContrast by remember { mutableFloatStateOf(0f) }
    var gifThreshold by remember { mutableFloatStateOf(128f) }
    var gifInvert by remember { mutableStateOf(false) }
    var gifFpsOverride by remember { mutableIntStateOf(0) }
    var gifSubsample by remember { mutableIntStateOf(1) }
    var isProcessingGif by remember { mutableStateOf(false) }
    var gifPreviewIndex by remember { mutableIntStateOf(0) }
    var gifPreviewPlaying by remember { mutableStateOf(true) }

    // Presets Dialog State
    var showPresetsDialog by remember { mutableStateOf(false) }

    val safeIndex = currentFrameIndex.coerceIn(0, (frames.size - 1).coerceAtLeast(0))
    val currentFrame = if (frames.isNotEmpty()) frames[safeIndex] else StudioFrame()

    // Push state to undo stack before mutation
    fun pushUndo() {
        if (undoStack.size > 25) undoStack.removeAt(0)
        undoStack.add(currentFrame.pixels.clone())
        redoStack.clear()
    }

    fun drawLineOnFrame(frame: StudioFrame, x0: Int, y0: Int, x1: Int, y1: Int, value: Boolean) {
        var x = x0
        var y = y0
        val dx = kotlin.math.abs(x1 - x0)
        val dy = -kotlin.math.abs(y1 - y0)
        val sx = if (x0 < x1) 1 else -1
        val sy = if (y0 < y1) 1 else -1
        var err = dx + dy
        while (true) {
            frame.setPixel(x, y, value)
            if (x == x1 && y == y1) break
            val e2 = 2 * err
            if (e2 >= dy) { err += dy; x += sx }
            if (e2 <= dx) { err += dx; y += sy }
        }
    }

    // Live Playback Loop
    LaunchedEffect(isPlaying, playbackFps, frames.size) {
        if (isPlaying && frames.isNotEmpty()) {
            while (isActive && isPlaying) {
                val delayMs = (1000 / playbackFps.coerceIn(1, 60)).toLong()
                delay(delayMs)
                currentFrameIndex = (currentFrameIndex + 1) % frames.size
            }
        }
    }

    // Media File Picker (GIF, PNG, JPG, BMP, .anim)
    val filePickerLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        if (uri != null) {
            try {
                val bytes = context.contentResolver.openInputStream(uri)?.use { it.readBytes() }
                if (bytes != null && bytes.isNotEmpty()) {
                    val mimeType = context.contentResolver.getType(uri) ?: ""
                    val fileName = uri.lastPathSegment?.substringAfterLast('/') ?: "imported"

                    if (AnimEncoder.isAnim(bytes) || fileName.endsWith(".anim", ignoreCase = true)) {
                        val parsed = AnimParser.parseAnim(fileName, bytes)
                        if (parsed != null && parsed.frames.isNotEmpty()) {
                            val isMsb = AnimParser.detectLikelyMsb(parsed.frames[0])
                            val newFrames = parsed.frames.map { fb ->
                                StudioFrame(
                                    pixels = AnimParser.decodeFramePixels(fb, isMsb),
                                    delayMs = parsed.header.frameDelayMs
                                )
                            }
                            frames = newFrames.toMutableList()
                            currentFrameIndex = 0
                            animName = fileName.removeSuffix(".anim")
                            canvasRevision++
                            statusMessage = "Loaded ${frames.size} frames from .anim"
                        } else {
                            statusMessage = "Invalid .anim binary file!"
                        }
                    } else if (AnimEncoder.isGif(bytes) || mimeType.contains("gif", ignoreCase = true) || fileName.endsWith(".gif", ignoreCase = true)) {
                        val gifFrames = AnimEncoder.decodeGif(bytes)
                        if (gifFrames.isNotEmpty()) {
                            pendingGifFrames = gifFrames
                            pendingGifFileName = fileName.substringBeforeLast('.').ifBlank { "imported_gif" }
                            gifPreviewIndex = 0
                            gifPreviewPlaying = true
                            showGifImportDialog = true
                            statusMessage = "Loaded ${gifFrames.size} frames. Configure quantization options."
                        } else {
                            statusMessage = "Failed to extract GIF frames!"
                        }
                    } else if (AnimEncoder.isBmp(bytes)) {
                        val bmpFrame = AnimEncoder.decodeBmpToStudioFrame(bytes)
                        if (bmpFrame != null) {
                            val newFrames = frames.toMutableList()
                            newFrames.add(safeIndex + 1, bmpFrame)
                            frames = newFrames
                            currentFrameIndex = safeIndex + 1
                            canvasRevision++
                            statusMessage = "Imported 128x64 1-bit BMP!"
                        } else {
                            val bmp = BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
                            if (bmp != null) {
                                pendingImportBitmap = bmp
                                showImportDialog = true
                            } else {
                                statusMessage = "Unsupported BMP format!"
                            }
                        }
                    } else {
                        // Static image (PNG, JPG, WEBP)
                        val bmp = BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
                        if (bmp != null) {
                            pendingImportBitmap = bmp
                            showImportDialog = true
                        } else {
                            statusMessage = "Unsupported image format!"
                        }
                    }
                }
            } catch (e: Exception) {
                statusMessage = "Import failed: ${e.message}"
            }
        }
    }

    statusMessage?.let { msg ->
        LaunchedEffect(msg) {
            Toast.makeText(context, msg, Toast.LENGTH_SHORT).show()
            statusMessage = null
        }
    }

    val mainScrollState = rememberScrollState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(TacticalBackground)
            .padding(12.dp)
            .verticalScroll(mainScrollState),
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        // 1. Top Header & Action Controls
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                IconButton(onClick = onBack, modifier = Modifier.size(32.dp)) {
                    Icon(imageVector = Icons.Default.ArrowBack, contentDescription = "Back", tint = TacticalCyan)
                }
                Column {
                    Text(
                        text = "ANIMATION PACKER & STUDIO",
                        color = TacticalCyan,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold,
                        fontSize = 12.sp,
                        letterSpacing = 0.5.sp
                    )
                    Text(
                        text = "LIVE 128x64 OLED PIXEL SUITE",
                        color = TextMuted,
                        fontFamily = FontFamily.Monospace,
                        fontSize = 8.sp
                    )
                }
            }

            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Button(
                    onClick = { showPresetsDialog = true },
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalCyanDim),
                    border = androidx.compose.foundation.BorderStroke(1.dp, TacticalCyan),
                    shape = RoundedCornerShape(4.dp),
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 4.dp),
                    modifier = Modifier.height(28.dp)
                ) {
                    Text("PRESETS", color = TacticalCyan, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }

                Button(
                    onClick = { filePickerLauncher.launch("*/*") },
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalAmberDim),
                    border = androidx.compose.foundation.BorderStroke(1.dp, TacticalAmber),
                    shape = RoundedCornerShape(4.dp),
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 4.dp),
                    modifier = Modifier.height(28.dp)
                ) {
                    Text("IMPORT", color = TacticalAmber, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }
            }
        }

        // 2. Interactive OLED Matrix Canvas & Touch Drawing Deck
        TacticalCard(title = "128x64 INTERACTIVE OLED MATRIX CANVAS", accentColor = TacticalAmber) {
            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                // Canvas HUD Header: Frame Counter, Zoom Toggle, Grid, Onion Skin
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = "FRAME ${safeIndex + 1}/${frames.size} (${1000 / currentFrame.delayMs.coerceAtLeast(10)} FPS)",
                        color = TacticalAmber,
                        fontSize = 9.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )

                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
                        // Zoom toggles
                        listOf(1f to "1X (FIT)", 2f to "2X", 3f to "3X", 4f to "4X").forEach { (lvl, lbl) ->
                            val isSel = zoomLevel == lvl
                            Box(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(3.dp))
                                    .background(if (isSel) TacticalCyan else TacticalSurfaceVariant)
                                    .border(1.dp, if (isSel) TacticalCyan else TacticalBorder, RoundedCornerShape(3.dp))
                                    .clickable { zoomLevel = lvl }
                                    .padding(horizontal = 5.dp, vertical = 2.dp)
                            ) {
                                Text(
                                    text = lbl,
                                    color = if (isSel) OledBlack else TacticalCyan,
                                    fontSize = 7.sp,
                                    fontFamily = FontFamily.Monospace,
                                    fontWeight = FontWeight.Bold
                                )
                            }
                        }

                        // Grid Toggle
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(3.dp))
                                .background(if (showGrid) TacticalCyanDim else TacticalSurfaceVariant)
                                .border(1.dp, if (showGrid) TacticalCyan else TacticalBorder, RoundedCornerShape(3.dp))
                                .clickable { showGrid = !showGrid }
                                .padding(horizontal = 5.dp, vertical = 2.dp)
                        ) {
                            Text(
                                text = if (showGrid) "GRID" else "NO GRID",
                                color = if (showGrid) TacticalCyan else TextMuted,
                                fontSize = 7.sp,
                                fontFamily = FontFamily.Monospace,
                                fontWeight = FontWeight.Bold
                            )
                        }

                        // Onion Skin Toggle
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(3.dp))
                                .background(if (onionSkin) TacticalGreenDim else TacticalSurfaceVariant)
                                .border(1.dp, if (onionSkin) TacticalGreen else TacticalBorder, RoundedCornerShape(3.dp))
                                .clickable { onionSkin = !onionSkin }
                                .padding(horizontal = 5.dp, vertical = 2.dp)
                        ) {
                            Text(
                                text = if (onionSkin) "ONION" else "NO ONION",
                                color = if (onionSkin) TacticalGreen else TextMuted,
                                fontSize = 7.sp,
                                fontFamily = FontFamily.Monospace,
                                fontWeight = FontWeight.Bold
                            )
                        }
                    }
                }

                // Interactive OLED Matrix Coordinates & Tool Telemetry HUD
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(4.dp))
                        .background(TacticalSurfaceVariant)
                        .padding(horizontal = 8.dp, vertical = 3.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                        Text(
                            text = "X: ${hoverCoord?.first ?: "--"}  Y: ${hoverCoord?.second ?: "--"}",
                            color = TacticalGreen,
                            fontSize = 8.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                        Text(
                            text = "TOOL: ${selectedTool.label}",
                            color = TacticalCyan,
                            fontSize = 8.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                    }
                    Text(
                        text = "128x64 MONO OLED (2:1)",
                        color = TextMuted,
                        fontSize = 7.5.sp,
                        fontFamily = FontFamily.Monospace
                    )
                }

                // Authentic 128x64 Interactive Drawing Canvas Container
                val prevFrame = if (onionSkin && safeIndex > 0) frames[safeIndex - 1] else null
                val canvasHorizScroll = rememberScrollState()
                val canvasVertScroll = rememberScrollState()

                val canvasModifier = if (zoomLevel > 1f) {
                    Modifier
                        .size((128f * zoomLevel * 2.6f).dp, (64f * zoomLevel * 2.6f).dp)
                } else {
                    Modifier.fillMaxSize()
                }

                val containerModifier = if (zoomLevel > 1f) {
                    Modifier
                        .fillMaxWidth()
                        .height(200.dp)
                        .clip(RoundedCornerShape(6.dp))
                        .background(Color(0xFF040608))
                        .border(1.5.dp, TacticalBorder, RoundedCornerShape(6.dp))
                        .horizontalScroll(canvasHorizScroll)
                        .verticalScroll(canvasVertScroll)
                } else {
                    Modifier
                        .fillMaxWidth().aspectRatio(128f / 64f)
                        .clip(RoundedCornerShape(6.dp))
                        .background(Color(0xFF040608))
                        .border(1.5.dp, TacticalBorder, RoundedCornerShape(6.dp))
                }

                Box(
                    modifier = containerModifier,
                    contentAlignment = Alignment.Center
                ) {
                    Canvas(
                        modifier = canvasModifier
                            .pointerInput(selectedTool, safeIndex) {
                                detectDragGestures(
                                    onDragStart = { offset ->
                                        val px = (offset.x / size.width * 128).toInt().coerceIn(0, 127)
                                        val py = (offset.y / size.height * 64).toInt().coerceIn(0, 63)
                                        hoverCoord = px to py
                                        pushUndo()

                                        when (selectedTool) {
                                            DrawTool.PENCIL -> {
                                                lastTouchCoord = px to py
                                                currentFrame.setPixel(px, py, true)
                                                canvasRevision++
                                            }
                                            DrawTool.ERASER -> {
                                                lastTouchCoord = px to py
                                                currentFrame.setPixel(px, py, false)
                                                canvasRevision++
                                            }
                                            DrawTool.BUCKET -> {
                                                currentFrame.floodFill(px, py, true)
                                                canvasRevision++
                                            }
                                            DrawTool.LINE, DrawTool.RECT, DrawTool.FILL_RECT, DrawTool.CIRCLE, DrawTool.FILL_CIRCLE -> {
                                                dragStartPos = px to py
                                                dragCurrentPos = px to py
                                                isDraggingShape = true
                                            }
                                            DrawTool.INVERT -> {
                                                currentFrame.invert()
                                                canvasRevision++
                                            }
                                            DrawTool.CLEAR -> {
                                                currentFrame.clear()
                                                canvasRevision++
                                            }
                                            DrawTool.FILL -> {
                                                currentFrame.fillAll()
                                                canvasRevision++
                                            }
                                        }
                                    },
                                    onDrag = { change, _ ->
                                        change.consume()
                                        val px = (change.position.x / size.width * 128).toInt().coerceIn(0, 127)
                                        val py = (change.position.y / size.height * 64).toInt().coerceIn(0, 63)
                                        hoverCoord = px to py

                                        when (selectedTool) {
                                            DrawTool.PENCIL -> {
                                                val last = lastTouchCoord
                                                if (last != null && (last.first != px || last.second != py)) {
                                                    drawLineOnFrame(currentFrame, last.first, last.second, px, py, true)
                                                    lastTouchCoord = px to py
                                                    canvasRevision++
                                                }
                                            }
                                            DrawTool.ERASER -> {
                                                val last = lastTouchCoord
                                                if (last != null && (last.first != px || last.second != py)) {
                                                    drawLineOnFrame(currentFrame, last.first, last.second, px, py, false)
                                                    lastTouchCoord = px to py
                                                    canvasRevision++
                                                }
                                            }
                                            DrawTool.LINE, DrawTool.RECT, DrawTool.FILL_RECT, DrawTool.CIRCLE, DrawTool.FILL_CIRCLE -> {
                                                dragCurrentPos = px to py
                                            }
                                            else -> {}
                                        }
                                    },
                                    onDragEnd = {
                                        val start = dragStartPos
                                        val end = dragCurrentPos ?: hoverCoord
                                        if (start != null && end != null) {
                                            when (selectedTool) {
                                                DrawTool.LINE -> {
                                                    currentFrame.drawLine(start.first, start.second, end.first, end.second, true)
                                                    canvasRevision++
                                                }
                                                DrawTool.RECT -> {
                                                    currentFrame.drawRect(start.first, start.second, end.first, end.second, fill = false, value = true)
                                                    canvasRevision++
                                                }
                                                DrawTool.FILL_RECT -> {
                                                    currentFrame.drawRect(start.first, start.second, end.first, end.second, fill = true, value = true)
                                                    canvasRevision++
                                                }
                                                DrawTool.CIRCLE -> {
                                                    val radius = kotlin.math.hypot((end.first - start.first).toDouble(), (end.second - start.second).toDouble()).toInt()
                                                    currentFrame.drawCircle(start.first, start.second, radius, value = true, fill = false)
                                                    canvasRevision++
                                                }
                                                DrawTool.FILL_CIRCLE -> {
                                                    val radius = kotlin.math.hypot((end.first - start.first).toDouble(), (end.second - start.second).toDouble()).toInt()
                                                    currentFrame.drawCircle(start.first, start.second, radius, value = true, fill = true)
                                                    canvasRevision++
                                                }
                                                else -> {}
                                            }
                                        }
                                        dragStartPos = null
                                        dragCurrentPos = null
                                        isDraggingShape = false
                                        lastTouchCoord = null
                                    },
                                    onDragCancel = {
                                        dragStartPos = null
                                        dragCurrentPos = null
                                        isDraggingShape = false
                                        lastTouchCoord = null
                                    }
                                )
                            }
                    ) {
                        // High-speed Bitmap-backed hardware accelerated Canvas rendering
                        val rev = canvasRevision
                        val activeColor = phosphorColor.toArgb()
                        val offColor = 0xFF050708.toInt()
                        val onionColor = phosphorColor.copy(alpha = 0.28f).toArgb()

                        for (y in 0 until 64) {
                            val rowOffset = y * 128
                            for (x in 0 until 128) {
                                val idx = rowOffset + x
                                val isLit = currentFrame.getPixel(x, y)
                                val isOnion = onionSkin && prevFrame != null && prevFrame.getPixel(x, y)
                                pixelBuffer[idx] = when {
                                    isLit -> activeColor
                                    isOnion -> onionColor
                                    else -> offColor
                                }
                            }
                        }

                        displayBitmap.setPixels(pixelBuffer, 0, 128, 0, 0, 128, 64)

                        drawImage(
                            image = displayBitmap.asImageBitmap(),
                            dstSize = IntSize(size.width.toInt(), size.height.toInt()),
                            filterQuality = FilterQuality.None
                        )

                        // Real-time Interactive Drag Shape Overlay Preview
                        if (isDraggingShape && dragStartPos != null && dragCurrentPos != null) {
                            val s = dragStartPos!!
                            val e = dragCurrentPos!!
                            val pixelW = size.width / 128f
                            val pixelH = size.height / 64f
                            val previewColor = phosphorColor.copy(alpha = 0.9f)
                            val previewFillColor = phosphorColor.copy(alpha = 0.35f)

                            when (selectedTool) {
                                DrawTool.LINE -> {
                                    drawLine(
                                        color = previewColor,
                                        start = Offset((s.first + 0.5f) * pixelW, (s.second + 0.5f) * pixelH),
                                        end = Offset((e.first + 0.5f) * pixelW, (e.second + 0.5f) * pixelH),
                                        strokeWidth = pixelW.coerceAtLeast(1.5f)
                                    )
                                }
                                DrawTool.RECT -> {
                                    val left = kotlin.math.min(s.first, e.first) * pixelW
                                    val top = kotlin.math.min(s.second, e.second) * pixelH
                                    val right = (kotlin.math.max(s.first, e.first) + 1) * pixelW
                                    val bottom = (kotlin.math.max(s.second, e.second) + 1) * pixelH
                                    drawRect(
                                        color = previewColor,
                                        topLeft = Offset(left, top),
                                        size = Size(right - left, bottom - top),
                                        style = androidx.compose.ui.graphics.drawscope.Stroke(width = pixelW.coerceAtLeast(1.5f))
                                    )
                                }
                                DrawTool.FILL_RECT -> {
                                    val left = kotlin.math.min(s.first, e.first) * pixelW
                                    val top = kotlin.math.min(s.second, e.second) * pixelH
                                    val right = (kotlin.math.max(s.first, e.first) + 1) * pixelW
                                    val bottom = (kotlin.math.max(s.second, e.second) + 1) * pixelH
                                    drawRect(
                                        color = previewFillColor,
                                        topLeft = Offset(left, top),
                                        size = Size(right - left, bottom - top)
                                    )
                                    drawRect(
                                        color = previewColor,
                                        topLeft = Offset(left, top),
                                        size = Size(right - left, bottom - top),
                                        style = androidx.compose.ui.graphics.drawscope.Stroke(width = pixelW.coerceAtLeast(1.5f))
                                    )
                                }
                                DrawTool.CIRCLE -> {
                                    val radiusPx = kotlin.math.hypot((e.first - s.first).toDouble(), (e.second - s.second).toDouble()).toFloat()
                                    val center = Offset((s.first + 0.5f) * pixelW, (s.second + 0.5f) * pixelH)
                                    val visualRadius = radiusPx * pixelW
                                    drawCircle(
                                        color = previewColor,
                                        center = center,
                                        radius = visualRadius,
                                        style = androidx.compose.ui.graphics.drawscope.Stroke(width = pixelW.coerceAtLeast(1.5f))
                                    )
                                }
                                DrawTool.FILL_CIRCLE -> {
                                    val radiusPx = kotlin.math.hypot((e.first - s.first).toDouble(), (e.second - s.second).toDouble()).toFloat()
                                    val center = Offset((s.first + 0.5f) * pixelW, (s.second + 0.5f) * pixelH)
                                    val visualRadius = radiusPx * pixelW
                                    drawCircle(
                                        color = previewFillColor,
                                        center = center,
                                        radius = visualRadius
                                    )
                                    drawCircle(
                                        color = previewColor,
                                        center = center,
                                        radius = visualRadius,
                                        style = androidx.compose.ui.graphics.drawscope.Stroke(width = pixelW.coerceAtLeast(1.5f))
                                    )
                                }
                                else -> {}
                            }
                        }

                        // Grid overlay when enabled
                        if (showGrid && (zoomLevel > 1.2f || zoomLevel == 1f)) {
                            val pixelW = size.width / 128f
                            val pixelH = size.height / 64f
                            val gridAlpha = if (zoomLevel > 1.2f) 0.12f else 0.04f
                            val gridColor = Color.White.copy(alpha = gridAlpha)
                            for (x in 0..128 step 8) {
                                drawLine(
                                    color = gridColor,
                                    start = Offset(x * pixelW, 0f),
                                    end = Offset(x * pixelW, size.height),
                                    strokeWidth = 0.5f
                                )
                            }
                            for (y in 0..64 step 8) {
                                drawLine(
                                    color = gridColor,
                                    start = Offset(0f, y * pixelH),
                                    end = Offset(size.width, y * pixelH),
                                    strokeWidth = 0.5f
                                )
                            }
                        }
                    }
                }

                // 3. Drawing Tools Ribbon
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(4.dp)
                ) {
                    DrawTool.values().forEach { tool ->
                        val isSelected = selectedTool == tool
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(3.dp))
                                .background(if (isSelected) TacticalAmber else TacticalSurfaceVariant)
                                .border(1.dp, if (isSelected) TacticalAmber else TacticalBorder, RoundedCornerShape(3.dp))
                                .clickable {
                                    selectedTool = tool
                                    dragStartPos = null
                                    dragCurrentPos = null
                                    isDraggingShape = false
                                    if (tool == DrawTool.INVERT) {
                                        pushUndo()
                                        currentFrame.invert()
                                        canvasRevision++
                                    } else if (tool == DrawTool.CLEAR) {
                                        pushUndo()
                                        currentFrame.clear()
                                        canvasRevision++
                                    } else if (tool == DrawTool.FILL) {
                                        pushUndo()
                                        currentFrame.fillAll()
                                        canvasRevision++
                                    }
                                }
                                .padding(horizontal = 7.dp, vertical = 4.dp)
                        ) {
                            Text(
                                text = tool.label,
                                color = if (isSelected) OledBlack else TextPrimary,
                                fontSize = 8.sp,
                                fontFamily = FontFamily.Monospace,
                                fontWeight = FontWeight.Bold
                            )
                        }
                    }
                }

                // 4. Directional Shift & Undo / Redo Toolbar
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    // Shift Nudge buttons
                    Row(horizontalArrangement = Arrangement.spacedBy(3.dp), verticalAlignment = Alignment.CenterVertically) {
                        Text("SHIFT:", color = TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                        listOf(
                            "▲" to (0 to -1),
                            "▼" to (0 to 1),
                            "◀" to (-1 to 0),
                            "▶" to (1 to 0)
                        ).forEach { (label, delta) ->
                            Box(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(3.dp))
                                    .background(TacticalSurfaceVariant)
                                    .border(1.dp, TacticalBorder, RoundedCornerShape(3.dp))
                                    .clickable {
                                        pushUndo()
                                        currentFrame.shift(delta.first, delta.second)
                                        canvasRevision++
                                    }
                                    .padding(horizontal = 6.dp, vertical = 2.dp)
                            ) {
                                Text(label, color = TacticalCyan, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                            }
                        }
                    }

                    // Undo & Redo buttons
                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        Button(
                            onClick = {
                                if (undoStack.isNotEmpty()) {
                                    redoStack.add(currentFrame.pixels.clone())
                                    val prev = undoStack.removeAt(undoStack.size - 1)
                                    System.arraycopy(prev, 0, currentFrame.pixels, 0, prev.size)
                                    canvasRevision++
                                }
                            },
                            enabled = undoStack.isNotEmpty(),
                            colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                            border = androidx.compose.foundation.BorderStroke(1.dp, TacticalBorder),
                            shape = RoundedCornerShape(3.dp),
                            contentPadding = PaddingValues(horizontal = 6.dp, vertical = 2.dp),
                            modifier = Modifier.height(24.dp)
                        ) {
                            Text("UNDO (${undoStack.size})", color = if (undoStack.isNotEmpty()) TacticalCyan else TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                        }

                        Button(
                            onClick = {
                                if (redoStack.isNotEmpty()) {
                                    undoStack.add(currentFrame.pixels.clone())
                                    val next = redoStack.removeAt(redoStack.size - 1)
                                    System.arraycopy(next, 0, currentFrame.pixels, 0, next.size)
                                    canvasRevision++
                                }
                            },
                            enabled = redoStack.isNotEmpty(),
                            colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                            border = androidx.compose.foundation.BorderStroke(1.dp, TacticalBorder),
                            shape = RoundedCornerShape(3.dp),
                            contentPadding = PaddingValues(horizontal = 6.dp, vertical = 2.dp),
                            modifier = Modifier.height(24.dp)
                        ) {
                            Text("REDO (${redoStack.size})", color = if (redoStack.isNotEmpty()) TacticalCyan else TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                        }
                    }
                }
            }
        }

        // 3. Multi-Frame Filmstrip & Timeline Deck
        TacticalCard(title = "MULTI-FRAME TIMELINE & FILMSTRIP", accentColor = TacticalCyan) {
            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                // Filmstrip horizontal thumbnails
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    frames.forEachIndexed { idx, sf ->
                        val isCurrent = idx == safeIndex
                        Box(
                            modifier = Modifier
                                .width(56.dp)
                                .height(38.dp)
                                .clip(RoundedCornerShape(3.dp))
                                .background(PhosphorOff)
                                .border(1.5.dp, if (isCurrent) TacticalAmber else TacticalBorder, RoundedCornerShape(3.dp))
                                .clickable { currentFrameIndex = idx }
                                .padding(2.dp)
                        ) {
                            Canvas(modifier = Modifier.fillMaxSize()) {
                                val pw = size.width / 128f
                                val ph = size.height / 64f
                                for (y in 0 until 64 step 2) {
                                    for (x in 0 until 128 step 2) {
                                        if (sf.getPixel(x, y)) {
                                            drawRect(
                                                color = phosphorColor,
                                                topLeft = Offset(x * pw, y * ph),
                                                size = Size(pw * 1.8f, ph * 1.8f)
                                            )
                                        }
                                    }
                                }
                            }
                            Text(
                                text = "${idx + 1}",
                                color = if (isCurrent) TacticalAmber else TextMuted,
                                fontSize = 6.5.sp,
                                fontFamily = FontFamily.Monospace,
                                fontWeight = FontWeight.Bold,
                                modifier = Modifier.align(Alignment.BottomEnd)
                            )
                        }
                    }
                }

                // Frame Sequence Operations: Add, Clone, Delete, Move
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(4.dp)
                ) {
                    Button(
                        onClick = {
                            val newFrames = frames.toMutableList()
                            newFrames.add(safeIndex + 1, StudioFrame(delayMs = currentFrame.delayMs))
                            frames = newFrames
                            currentFrameIndex = safeIndex + 1
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalCyanDim),
                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalCyan),
                        shape = RoundedCornerShape(3.dp),
                        contentPadding = PaddingValues(horizontal = 6.dp, vertical = 2.dp),
                        modifier = Modifier.weight(1f).height(26.dp)
                    ) {
                        Text("+ ADD", color = TacticalCyan, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    }

                    Button(
                        onClick = {
                            val newFrames = frames.toMutableList()
                            newFrames.add(safeIndex + 1, currentFrame.clone())
                            frames = newFrames
                            currentFrameIndex = safeIndex + 1
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalAmberDim),
                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalAmber),
                        shape = RoundedCornerShape(3.dp),
                        contentPadding = PaddingValues(horizontal = 6.dp, vertical = 2.dp),
                        modifier = Modifier.weight(1f).height(26.dp)
                    ) {
                        Text("CLONE", color = TacticalAmber, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    }

                    Button(
                        onClick = {
                            if (frames.size > 1) {
                                val newFrames = frames.toMutableList()
                                newFrames.removeAt(safeIndex)
                                frames = newFrames
                                currentFrameIndex = safeIndex.coerceAtMost(frames.size - 1)
                            } else {
                                currentFrame.clear()
                            }
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalRedDim),
                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalRed),
                        shape = RoundedCornerShape(3.dp),
                        contentPadding = PaddingValues(horizontal = 6.dp, vertical = 2.dp),
                        modifier = Modifier.weight(1f).height(26.dp)
                    ) {
                        Text("DEL", color = TacticalRed, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    }

                    Button(
                        onClick = {
                            if (safeIndex > 0) {
                                val newFrames = frames.toMutableList()
                                val item = newFrames.removeAt(safeIndex)
                                newFrames.add(safeIndex - 1, item)
                                frames = newFrames
                                currentFrameIndex = safeIndex - 1
                            }
                        },
                        enabled = safeIndex > 0,
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalBorder),
                        shape = RoundedCornerShape(3.dp),
                        contentPadding = PaddingValues(horizontal = 4.dp, vertical = 2.dp),
                        modifier = Modifier.height(26.dp)
                    ) {
                        Text("◀", color = TacticalCyan, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                    }

                    Button(
                        onClick = {
                            if (safeIndex < frames.size - 1) {
                                val newFrames = frames.toMutableList()
                                val item = newFrames.removeAt(safeIndex)
                                newFrames.add(safeIndex + 1, item)
                                frames = newFrames
                                currentFrameIndex = safeIndex + 1
                            }
                        },
                        enabled = safeIndex < frames.size - 1,
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalBorder),
                        shape = RoundedCornerShape(3.dp),
                        contentPadding = PaddingValues(horizontal = 4.dp, vertical = 2.dp),
                        modifier = Modifier.height(26.dp)
                    ) {
                        Text("▶", color = TacticalCyan, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                    }
                }
            }
        }

        // 4. Live OLED Simulation Viewer & Playback Transport
        TacticalCard(title = "LIVE OLED PREVIEW & TRANSPORT CONTROLS", accentColor = TacticalGreen) {
            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                // Phosphor Color Selector
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text("PHOSPHOR GLOW:", color = TextMuted, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        listOf(
                            "AMBER" to PhosphorAmber,
                            "CYAN" to PhosphorCyan,
                            "GREEN" to PhosphorGreen,
                            "WHITE" to Color.White
                        ).forEach { (lbl, col) ->
                            val isSel = phosphorColor == col
                            Box(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(3.dp))
                                    .background(if (isSel) col else TacticalSurfaceVariant)
                                    .border(1.dp, if (isSel) col else TacticalBorder, RoundedCornerShape(3.dp))
                                    .clickable { phosphorColor = col }
                                    .padding(horizontal = 5.dp, vertical = 2.dp)
                            ) {
                                Text(lbl, color = if (isSel) OledBlack else TextMuted, fontSize = 7.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                            }
                        }
                    }
                }

                // Transport Slider (Scrubber)
                Slider(
                    value = safeIndex.toFloat(),
                    onValueChange = { currentFrameIndex = it.toInt() },
                    valueRange = 0f..(frames.size - 1).coerceAtLeast(1).toFloat(),
                    colors = SliderDefaults.colors(thumbColor = TacticalAmber, activeTrackColor = TacticalAmber)
                )

                // Play / Pause & Stepper Buttons
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Button(
                        onClick = { isPlaying = !isPlaying },
                        colors = ButtonDefaults.buttonColors(containerColor = if (isPlaying) TacticalAmberDim else TacticalGreenDim),
                        border = androidx.compose.foundation.BorderStroke(1.dp, if (isPlaying) TacticalAmber else TacticalGreen),
                        shape = RoundedCornerShape(3.dp),
                        modifier = Modifier.weight(1f).height(30.dp)
                    ) {
                        Icon(
                            imageVector = if (isPlaying) Icons.Default.Pause else Icons.Default.PlayArrow,
                            contentDescription = if (isPlaying) "Pause" else "Play",
                            tint = if (isPlaying) TacticalAmber else TacticalGreen,
                            modifier = Modifier.size(16.dp)
                        )
                        Spacer(modifier = Modifier.width(4.dp))
                        Text(if (isPlaying) "PAUSE" else "PLAY", color = if (isPlaying) TacticalAmber else TacticalGreen, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    }

                    Button(
                        onClick = { currentFrameIndex = if (safeIndex > 0) safeIndex - 1 else frames.size - 1 },
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalBorder),
                        shape = RoundedCornerShape(3.dp),
                        modifier = Modifier.height(30.dp)
                    ) {
                        Text("< PREV", color = TacticalCyan, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                    }

                    Button(
                        onClick = { currentFrameIndex = (safeIndex + 1) % frames.size },
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalBorder),
                        shape = RoundedCornerShape(3.dp),
                        modifier = Modifier.height(30.dp)
                    ) {
                        Text("NEXT >", color = TacticalCyan, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                    }
                }

                // Playback FPS Slider
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text("PLAYBACK SPEED: $playbackFps FPS (${1000 / playbackFps}ms)", color = TextMuted, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text("LOOP", color = if (isLooping) TacticalGreen else TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                        Switch(
                            checked = isLooping,
                            onCheckedChange = { isLooping = it },
                            colors = SwitchDefaults.colors(checkedThumbColor = TacticalGreen)
                        )
                    }
                }
            }
        }

        // 5. Binary Animation Packer & Deployment Engine
        TacticalCard(title = "Q-WATCH BINARY PACKER & FLASH DEPLOYMENT", accentColor = TacticalCyan) {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                // Anim Name input
                OutlinedTextField(
                    value = animName,
                    onValueChange = { animName = it.replace(" ", "_").lowercase() },
                    label = { Text("ANIMATION FILENAME", fontSize = 9.sp, fontFamily = FontFamily.Monospace) },
                    suffix = { Text(".anim", color = TacticalCyan, fontFamily = FontFamily.Monospace, fontSize = 9.sp) },
                    singleLine = true,
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedBorderColor = TacticalCyan,
                        unfocusedBorderColor = TacticalBorder,
                        focusedTextColor = TextPrimary,
                        unfocusedTextColor = TextPrimary
                    ),
                    modifier = Modifier.fillMaxWidth()
                )

                val binaryBytes = remember(frames, playbackFps, isLooping, isInverted) {
                    AnimEncoder.encodeStudioFrames(
                        frames = frames,
                        delayMs = (1000 / playbackFps.coerceIn(1, 60)),
                        loop = isLooping,
                        invert = isInverted
                    )
                }

                // File metadata telemetry strip
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(3.dp))
                        .background(TacticalSurfaceLow)
                        .border(1.dp, TacticalBorder, RoundedCornerShape(3.dp))
                        .padding(horizontal = 8.dp, vertical = 6.dp),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text("FRAMES: ${frames.size}", color = TacticalCyan, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    Text("PAYLOAD: ${binaryBytes.size / 1024} KB (${binaryBytes.size} B)", color = TacticalAmber, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    Text("FORMAT: QANM v1", color = TacticalGreen, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }

                // Deployment Progress Indicator
                if (isDeploying) {
                    Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                        Text("STREAMING TO LITTLEFS... ${(deployProgress * 100).toInt()}%", color = TacticalAmber, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                        LinearProgressIndicator(
                            progress = { deployProgress },
                            modifier = Modifier.fillMaxWidth().height(4.dp).clip(RoundedCornerShape(2.dp)),
                            color = TacticalAmber,
                            trackColor = TacticalSurfaceVariant
                        )
                    }
                }

                // Deployment Action Buttons (Anim & Photo)
                Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        // Deploy Animation to /anim/
                        Button(
                            onClick = {
                                val cleanName = if (animName.endsWith(".anim")) animName else "$animName.anim"
                                val remotePath = "/anim/$cleanName"
                                isDeploying = true
                                deployProgress = 0.1f

                                scope.launch {
                                    val res = client.uploadFileWithProgress(remotePath, binaryBytes) { prog ->
                                        deployProgress = prog.progress
                                    }
                                    isDeploying = false
                                    statusMessage = if (res.isSuccess && res.getOrDefault(false)) "Successfully deployed to $remotePath!" else "Failed to deploy to watch!"
                                }
                            },
                            enabled = !isDeploying,
                            colors = ButtonDefaults.buttonColors(containerColor = TacticalCyanDim),
                            border = androidx.compose.foundation.BorderStroke(1.dp, TacticalCyan),
                            shape = RoundedCornerShape(4.dp),
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("DEPLOY ANIM (/anim)", color = TacticalCyan, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                        }

                        // Deploy Photo to /photos/
                        Button(
                            onClick = {
                                val baseName = animName.removeSuffix(".anim").removeSuffix(".bmp")
                                val cleanBmpName = "$baseName.bmp"
                                val remotePath = "/photos/$cleanBmpName"
                                val bmpBytes = AnimEncoder.encodeStudioFrameToBmp(currentFrame, isInverted)
                                isDeploying = true
                                deployProgress = 0.1f

                                scope.launch {
                                    val res = client.uploadFileWithProgress(remotePath, bmpBytes) { prog ->
                                        deployProgress = prog.progress
                                    }
                                    isDeploying = false
                                    statusMessage = if (res.isSuccess && res.getOrDefault(false)) "Photo deployed to $remotePath!" else "Photo deploy failed!"
                                }
                            },
                            enabled = !isDeploying,
                            colors = ButtonDefaults.buttonColors(containerColor = TacticalGreenDim),
                            border = androidx.compose.foundation.BorderStroke(1.dp, TacticalGreen),
                            shape = RoundedCornerShape(4.dp),
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("DEPLOY PHOTO (/photos)", color = TacticalGreen, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                        }
                    }

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        // Set as Boot Animation
                        Button(
                            onClick = {
                                isDeploying = true
                                deployProgress = 0.1f
                                scope.launch {
                                    val res = client.uploadFileWithProgress("/boot/boot.anim", binaryBytes) { prog ->
                                        deployProgress = prog.progress
                                    }
                                    isDeploying = false
                                    statusMessage = if (res.isSuccess && res.getOrDefault(false)) "Installed as /boot/boot.anim!" else "Boot deploy failed!"
                                }
                            },
                            enabled = !isDeploying,
                            colors = ButtonDefaults.buttonColors(containerColor = TacticalAmberDim),
                            border = androidx.compose.foundation.BorderStroke(1.dp, TacticalAmber),
                            shape = RoundedCornerShape(4.dp),
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("BOOT ANIM", color = TacticalAmber, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                        }

                        // Set as Boot Photo Splash
                        Button(
                            onClick = {
                                val bmpBytes = AnimEncoder.encodeStudioFrameToBmp(currentFrame, isInverted)
                                isDeploying = true
                                deployProgress = 0.1f
                                scope.launch {
                                    val res = client.uploadFileWithProgress("/boot/boot.bmp", bmpBytes) { prog ->
                                        deployProgress = prog.progress
                                    }
                                    isDeploying = false
                                    statusMessage = if (res.isSuccess && res.getOrDefault(false)) "Saved as /boot/boot.bmp splash!" else "Boot BMP deploy failed!"
                                }
                            },
                            enabled = !isDeploying,
                            colors = ButtonDefaults.buttonColors(containerColor = TacticalAmberDim),
                            border = androidx.compose.foundation.BorderStroke(1.dp, TacticalAmber),
                            shape = RoundedCornerShape(4.dp),
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("BOOT PHOTO", color = TacticalAmber, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                        }

                        // Save File
                        Button(
                            onClick = {
                                try {
                                    val cleanName = if (animName.endsWith(".anim")) animName else "$animName.anim"
                                    val outFile = File(context.filesDir, cleanName)
                                    FileOutputStream(outFile).use { it.write(binaryBytes) }

                                    val bmpName = animName.removeSuffix(".anim") + ".bmp"
                                    val bmpFile = File(context.filesDir, bmpName)
                                    val bmpBytes = AnimEncoder.encodeStudioFrameToBmp(currentFrame, isInverted)
                                    FileOutputStream(bmpFile).use { it.write(bmpBytes) }

                                    statusMessage = "Saved $cleanName & $bmpName to app storage!"
                                } catch (e: Exception) {
                                    statusMessage = "Export failed: ${e.message}"
                                }
                            },
                            colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                            border = androidx.compose.foundation.BorderStroke(1.dp, TacticalBorder),
                            shape = RoundedCornerShape(4.dp),
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("SAVE FILES", color = TextPrimary, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                        }
                    }
                }
            }
        }
    }

    // =========================================================================
    // FULL-FLEDGED ANIMATED GIF IMPORT STUDIO DIALOG
    // =========================================================================
    if (showGifImportDialog && pendingGifFrames.isNotEmpty()) {
        val gifDialogScrollState = rememberScrollState()
        val previewOledBitmap = remember { Bitmap.createBitmap(128, 64, Bitmap.Config.ARGB_8888) }
        val previewPixelBuffer = remember { IntArray(128 * 64) }
        var previewRendered by remember { mutableStateOf<Bitmap?>(null) }

        // Live preview animation loop
        LaunchedEffect(gifPreviewPlaying, pendingGifFrames.size, gifFpsOverride) {
            if (gifPreviewPlaying && pendingGifFrames.isNotEmpty()) {
                while (isActive && gifPreviewPlaying) {
                    val stepDelay = if (gifFpsOverride > 0) (1000 / gifFpsOverride) else pendingGifFrames[gifPreviewIndex.coerceIn(pendingGifFrames.indices)].second
                    delay(stepDelay.coerceAtLeast(20).toLong())
                    gifPreviewIndex = (gifPreviewIndex + 1) % pendingGifFrames.size
                }
            }
        }

        // Recompute 1-bit quantized preview frame
        LaunchedEffect(gifPreviewIndex, gifDitherMode, gifFitMode, gifBrightness, gifContrast, gifThreshold, gifInvert) {
            if (pendingGifFrames.isNotEmpty()) {
                val rawBmp = pendingGifFrames[gifPreviewIndex.coerceIn(pendingGifFrames.indices)].first
                val sf = AnimEncoder.decodeBitmapToStudioFrame(
                    src = rawBmp,
                    ditherMode = gifDitherMode,
                    threshold = gifThreshold,
                    invert = gifInvert,
                    fitMode = gifFitMode,
                    brightness = gifBrightness,
                    contrast = gifContrast
                )
                val active = phosphorColor.toArgb()
                val off = 0xFF040608.toInt()
                for (y in 0 until 64) {
                    val row = y * 128
                    for (x in 0 until 128) {
                        previewPixelBuffer[row + x] = if (sf.getPixel(x, y)) active else off
                    }
                }
                previewOledBitmap.setPixels(previewPixelBuffer, 0, 128, 0, 0, 128, 64)
                previewRendered = previewOledBitmap
            }
        }

        AlertDialog(
            onDismissRequest = {
                if (!isProcessingGif) {
                    showGifImportDialog = false
                    pendingGifFrames = emptyList()
                }
            },
            containerColor = TacticalSurface,
            title = {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column {
                        Text(
                            text = "GIF IMPORT & QUANTIZATION STUDIO",
                            color = TacticalAmber,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold,
                            fontSize = 11.sp
                        )
                        Text(
                            text = "${pendingGifFrames.size} FRAMES · ${pendingGifFileName}.gif",
                            color = TextMuted,
                            fontFamily = FontFamily.Monospace,
                            fontSize = 8.sp
                        )
                    }
                }
            },
            text = {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(max = 420.dp)
                        .verticalScroll(gifDialogScrollState),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    // 1. Live 128x64 OLED Animated Preview
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .aspectRatio(128f / 64f)
                            .clip(RoundedCornerShape(4.dp))
                            .background(Color(0xFF040608))
                            .border(1.5.dp, TacticalBorder, RoundedCornerShape(4.dp)),
                        contentAlignment = Alignment.Center
                    ) {
                        val bmp = previewRendered
                        if (bmp != null) {
                            Canvas(modifier = Modifier.fillMaxSize()) {
                                drawImage(
                                    image = bmp.asImageBitmap(),
                                    dstSize = IntSize(size.width.toInt(), size.height.toInt()),
                                    filterQuality = FilterQuality.None
                                )
                            }
                        }
                    }

                    // Scrubber & Playback Controls
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                            IconButton(
                                onClick = { gifPreviewPlaying = !gifPreviewPlaying },
                                modifier = Modifier.size(24.dp)
                            ) {
                                Icon(
                                    imageVector = if (gifPreviewPlaying) Icons.Default.Pause else Icons.Default.PlayArrow,
                                    contentDescription = "Toggle Preview",
                                    tint = TacticalCyan,
                                    modifier = Modifier.size(16.dp)
                                )
                            }
                            Text(
                                text = "FRAME ${gifPreviewIndex + 1}/${pendingGifFrames.size}",
                                color = TacticalCyan,
                                fontSize = 8.sp,
                                fontFamily = FontFamily.Monospace,
                                fontWeight = FontWeight.Bold
                            )
                        }

                        val estFrames = if (gifSubsample == 2) ((pendingGifFrames.size + 1) / 2) else pendingGifFrames.size
                        val estSizeKb = (AnimParser.ANIM_HEADER_SIZE + (estFrames * AnimParser.ANIM_FRAME_BYTES)) / 1024
                        Text(
                            text = "~$estSizeKb KB ($estFrames fr)",
                            color = TacticalGreen,
                            fontSize = 7.5.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }

                    Slider(
                        value = gifPreviewIndex.toFloat(),
                        onValueChange = {
                            gifPreviewPlaying = false
                            gifPreviewIndex = it.toInt().coerceIn(0, (pendingGifFrames.size - 1).coerceAtLeast(0))
                        },
                        valueRange = 0f..(pendingGifFrames.size - 1).coerceAtLeast(1).toFloat(),
                        colors = SliderDefaults.colors(thumbColor = TacticalCyan, activeTrackColor = TacticalCyan),
                        modifier = Modifier.height(20.dp)
                    )

                    // 2. Dithering Algorithm Chips
                    Text("DITHERING ALGORITHM:", color = TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                    Row(
                        modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                        horizontalArrangement = Arrangement.spacedBy(4.dp)
                    ) {
                        DitherMode.values().forEach { dm ->
                            val isSel = gifDitherMode == dm
                            Box(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(3.dp))
                                    .background(if (isSel) TacticalAmber else TacticalSurfaceVariant)
                                    .border(1.dp, if (isSel) TacticalAmber else TacticalBorder, RoundedCornerShape(3.dp))
                                    .clickable { gifDitherMode = dm }
                                    .padding(horizontal = 6.dp, vertical = 3.dp)
                            ) {
                                Text(dm.label, color = if (isSel) OledBlack else TextPrimary, fontSize = 7.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                            }
                        }
                    }

                    // 3. Aspect Ratio / Framing (Fill Screen vs Fit vs Stretch)
                    Text("GEOMETRY & SCREEN FRAMING:", color = TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                    Row(
                        modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                        horizontalArrangement = Arrangement.spacedBy(4.dp)
                    ) {
                        AspectFitMode.values().forEach { fm ->
                            val isSel = gifFitMode == fm
                            Box(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(3.dp))
                                    .background(if (isSel) TacticalCyan else TacticalSurfaceVariant)
                                    .border(1.dp, if (isSel) TacticalCyan else TacticalBorder, RoundedCornerShape(3.dp))
                                    .clickable { gifFitMode = fm }
                                    .padding(horizontal = 6.dp, vertical = 3.dp)
                            ) {
                                Text(
                                    text = when (fm) {
                                        AspectFitMode.CROP -> "FILL SCREEN (CROP)"
                                        AspectFitMode.FIT -> "LETTERBOX (FIT)"
                                        AspectFitMode.STRETCH -> "STRETCH"
                                    },
                                    color = if (isSel) OledBlack else TextPrimary,
                                    fontSize = 7.sp,
                                    fontFamily = FontFamily.Monospace,
                                    fontWeight = FontWeight.Bold
                                )
                            }
                        }
                    }

                    // 4. Threshold Slider
                    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                        Text("THRESHOLD", color = TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                        Text("${gifThreshold.toInt()}", color = TacticalGreen, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    }
                    Slider(
                        value = gifThreshold,
                        onValueChange = { gifThreshold = it },
                        valueRange = 0f..255f,
                        colors = SliderDefaults.colors(thumbColor = TacticalGreen, activeTrackColor = TacticalGreen),
                        modifier = Modifier.height(20.dp)
                    )

                    // 5. Brightness Slider
                    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                        Text("BRIGHTNESS", color = TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                        Text("${gifBrightness.toInt()}", color = TacticalCyan, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    }
                    Slider(
                        value = gifBrightness,
                        onValueChange = { gifBrightness = it },
                        valueRange = -100f..100f,
                        colors = SliderDefaults.colors(thumbColor = TacticalCyan, activeTrackColor = TacticalCyan),
                        modifier = Modifier.height(20.dp)
                    )

                    // 6. Contrast Slider
                    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                        Text("CONTRAST", color = TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                        Text("${gifContrast.toInt()}", color = TacticalAmber, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    }
                    Slider(
                        value = gifContrast,
                        onValueChange = { gifContrast = it },
                        valueRange = -100f..100f,
                        colors = SliderDefaults.colors(thumbColor = TacticalAmber, activeTrackColor = TacticalAmber),
                        modifier = Modifier.height(20.dp)
                    )

                    // 7. Invert Colors Switch
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text("INVERT COLORS", color = TextPrimary, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                        Switch(
                            checked = gifInvert,
                            onCheckedChange = { gifInvert = it },
                            colors = SwitchDefaults.colors(checkedThumbColor = TacticalAmber),
                            modifier = Modifier.height(20.dp)
                        )
                    }

                    // 8. FPS / Frame Delay Override
                    Text("TIMING / FPS OVERRIDE:", color = TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                    Row(
                        modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                        horizontalArrangement = Arrangement.spacedBy(4.dp)
                    ) {
                        listOf(
                            0 to "ORIGINAL",
                            20 to "20 FPS (50ms)",
                            15 to "15 FPS (66ms)",
                            10 to "10 FPS (100ms)",
                            30 to "30 FPS (33ms)"
                        ).forEach { (fps, lbl) ->
                            val isSel = gifFpsOverride == fps
                            Box(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(3.dp))
                                    .background(if (isSel) TacticalCyan else TacticalSurfaceVariant)
                                    .border(1.dp, if (isSel) TacticalCyan else TacticalBorder, RoundedCornerShape(3.dp))
                                    .clickable { gifFpsOverride = fps }
                                    .padding(horizontal = 6.dp, vertical = 2.dp)
                            ) {
                                Text(lbl, color = if (isSel) OledBlack else TextPrimary, fontSize = 7.sp, fontFamily = FontFamily.Monospace)
                            }
                        }
                    }

                    // 9. Frame Subsampling
                    Text("FRAME SUBSAMPLING:", color = TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        listOf(1 to "ALL FRAMES", 2 to "SKIP EVERY 2ND (1/2 SIZE)").forEach { (sub, lbl) ->
                            val isSel = gifSubsample == sub
                            Box(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(3.dp))
                                    .background(if (isSel) TacticalAmber else TacticalSurfaceVariant)
                                    .border(1.dp, if (isSel) TacticalAmber else TacticalBorder, RoundedCornerShape(3.dp))
                                    .clickable { gifSubsample = sub }
                                    .padding(horizontal = 6.dp, vertical = 2.dp)
                            ) {
                                Text(lbl, color = if (isSel) OledBlack else TextPrimary, fontSize = 7.sp, fontFamily = FontFamily.Monospace)
                            }
                        }
                    }

                    if (isProcessingGif) {
                        Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                            LinearProgressIndicator(modifier = Modifier.fillMaxWidth(), color = TacticalAmber)
                            Text("Quantizing all frames with $gifDitherMode...", color = TacticalAmber, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                        }
                    }
                }
            },
            confirmButton = {
                Button(
                    onClick = {
                        scope.launch {
                            isProcessingGif = true
                            withContext(Dispatchers.Default) {
                                val step = if (gifSubsample == 2) 2 else 1
                                val selectedFrames = pendingGifFrames.filterIndexed { idx, _ -> idx % step == 0 }
                                val converted = selectedFrames.map { (bmp, delay) ->
                                    val finalDelay = if (gifFpsOverride > 0) (1000 / gifFpsOverride) else delay
                                    AnimEncoder.decodeBitmapToStudioFrame(
                                        src = bmp,
                                        ditherMode = gifDitherMode,
                                        threshold = gifThreshold,
                                        invert = gifInvert,
                                        fitMode = gifFitMode,
                                        brightness = gifBrightness,
                                        contrast = gifContrast
                                    ).copy(delayMs = finalDelay)
                                }
                                frames = converted.toMutableList()
                                currentFrameIndex = 0
                                animName = pendingGifFileName
                                canvasRevision++
                            }
                            isProcessingGif = false
                            showGifImportDialog = false
                            pendingGifFrames = emptyList()
                            statusMessage = "Imported ${frames.size} frames from GIF!"
                        }
                    },
                    enabled = !isProcessingGif,
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalAmberDim),
                    border = androidx.compose.foundation.BorderStroke(1.dp, TacticalAmber)
                ) {
                    Text("IMPORT ANIMATION", color = TacticalAmber, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }
            },
            dismissButton = {
                TextButton(
                    onClick = {
                        showGifImportDialog = false
                        pendingGifFrames = emptyList()
                    },
                    enabled = !isProcessingGif
                ) {
                    Text("CANCEL", color = TextMuted, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                }
            }
        )
    }

    // =========================================================================
    // ENHANCED STATIC IMAGE IMPORT & QUANTIZATION DIALOG
    // =========================================================================
    if (showImportDialog && pendingImportBitmap != null) {
        val bmp = pendingImportBitmap!!
        val previewOledBitmap = remember { Bitmap.createBitmap(128, 64, Bitmap.Config.ARGB_8888) }
        val previewPixelBuffer = remember { IntArray(128 * 64) }
        var imagePreviewRendered by remember { mutableStateOf<Bitmap?>(null) }

        LaunchedEffect(importDitherMode, importFitMode, importBrightness, importContrast, importThreshold, importInvert) {
            val sf = AnimEncoder.decodeBitmapToStudioFrame(
                src = bmp,
                ditherMode = importDitherMode,
                threshold = importThreshold,
                invert = importInvert,
                fitMode = importFitMode,
                brightness = importBrightness,
                contrast = importContrast
            )
            val active = phosphorColor.toArgb()
            val off = 0xFF040608.toInt()
            for (y in 0 until 64) {
                val row = y * 128
                for (x in 0 until 128) {
                    previewPixelBuffer[row + x] = if (sf.getPixel(x, y)) active else off
                }
            }
            previewOledBitmap.setPixels(previewPixelBuffer, 0, 128, 0, 0, 128, 64)
            imagePreviewRendered = previewOledBitmap
        }

        AlertDialog(
            onDismissRequest = {
                showImportDialog = false
                pendingImportBitmap = null
            },
            containerColor = TacticalSurface,
            title = {
                Text("IMAGE IMPORT & 1-BIT QUANTIZER", color = TacticalCyan, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold, fontSize = 11.sp)
            },
            text = {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(max = 400.dp)
                        .verticalScroll(rememberScrollState()),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    // 1. Live 128x64 OLED Preview
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .aspectRatio(128f / 64f)
                            .clip(RoundedCornerShape(4.dp))
                            .background(Color(0xFF040608))
                            .border(1.5.dp, TacticalBorder, RoundedCornerShape(4.dp)),
                        contentAlignment = Alignment.Center
                    ) {
                        val img = imagePreviewRendered
                        if (img != null) {
                            Canvas(modifier = Modifier.fillMaxSize()) {
                                drawImage(
                                    image = img.asImageBitmap(),
                                    dstSize = IntSize(size.width.toInt(), size.height.toInt()),
                                    filterQuality = FilterQuality.None
                                )
                            }
                        }
                    }

                    // 2. Dithering Mode Chips
                    Text("DITHERING ALGORITHM:", color = TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                    Row(
                        modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                        horizontalArrangement = Arrangement.spacedBy(4.dp)
                    ) {
                        DitherMode.values().forEach { dm ->
                            val isSel = importDitherMode == dm
                            Box(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(3.dp))
                                    .background(if (isSel) TacticalCyan else TacticalSurfaceVariant)
                                    .border(1.dp, if (isSel) TacticalCyan else TacticalBorder, RoundedCornerShape(3.dp))
                                    .clickable { importDitherMode = dm }
                                    .padding(horizontal = 6.dp, vertical = 3.dp)
                            ) {
                                Text(dm.label, color = if (isSel) OledBlack else TextPrimary, fontSize = 7.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                            }
                        }
                    }

                    // 3. Screen Framing (Fill Screen / Crop vs Fit vs Stretch)
                    Text("GEOMETRY & SCREEN FRAMING:", color = TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                    Row(
                        modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                        horizontalArrangement = Arrangement.spacedBy(4.dp)
                    ) {
                        AspectFitMode.values().forEach { fm ->
                            val isSel = importFitMode == fm
                            Box(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(3.dp))
                                    .background(if (isSel) TacticalAmber else TacticalSurfaceVariant)
                                    .border(1.dp, if (isSel) TacticalAmber else TacticalBorder, RoundedCornerShape(3.dp))
                                    .clickable { importFitMode = fm }
                                    .padding(horizontal = 6.dp, vertical = 3.dp)
                            ) {
                                Text(
                                    text = when (fm) {
                                        AspectFitMode.CROP -> "FILL SCREEN (CROP)"
                                        AspectFitMode.FIT -> "LETTERBOX (FIT)"
                                        AspectFitMode.STRETCH -> "STRETCH"
                                    },
                                    color = if (isSel) OledBlack else TextPrimary,
                                    fontSize = 7.sp,
                                    fontFamily = FontFamily.Monospace,
                                    fontWeight = FontWeight.Bold
                                )
                            }
                        }
                    }

                    // 4. Threshold Slider
                    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                        Text("THRESHOLD", color = TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                        Text("${importThreshold.toInt()}", color = TacticalGreen, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    }
                    Slider(
                        value = importThreshold,
                        onValueChange = { importThreshold = it },
                        valueRange = 0f..255f,
                        colors = SliderDefaults.colors(thumbColor = TacticalGreen, activeTrackColor = TacticalGreen),
                        modifier = Modifier.height(20.dp)
                    )

                    // 5. Brightness Slider
                    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                        Text("BRIGHTNESS", color = TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                        Text("${importBrightness.toInt()}", color = TacticalCyan, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    }
                    Slider(
                        value = importBrightness,
                        onValueChange = { importBrightness = it },
                        valueRange = -100f..100f,
                        colors = SliderDefaults.colors(thumbColor = TacticalCyan, activeTrackColor = TacticalCyan),
                        modifier = Modifier.height(20.dp)
                    )

                    // 6. Contrast Slider
                    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                        Text("CONTRAST", color = TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                        Text("${importContrast.toInt()}", color = TacticalAmber, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    }
                    Slider(
                        value = importContrast,
                        onValueChange = { importContrast = it },
                        valueRange = -100f..100f,
                        colors = SliderDefaults.colors(thumbColor = TacticalAmber, activeTrackColor = TacticalAmber),
                        modifier = Modifier.height(20.dp)
                    )

                    // 7. Invert Colors Switch
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text("INVERT COLORS", color = TextPrimary, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                        Switch(
                            checked = importInvert,
                            onCheckedChange = { importInvert = it },
                            colors = SwitchDefaults.colors(checkedThumbColor = TacticalAmber),
                            modifier = Modifier.height(20.dp)
                        )
                    }
                }
            },
            confirmButton = {
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    Button(
                        onClick = {
                            val sf = AnimEncoder.decodeBitmapToStudioFrame(
                                src = bmp,
                                ditherMode = importDitherMode,
                                threshold = importThreshold,
                                invert = importInvert,
                                fitMode = importFitMode,
                                brightness = importBrightness,
                                contrast = importContrast
                            )
                            val newFrames = frames.toMutableList()
                            newFrames.add(safeIndex + 1, sf)
                            frames = newFrames
                            currentFrameIndex = safeIndex + 1
                            showImportDialog = false
                            pendingImportBitmap = null
                            canvasRevision++
                            statusMessage = "Added new frame!"
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalCyanDim),
                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalCyan)
                    ) {
                        Text("+ ADD FRAME", color = TacticalCyan, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    }

                    Button(
                        onClick = {
                            pushUndo()
                            val sf = AnimEncoder.decodeBitmapToStudioFrame(
                                src = bmp,
                                ditherMode = importDitherMode,
                                threshold = importThreshold,
                                invert = importInvert,
                                fitMode = importFitMode,
                                brightness = importBrightness,
                                contrast = importContrast
                            )
                            currentFrame.clear()
                            for (i in sf.pixels.indices) currentFrame.pixels[i] = sf.pixels[i]
                            showImportDialog = false
                            pendingImportBitmap = null
                            canvasRevision++
                            statusMessage = "Replaced current frame!"
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalAmberDim),
                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalAmber)
                    ) {
                        Text("REPLACE", color = TacticalAmber, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    }
                }
            },
            dismissButton = {
                TextButton(onClick = {
                    showImportDialog = false
                    pendingImportBitmap = null
                }) {
                    Text("CANCEL", color = TextMuted, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                }
            }
        )
    }

    // =========================================================================
    // PRESETS GENERATORS DIALOG
    // =========================================================================
    if (showPresetsDialog) {
        AlertDialog(
            onDismissRequest = { showPresetsDialog = false },
            containerColor = TacticalSurface,
            title = {
                Text("LOAD PROCEDURAL TACTICAL PRESET", color = TacticalAmber, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold, fontSize = 12.sp)
            },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Text("Select a procedural tactical animation preset to load and paint on:", color = TextMuted, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)

                    listOf(
                        "RADAR 360 SWEEP" to { AnimEncoder.generateRadarFrames(24, 50) },
                        "007 GUNBARREL SPIRAL" to { AnimEncoder.generateGunbarrelFrames(20, 50) },
                        "MATRIX DIGITAL RAIN" to { AnimEncoder.generateMatrixFrames(24, 50) },
                        "ECG CARDIAC PULSE" to { AnimEncoder.generateEcgFrames(24, 50) },
                        "TACTICAL LOCK-ON" to { AnimEncoder.generateLockonFrames(20, 50) }
                    ).forEach { (label, gen) ->
                        Button(
                            onClick = {
                                val generated = gen()
                                frames = generated.toMutableList()
                                currentFrameIndex = 0
                                animName = label.lowercase().replace(" ", "_")
                                showPresetsDialog = false
                                statusMessage = "Loaded $label (${frames.size} frames)"
                            },
                            colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                            border = androidx.compose.foundation.BorderStroke(1.dp, TacticalBorder),
                            shape = RoundedCornerShape(3.dp),
                            modifier = Modifier.fillMaxWidth().height(32.dp)
                        ) {
                            Text(label, color = TacticalAmber, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                        }
                    }
                }
            },
            confirmButton = {},
            dismissButton = {
                TextButton(onClick = { showPresetsDialog = false }) {
                    Text("CLOSE", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                }
            }
        )
    }
}
