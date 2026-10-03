package com.qwatch.qlink.ui.screens

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
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material.icons.outlined.StarBorder
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.qwatch.qlink.anim.*
import com.qwatch.qlink.ui.components.TacticalCard
import com.qwatch.qlink.ui.theme.*
import com.qwatch.qlink.viewmodel.AnimMarketViewModel

@Composable
fun AnimMarketScreen(
    onBack: (() -> Unit)? = null,
    viewModel: AnimMarketViewModel = viewModel()
) {
    val context = LocalContext.current
    val catalog by viewModel.catalog.collectAsState()
    val selectedItem by viewModel.selectedItem.collectAsState()
    val activeAnim by viewModel.activeAnimFile.collectAsState()
    val currentFrameIndex by viewModel.currentFrameIndex.collectAsState()
    val isPlaying by viewModel.isPlaying.collectAsState()
    val speedMultiplier by viewModel.speedMultiplier.collectAsState()
    val selectedCategory by viewModel.selectedCategory.collectAsState()
    val storageTotal by viewModel.storageTotalBytes.collectAsState()
    val storageUsed by viewModel.storageUsedBytes.collectAsState()
    val phosphorColor by viewModel.selectedPhosphor.collectAsState()
    val statusMessage by viewModel.statusMessage.collectAsState()
    val isLoading by viewModel.isLoading.collectAsState()

    // Custom Animation Studio dialog state
    var customBitmap by remember { mutableStateOf<Bitmap?>(null) }
    var customAnimName by remember { mutableStateOf("custom") }
    var useDithering by remember { mutableStateOf(true) }
    var customThreshold by remember { mutableFloatStateOf(128f) }
    var invertColors by remember { mutableStateOf(false) }
    var customDelayMs by remember { mutableIntStateOf(50) }
    var showCustomStudio by remember { mutableStateOf(false) }

    // File picker launcher for images and .anim
    val filePickerLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        if (uri != null) {
            try {
                context.contentResolver.openInputStream(uri)?.use { stream ->
                    val bytes = stream.readBytes()
                    val fileName = uri.lastPathSegment?.substringAfterLast('/') ?: "custom.anim"
                    if (fileName.endsWith(".anim")) {
                        viewModel.importCustomAnim(fileName, bytes)
                    } else {
                        // Image file -> open Custom Studio for fine-tuning
                        val bmp = BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
                        if (bmp != null) {
                            customBitmap = bmp
                            customAnimName = fileName.substringBeforeLast('.')
                            showCustomStudio = true
                        } else {
                            Toast.makeText(context, "Unsupported file format!", Toast.LENGTH_SHORT).show()
                        }
                    }
                }
            } catch (e: Exception) {
                Toast.makeText(context, "Import failed: ${e.message}", Toast.LENGTH_SHORT).show()
            }
        }
    }

    LaunchedEffect(Unit) {
        viewModel.loadCatalog(context)
    }

    statusMessage?.let { msg ->
        LaunchedEffect(msg) {
            Toast.makeText(context, msg, Toast.LENGTH_SHORT).show()
            viewModel.clearStatus()
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(TacticalBackground)
            .padding(14.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        // 1. Top Header Bar
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                if (onBack != null) {
                    IconButton(onClick = onBack, modifier = Modifier.size(32.dp)) {
                        Icon(imageVector = Icons.Default.ArrowBack, contentDescription = "Back", tint = TacticalCyan)
                    }
                }
                Column {
                    Text(
                        text = "MOCHI ANIMATION MARKET",
                        color = TacticalCyan,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold,
                        fontSize = 13.sp,
                        letterSpacing = 1.sp
                    )
                    Text(
                        text = "128x64 OLED DEPLOYMENT DECK",
                        color = TextMuted,
                        fontFamily = FontFamily.Monospace,
                        fontSize = 8.sp
                    )
                }
            }

            IconButton(onClick = { viewModel.loadCatalog(context) }) {
                Icon(imageVector = Icons.Default.Refresh, contentDescription = "Refresh", tint = TacticalCyan)
            }
        }

        // 2. Hero Section: Virtual 128x64 OLED Canvas Previewer
        TacticalCard(title = "VIRTUAL OLED ANIMATION PREVIEWER", accentColor = TacticalAmber) {
            // Header: Animation title and Phosphor Palette Selector
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                val currentName = selectedItem?.displayName ?: "NO ANIMATION LOADED"
                Text(
                    text = currentName,
                    color = TacticalAmber,
                    fontSize = 10.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.weight(1f)
                )

                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    listOf(
                        "AMBER" to PhosphorAmber,
                        "CYAN" to PhosphorCyan,
                        "GREEN" to PhosphorGreen,
                        "WHITE" to Color.White
                    ).forEach { (label, col) ->
                        val isSelected = phosphorColor == col
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(3.dp))
                                .background(if (isSelected) col else TacticalSurfaceVariant)
                                .border(1.dp, if (isSelected) col else TacticalBorder, RoundedCornerShape(3.dp))
                                .clickable { viewModel.setPhosphorColor(col) }
                                .padding(horizontal = 5.dp, vertical = 2.dp)
                        ) {
                            Text(
                                text = label,
                                color = if (isSelected) OledBlack else TextMuted,
                                fontSize = 7.5.sp,
                                fontFamily = FontFamily.Monospace,
                                fontWeight = FontWeight.Bold
                            )
                        }
                    }
                }
            }

            Spacer(modifier = Modifier.height(6.dp))

            // Virtual OLED Display Canvas (128x64)
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .aspectRatio(128f / 64f)
                    .clip(RoundedCornerShape(6.dp))
                    .background(PhosphorOff)
                    .border(2.dp, TacticalBorder, RoundedCornerShape(6.dp))
                    .padding(3.dp)
            ) {
                val anim = activeAnim
                if (anim != null && anim.frames.isNotEmpty()) {
                    val safeIdx = currentFrameIndex.coerceIn(0, anim.frames.size - 1)
                    val frameBytes = anim.frames[safeIdx]
                    val pixelArray = remember(frameBytes) {
                        AnimParser.decodeFramePixels(frameBytes)
                    }

                    Canvas(modifier = Modifier.fillMaxSize()) {
                        val canvasW = size.width
                        val canvasH = size.height
                        val pixelW = canvasW / 128f
                        val pixelH = canvasH / 64f

                        for (y in 0 until 64) {
                            val rowOffset = y * 128
                            for (x in 0 until 128) {
                                if (pixelArray[rowOffset + x]) {
                                    drawRect(
                                        color = phosphorColor,
                                        topLeft = Offset(x * pixelW, y * pixelH),
                                        size = Size(pixelW * 0.92f, pixelH * 0.92f)
                                    )
                                }
                            }
                        }
                    }
                } else {
                    Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                        Text(
                            text = "[ NO ANIMATION LOADED ]",
                            color = TextMuted,
                            fontSize = 10.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }
            }

            Spacer(modifier = Modifier.height(6.dp))

            // Metadata & Telemetry Status Line
            val frameCount = activeAnim?.frames?.size ?: 0
            val effectiveFps = activeAnim?.header?.fps ?: 20
            val displayFps = (effectiveFps * speedMultiplier).toInt()
            val totalBytes = activeAnim?.totalBytes ?: (selectedItem?.sizeBytes ?: 0L)
            val durationSec = activeAnim?.header?.durationSec ?: (selectedItem?.durationSec ?: 1.0f)

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "128x64 · ${frameCount}F · ${String.format("%.1f", durationSec)}s @ ${displayFps}FPS · ${String.format("%,d", totalBytes)}B",
                    color = TextMuted,
                    fontSize = 8.sp,
                    fontFamily = FontFamily.Monospace
                )

                // Speed Cycle Button
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(3.dp))
                        .background(TacticalSurfaceVariant)
                        .clickable { viewModel.cycleSpeed() }
                        .padding(horizontal = 6.dp, vertical = 2.dp)
                ) {
                    Text(
                        text = "SPD: ${speedMultiplier}x",
                        color = TacticalCyan,
                        fontSize = 8.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                }
            }

            Spacer(modifier = Modifier.height(4.dp))

            // Timeline Scrubber Slider
            if (frameCount > 1) {
                Slider(
                    value = currentFrameIndex.toFloat(),
                    onValueChange = { viewModel.seekFrame(it.toInt()) },
                    valueRange = 0f..(frameCount - 1).toFloat(),
                    steps = (frameCount - 2).coerceAtLeast(0),
                    colors = SliderDefaults.colors(
                        thumbColor = TacticalAmber,
                        activeTrackColor = TacticalAmber,
                        inactiveTrackColor = TacticalSurfaceVariant
                    ),
                    modifier = Modifier.fillMaxWidth().height(24.dp)
                )
            }

            // Transport Control Buttons
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                Button(
                    onClick = { viewModel.stepBackward() },
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                    shape = RoundedCornerShape(4.dp),
                    modifier = Modifier.weight(1f)
                ) {
                    Icon(imageVector = Icons.Default.SkipPrevious, contentDescription = "Prev", tint = TextPrimary, modifier = Modifier.size(16.dp))
                }

                Button(
                    onClick = { viewModel.togglePlayPause() },
                    colors = ButtonDefaults.buttonColors(containerColor = if (isPlaying) TacticalAmber else TacticalCyan),
                    shape = RoundedCornerShape(4.dp),
                    modifier = Modifier.weight(1.5f)
                ) {
                    Icon(
                        imageVector = if (isPlaying) Icons.Default.Pause else Icons.Default.PlayArrow,
                        contentDescription = "Play/Pause",
                        tint = OledBlack,
                        modifier = Modifier.size(16.dp)
                    )
                    Spacer(modifier = Modifier.width(4.dp))
                    Text(
                        text = if (isPlaying) "PAUSE" else "PLAY",
                        color = OledBlack,
                        fontSize = 9.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                }

                Button(
                    onClick = { viewModel.stepForward() },
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                    shape = RoundedCornerShape(4.dp),
                    modifier = Modifier.weight(1f)
                ) {
                    Icon(imageVector = Icons.Default.SkipNext, contentDescription = "Next", tint = TextPrimary, modifier = Modifier.size(16.dp))
                }
            }
        }

        // 3. Dynamic LittleFS Storage Headroom & Procedural Mochi Badge
        val usedKb = storageUsed / 1024L
        val totalKb = storageTotal / 1024L
        val freeKb = (storageTotal - storageUsed).coerceAtLeast(0L) / 1024L
        val freePct = if (totalKb > 0) (freeKb.toFloat() / totalKb.toFloat()) else 0f
        val freeSlots = (freeKb / 20L).toInt() // Average ~20 KB per anim

        val barColor = when {
            freePct > 0.30f -> TacticalGreen
            freePct > 0.10f -> TacticalAmber
            else -> TacticalRed
        }

        TacticalCard(title = "LITTLEFS STORAGE MANAGEMENT", accentColor = barColor) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "$usedKb KB / $totalKb KB USED",
                    color = TextPrimary,
                    fontSize = 9.5.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold
                )
                Text(
                    text = "$freeKb KB FREE (~$freeSlots SLOTS)",
                    color = barColor,
                    fontSize = 9.5.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold
                )
            }

            Spacer(modifier = Modifier.height(4.dp))

            LinearProgressIndicator(
                progress = { (usedKb.toFloat() / totalKb.toFloat().coerceAtLeast(1f)).coerceIn(0f, 1f) },
                modifier = Modifier
                    .fillMaxWidth()
                    .height(6.dp)
                    .clip(RoundedCornerShape(3.dp)),
                color = barColor,
                trackColor = TacticalSurfaceVariant
            )

            Spacer(modifier = Modifier.height(6.dp))

            // Procedural Mochi preservation note
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(4.dp))
                    .background(TacticalSurfaceLow)
                    .border(1.dp, TacticalBorder, RoundedCornerShape(4.dp))
                    .padding(horizontal = 8.dp, vertical = 4.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                Icon(imageVector = Icons.Default.Shield, contentDescription = "Shield", tint = TacticalGreen, modifier = Modifier.size(12.dp))
                Text(
                    text = "FIRMWARE PROCEDURAL MOCHI (17 EMOTES, 5 HELMETS) PRESERVED IN FLASH ROM",
                    color = TextMuted,
                    fontSize = 7.5.sp,
                    fontFamily = FontFamily.Monospace
                )
            }
        }

        // 4. Category Filter Chips (with ★ FAVORITES) & Custom Import
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Row(horizontalArrangement = Arrangement.spacedBy(3.dp)) {
                AnimCategory.values().forEach { cat ->
                    val isSelected = selectedCategory == cat
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(4.dp))
                            .background(if (isSelected) TacticalCyanDim else TacticalSurfaceVariant)
                            .border(1.dp, if (isSelected) TacticalCyan else TacticalBorder, RoundedCornerShape(4.dp))
                            .clickable { viewModel.setCategory(cat) }
                            .padding(horizontal = 5.dp, vertical = 3.dp)
                    ) {
                        Text(
                            text = cat.label,
                            color = if (isSelected) TacticalCyan else TextPrimary,
                            fontSize = 8.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                    }
                }
            }

            // Custom Import Action Button
            IconButton(
                onClick = { filePickerLauncher.launch("*/*") },
                modifier = Modifier.size(28.dp)
            ) {
                Icon(imageVector = Icons.Default.AddPhotoAlternate, contentDescription = "Import", tint = TacticalCyan)
            }
        }

        // 5. Animation Cards Catalog
        val filteredList = remember(catalog, selectedCategory) {
            when (selectedCategory) {
                AnimCategory.ALL -> catalog
                AnimCategory.FAVORITES -> catalog.filter { it.isFavorite }
                AnimCategory.INSTALLED -> catalog.filter { it.isInstalled }
                else -> catalog.filter { it.category == selectedCategory }
            }
        }

        if (isLoading) {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                CircularProgressIndicator(color = TacticalCyan)
            }
        } else if (filteredList.isEmpty()) {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Text(
                    text = "NO ANIMATIONS IN CATEGORY",
                    color = TextMuted,
                    fontSize = 11.sp,
                    fontFamily = FontFamily.Monospace
                )
            }
        } else {
            LazyColumn(
                modifier = Modifier.fillMaxSize(),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(filteredList, key = { it.name }) { item ->
                    AnimMarketCard(
                        item = item,
                        isSelected = selectedItem?.name == item.name,
                        onSelect = { viewModel.selectAnim(context, item) },
                        onToggleFavorite = { viewModel.toggleFavorite(context, item) },
                        onDeploy = { viewModel.deployToWatch(context, item, isReplace = false) },
                        onReplace = { viewModel.deployToWatch(context, item, isReplace = true) },
                        onDelete = { viewModel.deleteFromWatch(item) },
                        onSetBoot = { viewModel.setAsBootAnimation(context, item) }
                    )
                }
            }
        }
    }

    // 6. Custom Animation Studio Dialog
    if (showCustomStudio && customBitmap != null) {
        val bmp = customBitmap!!
        AlertDialog(
            onDismissRequest = { showCustomStudio = false },
            containerColor = TacticalSurface,
            title = {
                Text("CUSTOM ANIMATION STUDIO", color = TacticalCyan, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold, fontSize = 13.sp)
            },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    Text(
                        text = "Fine-tune 1-bit OLED conversion before deploying to watch:",
                        color = TextMuted,
                        fontSize = 9.sp,
                        fontFamily = FontFamily.Monospace
                    )

                    // Dithering vs Threshold Switch
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text("DITHERING (FLOYD-STEINBERG)", color = TextPrimary, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                        Switch(
                            checked = useDithering,
                            onCheckedChange = { useDithering = it },
                            colors = SwitchDefaults.colors(checkedThumbColor = TacticalCyan)
                        )
                    }

                    // Invert Colors Switch
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text("INVERT COLORS", color = TextPrimary, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                        Switch(
                            checked = invertColors,
                            onCheckedChange = { invertColors = it },
                            colors = SwitchDefaults.colors(checkedThumbColor = TacticalAmber)
                        )
                    }

                    // Threshold Slider
                    Column {
                        Text("THRESHOLD: ${customThreshold.toInt()}", color = TextMuted, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                        Slider(
                            value = customThreshold,
                            onValueChange = { customThreshold = it },
                            valueRange = 0f..255f,
                            colors = SliderDefaults.colors(thumbColor = TacticalCyan, activeTrackColor = TacticalCyan)
                        )
                    }

                    // Frame Delay Slider
                    Column {
                        Text("FRAME DELAY: ${customDelayMs}ms (${1000 / customDelayMs.coerceAtLeast(10)} FPS)", color = TextMuted, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                        Slider(
                            value = customDelayMs.toFloat(),
                            onValueChange = { customDelayMs = it.toInt() },
                            valueRange = 20f..200f,
                            colors = SliderDefaults.colors(thumbColor = TacticalAmber, activeTrackColor = TacticalAmber)
                        )
                    }
                }
            },
            confirmButton = {
                Button(
                    onClick = {
                        val animBytes = AnimEncoder.encodeAnimation(
                            bitmaps = listOf(bmp),
                            delayMs = customDelayMs,
                            useDithering = useDithering,
                            threshold = customThreshold,
                            invert = invertColors
                        )
                        viewModel.importCustomAnim("${customAnimName}.anim", animBytes)
                        showCustomStudio = false
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalCyanDim),
                    border = androidx.compose.foundation.BorderStroke(1.dp, TacticalCyan)
                ) {
                    Text("GENERATE & PREVIEW", color = TacticalCyan, fontSize = 10.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }
            },
            dismissButton = {
                TextButton(onClick = { showCustomStudio = false }) {
                    Text("CANCEL", color = TextMuted, fontSize = 10.sp, fontFamily = FontFamily.Monospace)
                }
            }
        )
    }
}

