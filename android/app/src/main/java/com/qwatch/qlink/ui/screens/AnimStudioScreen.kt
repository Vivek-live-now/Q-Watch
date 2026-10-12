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
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.anim.*
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.ui.components.TacticalCard
import com.qwatch.qlink.ui.theme.*
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
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

    // Line / Box shape start coordinates
    var shapeStartPos by remember { mutableStateOf<Pair<Int, Int>?>(null) }

    // Undo / Redo History Stacks (Per-Frame)
    val undoStack = remember { mutableStateListOf<BooleanArray>() }
    val redoStack = remember { mutableStateListOf<BooleanArray>() }

    // Deployment & Status State
    var isDeploying by remember { mutableStateOf(false) }
    var deployProgress by remember { mutableFloatStateOf(0f) }
    var statusMessage by remember { mutableStateOf<String?>(null) }

    // Import Dialog State
    var showImportDialog by remember { mutableStateOf(false) }
    var pendingImportBitmap by remember { mutableStateOf<Bitmap?>(null) }
    var importDitherMode by remember { mutableStateOf(DitherMode.FLOYD_STEINBERG) }
    var importThreshold by remember { mutableFloatStateOf(128f) }
    var importInvert by remember { mutableStateOf(false) }
    var importFitMode by remember { mutableStateOf(AspectFitMode.FIT) }

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

    // Media File Picker (GIF, PNG, JPG, .anim)
    val filePickerLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        if (uri != null) {
            try {
                context.contentResolver.openInputStream(uri)?.use { stream ->
                    val fileName = uri.lastPathSegment?.substringAfterLast('/') ?: "imported"
                    if (fileName.endsWith(".anim")) {
                        val bytes = stream.readBytes()
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
                            statusMessage = "Loaded ${frames.size} frames from $fileName"
                        } else {
                            statusMessage = "Invalid .anim binary file!"
                        }
                    } else if (fileName.endsWith(".gif", ignoreCase = true)) {
                        val gifFrames = AnimEncoder.decodeGif(stream)
                        if (gifFrames.isNotEmpty()) {
                            val newFrames = gifFrames.map { (bmp, delay) ->
                                AnimEncoder.decodeBitmapToStudioFrame(
                                    src = bmp,
                                    ditherMode = importDitherMode,
                                    threshold = importThreshold,
                                    invert = importInvert,
                                    fitMode = importFitMode
                                ).copy(delayMs = delay)
                            }
                            frames = newFrames.toMutableList()
                            currentFrameIndex = 0
                            animName = fileName.substringBeforeLast('.')
                            statusMessage = "Extracted ${frames.size} frames from GIF"
                        } else {
                            statusMessage = "Failed to extract GIF frames!"
                        }
                    } else {
                        // Static image
                        val bmp = BitmapFactory.decodeStream(stream)
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
                        // Zoom cycle button
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(3.dp))
                                .background(TacticalSurfaceVariant)
                                .border(1.dp, TacticalBorder, RoundedCornerShape(3.dp))
                                .clickable {
                                    zoomLevel = when (zoomLevel) {
                                        1f -> 2f
                                        2f -> 3.5f
                                        else -> 1f
                                    }
                                }
                                .padding(horizontal = 5.dp, vertical = 2.dp)
                        ) {
                            Text(
                                text = "ZOOM ${zoomLevel.toInt()}X",
                                color = TacticalCyan,
                                fontSize = 7.5.sp,
                                fontFamily = FontFamily.Monospace,
                                fontWeight = FontWeight.Bold
                            )
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
                                text = if (showGrid) "GRID ON" else "GRID OFF",
                                color = if (showGrid) TacticalCyan else TextMuted,
                                fontSize = 7.5.sp,
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
                                text = if (onionSkin) "ONION ON" else "ONION OFF",
                                color = if (onionSkin) TacticalGreen else TextMuted,
                                fontSize = 7.5.sp,
                                fontFamily = FontFamily.Monospace,
                                fontWeight = FontWeight.Bold
                            )
                        }
                    }
                }

                // Interactive Drawing Canvas (128x64)
                val prevFrame = if (onionSkin && safeIndex > 0) frames[safeIndex - 1] else null
                val canvasScrollState = rememberScrollState()

                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(180.dp)
                        .clip(RoundedCornerShape(4.dp))
                        .background(PhosphorOff)
                        .border(1.5.dp, TacticalBorder, RoundedCornerShape(4.dp))
                        .horizontalScroll(canvasScrollState)
                ) {
                    val displayWidth = 128f * (if (zoomLevel > 1f) zoomLevel * 3.5f else 2.6f)
                    val displayHeight = 64f * (if (zoomLevel > 1f) zoomLevel * 3.5f else 2.6f)

                    Canvas(
                        modifier = Modifier
                            .size(displayWidth.dp, displayHeight.dp)
                            .pointerInput(selectedTool, safeIndex) {
                                detectTapGestures { offset ->
                                    val px = (offset.x / size.width * 128).toInt().coerceIn(0, 127)
                                    val py = (offset.y / size.height * 64).toInt().coerceIn(0, 63)
                                    pushUndo()

                                    when (selectedTool) {
                                        DrawTool.PENCIL -> currentFrame.setPixel(px, py, true)
                                        DrawTool.ERASER -> currentFrame.setPixel(px, py, false)
                                        DrawTool.LINE -> {
                                            val start = shapeStartPos
                                            if (start == null) {
                                                shapeStartPos = px to py
                                            } else {
                                                currentFrame.drawLine(start.first, start.second, px, py, true)
                                                shapeStartPos = null
                                            }
                                        }
                                        DrawTool.RECT -> {
                                            val start = shapeStartPos
                                            if (start == null) {
                                                shapeStartPos = px to py
                                            } else {
                                                currentFrame.drawRect(start.first, start.second, px, py, fill = false, value = true)
                                                shapeStartPos = null
                                            }
                                        }
                                        DrawTool.FILL_RECT -> {
                                            val start = shapeStartPos
                                            if (start == null) {
                                                shapeStartPos = px to py
                                            } else {
                                                currentFrame.drawRect(start.first, start.second, px, py, fill = true, value = true)
                                                shapeStartPos = null
                                            }
                                        }
                                        DrawTool.CIRCLE -> {
                                            val start = shapeStartPos
                                            if (start == null) {
                                                shapeStartPos = px to py
                                            } else {
                                                val radius = kotlin.math.max(
                                                    kotlin.math.abs(px - start.first),
                                                    kotlin.math.abs(py - start.second)
                                                )
                                                currentFrame.drawCircle(start.first, start.second, radius, true)
                                                shapeStartPos = null
                                            }
                                        }
                                        DrawTool.INVERT -> currentFrame.invert()
                                        DrawTool.CLEAR -> currentFrame.clear()
                                        DrawTool.FILL -> currentFrame.fillAll()
                                    }
                                }
                            }
                            .pointerInput(selectedTool, safeIndex) {
                                detectDragGestures { change, _ ->
                                    change.consume()
                                    val px = (change.position.x / size.width * 128).toInt().coerceIn(0, 127)
                                    val py = (change.position.y / size.height * 64).toInt().coerceIn(0, 63)
                                    if (selectedTool == DrawTool.PENCIL) {
                                        currentFrame.setPixel(px, py, true)
                                    } else if (selectedTool == DrawTool.ERASER) {
                                        currentFrame.setPixel(px, py, false)
                                    }
                                }
                            }
                    ) {
                        val pixelW = size.width / 128f
                        val pixelH = size.height / 64f

                        // 1. Onion Skin (Previous frame faint overlay)
                        if (prevFrame != null) {
                            val onionColor = phosphorColor.copy(alpha = 0.22f)
                            for (y in 0 until 64) {
                                for (x in 0 until 128) {
                                    if (prevFrame.getPixel(x, y)) {
                                        drawRect(
                                            color = onionColor,
                                            topLeft = Offset(x * pixelW, y * pixelH),
                                            size = Size(pixelW * 0.9f, pixelH * 0.9f)
                                        )
                                    }
                                }
                            }
                        }

                        // 2. Active Frame Pixels
                        for (y in 0 until 64) {
                            for (x in 0 until 128) {
                                if (currentFrame.getPixel(x, y)) {
                                    drawRect(
                                        color = phosphorColor,
                                        topLeft = Offset(x * pixelW, y * pixelH),
                                        size = Size(pixelW * 0.92f, pixelH * 0.92f)
                                    )
                                }
                            }
                        }

                        // 3. Grid lines when zoomed
                        if (showGrid && zoomLevel > 1f) {
                            val gridColor = Color.White.copy(alpha = 0.08f)
                            for (x in 0..128) {
                                drawLine(
                                    color = gridColor,
                                    start = Offset(x * pixelW, 0f),
                                    end = Offset(x * pixelW, size.height),
                                    strokeWidth = 0.5f
                                )
                            }
                            for (y in 0..64) {
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

                // 3. Drawing Tools Strip
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
                                    shapeStartPos = null
                                    if (tool == DrawTool.INVERT) {
                                        pushUndo()
                                        currentFrame.invert()
                                    } else if (tool == DrawTool.CLEAR) {
                                        pushUndo()
                                        currentFrame.clear()
                                    } else if (tool == DrawTool.FILL) {
                                        pushUndo()
                                        currentFrame.fillAll()
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

                // Deployment Action Buttons
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    // Deploy to Watch
                    Button(
                        onClick = {
                            val cleanName = if (animName.endsWith(".anim")) animName else "$animName.anim"
                            val remotePath = "/mochi/$cleanName"
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
                        Text("DEPLOY TO WATCH", color = TacticalCyan, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    }

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
                        Text("SET AS BOOT", color = TacticalAmber, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    }

                    // Save Local File
                    Button(
                        onClick = {
                            try {
                                val cleanName = if (animName.endsWith(".anim")) animName else "$animName.anim"
                                val outFile = File(context.filesDir, cleanName)
                                FileOutputStream(outFile).use { it.write(binaryBytes) }
                                statusMessage = "Saved to ${outFile.name} (${outFile.length() / 1024} KB)"
                            } catch (e: Exception) {
                                statusMessage = "Export failed: ${e.message}"
                            }
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalBorder),
                        shape = RoundedCornerShape(4.dp),
                        modifier = Modifier.weight(1f)
                    ) {
                        Text("SAVE FILE", color = TextPrimary, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                    }
                }
            }
        }
    }

    // =========================================================================
    // IMPORT FINE-TUNING DIALOG
    // =========================================================================
    if (showImportDialog && pendingImportBitmap != null) {
        val bmp = pendingImportBitmap!!
        AlertDialog(
            onDismissRequest = { showImportDialog = false },
            containerColor = TacticalSurface,
            title = {
                Text("IMPORT & QUANTIZE IMAGE", color = TacticalCyan, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold, fontSize = 12.sp)
            },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("Select 1-bit dithering algorithm and geometry:", color = TextMuted, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)

                    // Dithering Mode Chips
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
                                    .clickable { importDitherMode = dm }
                                    .padding(horizontal = 6.dp, vertical = 3.dp)
                            ) {
                                Text(dm.label, color = if (isSel) OledBlack else TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                            }
                        }
                    }

                    // Threshold Slider
                    Column {
                        Text("THRESHOLD: ${importThreshold.toInt()}", color = TextMuted, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                        Slider(
                            value = importThreshold,
                            onValueChange = { importThreshold = it },
                            valueRange = 0f..255f,
                            colors = SliderDefaults.colors(thumbColor = TacticalCyan, activeTrackColor = TacticalCyan)
                        )
                    }

                    // Invert Colors Switch
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text("INVERT COLORS", color = TextPrimary, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                        Switch(
                            checked = importInvert,
                            onCheckedChange = { importInvert = it },
                            colors = SwitchDefaults.colors(checkedThumbColor = TacticalAmber)
                        )
                    }
                }
            },
            confirmButton = {
                Button(
                    onClick = {
                        val sf = AnimEncoder.decodeBitmapToStudioFrame(
                            src = bmp,
                            ditherMode = importDitherMode,
                            threshold = importThreshold,
                            invert = importInvert,
                            fitMode = importFitMode
                        )
                        val newFrames = frames.toMutableList()
                        newFrames.add(safeIndex + 1, sf)
                        frames = newFrames
                        currentFrameIndex = safeIndex + 1
                        showImportDialog = false
                        statusMessage = "Imported 1-bit frame!"
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalCyanDim),
                    border = androidx.compose.foundation.BorderStroke(1.dp, TacticalCyan)
                ) {
                    Text("ADD FRAME", color = TacticalCyan, fontSize = 9.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }
            },
            dismissButton = {
                TextButton(onClick = { showImportDialog = false }) {
                    Text("CANCEL", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
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