@Composable
fun AnimMarketCard(
    item: AnimMarketItem,
    isSelected: Boolean,
    onSelect: () -> Unit,
    onToggleFavorite: () -> Unit,
    onDeploy: () -> Unit,
    onReplace: () -> Unit,
    onDelete: () -> Unit,
    onSetBoot: () -> Unit
) {
    val borderColor = if (isSelected) TacticalAmber else TacticalBorder

    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(6.dp))
            .background(TacticalSurfaceLow)
            .border(1.dp, borderColor, RoundedCornerShape(6.dp))
            .clickable(onClick = onSelect)
            .padding(10.dp)
    ) {
        Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            // Header: Name, Favorite Star, and Status Badge
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    IconButton(onClick = onToggleFavorite, modifier = Modifier.size(24.dp)) {
                        Icon(
                            imageVector = if (item.isFavorite) Icons.Default.Star else Icons.Outlined.StarBorder,
                            contentDescription = "Favorite",
                            tint = if (item.isFavorite) TacticalAmber else TextMuted,
                            modifier = Modifier.size(18.dp)
                        )
                    }

                    Column {
                        Text(
                            text = item.displayName,
                            color = if (isSelected) TacticalAmber else TextPrimary,
                            fontSize = 11.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                        Text(
                            text = "${item.formattedDuration} · ${item.formattedSize}",
                            color = TextMuted,
                            fontSize = 8.5.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }

                // Status Badge
                val (badgeText, badgeColor) = when {
                    item.deployState == AnimDeployState.DEPLOYING -> "DEPLOYING..." to TacticalAmber
                    item.isInstalled -> "INSTALLED" to TacticalGreen
                    else -> "AVAILABLE" to TacticalCyan
                }

                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(3.dp))
                        .background(badgeColor.copy(alpha = 0.15f))
                        .border(1.dp, badgeColor, RoundedCornerShape(3.dp))
                        .padding(horizontal = 6.dp, vertical = 2.dp)
                ) {
                    Text(
                        text = badgeText,
                        color = badgeColor,
                        fontSize = 8.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                }
            }

            // Real-time Transfer Progress Bar
            if (item.deployState == AnimDeployState.DEPLOYING && item.transferProgress != null) {
                val prog = item.transferProgress
                Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        Text(
                            text = "STREAMING: ${prog.percent}%",
                            color = TacticalAmber,
                            fontSize = 8.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                        Text(
                            text = "${prog.bytesTransferred / 1024} KB / ${prog.totalBytes / 1024} KB",
                            color = TextMuted,
                            fontSize = 8.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }

                    LinearProgressIndicator(
                        progress = { prog.progress },
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(4.dp)
                            .clip(RoundedCornerShape(2.dp)),
                        color = TacticalAmber,
                        trackColor = TacticalSurfaceVariant
                    )
                }
            }

            // Action Buttons
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                if (!item.isInstalled) {
                    Button(
                        onClick = onDeploy,
                        enabled = item.deployState != AnimDeployState.DEPLOYING,
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalCyanDim),
                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalCyan),
                        shape = RoundedCornerShape(4.dp),
                        modifier = Modifier.weight(1f)
                    ) {
                        Text(
                            text = "DEPLOY TO WATCH",
                            color = TacticalCyan,
                            fontSize = 9.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                    }
                } else {
                    Button(
                        onClick = onReplace,
                        enabled = item.deployState != AnimDeployState.DEPLOYING,
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalCyanDim),
                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalCyan),
                        shape = RoundedCornerShape(4.dp),
                        modifier = Modifier.weight(1f)
                    ) {
                        Text(
                            text = "REPLACE",
                            color = TacticalCyan,
                            fontSize = 8.5.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                    }

                    Button(
                        onClick = onSetBoot,
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalAmberDim),
                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalAmber),
                        shape = RoundedCornerShape(4.dp),
                        modifier = Modifier.weight(1f)
                    ) {
                        Text(
                            text = "SET BOOT",
                            color = TacticalAmber,
                            fontSize = 8.5.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }

                    Button(
                        onClick = onDelete,
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalRedDim),
                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalRed),
                        shape = RoundedCornerShape(4.dp),
                        modifier = Modifier.weight(1f)
                    ) {
                        Text(
                            text = "DELETE",
                            color = TacticalRed,
                            fontSize = 8.5.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }
            }
        }
    }
}
