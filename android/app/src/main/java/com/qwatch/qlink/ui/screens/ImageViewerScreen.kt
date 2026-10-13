package com.qwatch.qlink.ui.screens

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.os.Environment
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.anim.*
import com.qwatch.qlink.model.WatchFile
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.ui.components.TacticalCard
import com.qwatch.qlink.ui.theme.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ImageViewerScreen(
    onBack: () -> Unit,
    onOpenInAnimStudio: ((ByteArray, String) -> Unit)? = null
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val client = remember { QLinkClient.instance }

    // Screen Mode: 0 = CONVERTER & STUDIO, 1 = WATCH GALLERY
    var activeTab by remember { mutableIntStateOf(0) }

    // Converter State
    var sourceBitmap by remember { mutableStateOf<Bitmap?>(null) }
    var sourceUri by remember { mutableStateOf<Uri?>(null) }
    var photoName by remember { mutableStateOf("photo_01") }
    var ditherMode by remember { mutableStateOf(DitherMode.FLOYD_STEINBERG) }
    var fitMode by remember { mutableStateOf(AspectFitMode.FIT) }
    var brightness by remember { mutableFloatStateOf(0f) }
    var contrast by remember { mutableFloatStateOf(0f) }
    var threshold by remember { mutableFloatStateOf(128f) }
    var invertColors by remember { mutableStateOf(false) }
    var phosphorColor by remember { mutableStateOf(PhosphorAmber) }

    // Rendered 1-bit frame
    var previewFrame by remember { mutableStateOf<StudioFrame?>(null) }
    var isProcessing by remember { mutableStateOf(false) }

    // Fast Bitmap for OLED Canvas Rendering
    val oledBitmap = remember { Bitmap.createBitmap(128, 64, Bitmap.Config.ARGB_8888) }
    val pixelBuffer = remember { IntArray(128 * 64) }

    // Watch Gallery State
    var watchPhotos by remember { mutableStateOf<List<WatchFile>>(emptyList()) }
    var isScanningGallery by remember { mutableStateOf(false) }
    var selectedWatchPhoto by remember { mutableStateOf<WatchFile?>(null) }

    // Deployment State
    var isDeploying by remember { mutableStateOf(false) }
    var deployProgress by remember { mutableFloatStateOf(0f) }
    var statusMessage by remember { mutableStateOf<String?>(null) }

    // Recompute 1-bit quantized frame whenever inputs change
    LaunchedEffect(sourceBitmap, ditherMode, fitMode, brightness, contrast, threshold, invertColors) {
        val src = sourceBitmap
        if (src != null) {
            isProcessing = true
            withContext(Dispatchers.Default) {
                val frame = AnimEncoder.decodeBitmapToStudioFrame(
                    src = src,
                    ditherMode = ditherMode,
                    threshold = threshold,
                    invert = invertColors,
                    fitMode = fitMode,
                    brightness = brightness,
                    contrast = contrast
                )
                previewFrame = frame
            }
            isProcessing = false
        }
    }

    // Image Picker Launcher
    val imagePickerLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        if (uri != null) {
            try {
                sourceUri = uri
                val name = uri.lastPathSegment?.substringAfterLast('/')?.substringBeforeLast('.') ?: "photo_01"
                photoName = name.replace(" ", "_").lowercase()

                val bytes = context.contentResolver.openInputStream(uri)?.use { it.readBytes() }
                if (bytes != null && bytes.isNotEmpty()) {
                    if (AnimEncoder.isBmp(bytes)) {
                        val decodedFrame = AnimEncoder.decodeBmpToStudioFrame(bytes)
                        if (decodedFrame != null) {
                            previewFrame = decodedFrame
                            sourceBitmap = decodedFrame.toBitmap()
                            statusMessage = "Loaded 128x64 BMP photo!"
                        } else {
                            val bmp = AnimEncoder.decodeBmpToBitmap(bytes) ?: BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
                            if (bmp != null) {
                                sourceBitmap = bmp
                            } else {
                                statusMessage = "Unsupported BMP format!"
                            }
                        }
                    } else {
                        val bmp = BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
                        if (bmp != null) {
                            sourceBitmap = bmp
                        } else {
                            statusMessage = "Unsupported image format!"
                        }
                    }
                }
            } catch (e: Exception) {
                statusMessage = "Failed to load image: ${e.message}"
            }
        }
    }

    // Refresh photos from watch LittleFS
    fun refreshWatchGallery() {
        if (!client.isConnected()) {
            statusMessage = "Watch not connected!"
            return
        }
        scope.launch {
            isScanningGallery = true
            val list = mutableListOf<WatchFile>()
            val resPhotos = client.listFiles("/photos")
            if (resPhotos.isSuccess) {
                list.addAll(resPhotos.getOrDefault(emptyList()).filter { !it.isDirectory && it.name.endsWith(".bmp", ignoreCase = true) })
            }
            val resImages = client.listFiles("/images")
            if (resImages.isSuccess) {
                list.addAll(resImages.getOrDefault(emptyList()).filter { !it.isDirectory && it.name.endsWith(".bmp", ignoreCase = true) })
            }
            watchPhotos = list
            isScanningGallery = false
            statusMessage = "Found ${list.size} photos on watch"
        }
    }

    statusMessage?.let { msg ->
        LaunchedEffect(msg) {
            Toast.makeText(context, msg, Toast.LENGTH_SHORT).show()
            statusMessage = null
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(TacticalBackground)
            .padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        // 1. Top Header with Back Button & Title
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                IconButton(onClick = onBack, modifier = Modifier.size(32.dp)) {
                    Icon(imageVector = Icons.Default.ArrowBack, contentDescription = "Back", tint = TacticalGreen)
                }
                Column {
                    Text(
                        text = "PHOTO STUDIO // IMAGE VIEWER",
                        color = TacticalGreen,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold,
                        fontSize = 12.sp,
                        letterSpacing = 0.5.sp
                    )
                    Text(
                        text = "128x64 OLED BMP QUANTIZER & WATCH ALBUM",
                        color = TextMuted,
                        fontFamily = FontFamily.Monospace,
                        fontSize = 8.sp
                    )
                }
            }

            // Tab Selector
            Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                Button(
                    onClick = { activeTab = 0 },
                    colors = ButtonDefaults.buttonColors(containerColor = if (activeTab == 0) TacticalGreenDim else TacticalSurfaceVariant),
                    border = androidx.compose.foundation.BorderStroke(1.dp, if (activeTab == 0) TacticalGreen else TacticalBorder),
                    shape = RoundedCornerShape(4.dp),
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 4.dp),
                    modifier = Modifier.height(28.dp)
                ) {
                    Text("CONVERT", color = if (activeTab == 0) TacticalGreen else TextMuted, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }

                Button(
                    onClick = {
                        activeTab = 1
                        refreshWatchGallery()
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = if (activeTab == 1) TacticalCyanDim else TacticalSurfaceVariant),
                    border = androidx.compose.foundation.BorderStroke(1.dp, if (activeTab == 1) TacticalCyan else TacticalBorder),
                    shape = RoundedCornerShape(4.dp),
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 4.dp),
                    modifier = Modifier.height(28.dp)
                ) {
                    Text("GALLERY", color = if (activeTab == 1) TacticalCyan else TextMuted, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }
            }
        }

        // 2. Tab Contents
        if (activeTab == 0) {
            // TAB 0: PHOTO CONVERTER & OLED SIMULATOR
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                // Image Pick Button & File Meta
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Button(
                        onClick = { imagePickerLauncher.launch("image/*") },
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalGreenDim),
                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalGreen),
                        shape = RoundedCornerShape(4.dp),
                        modifier = Modifier.weight(1f)
                    ) {
                        Icon(imageVector = Icons.Default.AddPhotoAlternate, contentDescription = "Pick Image", tint = TacticalGreen, modifier = Modifier.size(16.dp))
                        Spacer(modifier = Modifier.width(6.dp))
                        Text("CHOOSE PHOTO", color = TacticalGreen, fontSize = 9.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    }

                    OutlinedTextField(
                        value = photoName,
                        onValueChange = { photoName = it.replace(" ", "_").lowercase() },
                        label = { Text("PHOTO NAME", fontSize = 8.sp, fontFamily = FontFamily.Monospace) },
                        suffix = { Text(".bmp", color = TacticalGreen, fontFamily = FontFamily.Monospace, fontSize = 8.sp) },
                        singleLine = true,
                        colors = OutlinedTextFieldDefaults.colors(
                            focusedBorderColor = TacticalGreen,
                            unfocusedBorderColor = TacticalBorder,
                            focusedTextColor = TextPrimary,
                            unfocusedTextColor = TextPrimary
                        ),
                        modifier = Modifier.weight(1f).height(50.dp)
                    )
                }

                // Live OLED Simulation Screen
                TacticalCard(title = "LIVE 128x64 OLED PREVIEW", accentColor = phosphorColor) {
                    Column(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        // OLED Display Frame
                        Box(
                            modifier = Modifier
                                .width(280.dp)
                                .height(140.dp)
                                .clip(RoundedCornerShape(6.dp))
                                .background(Color(0xFF040608))
                                .border(2.dp, TacticalBorder, RoundedCornerShape(6.dp))
                                .padding(6.dp),
                            contentAlignment = Alignment.Center
                        ) {
                            val frame = previewFrame
                            if (frame != null) {
                                Canvas(modifier = Modifier.fillMaxSize()) {
                                    val activeColor = phosphorColor.toArgb()
                                    val offColor = 0xFF06080A.toInt()

                                    for (y in 0 until 64) {
                                        val rowOffset = y * 128
                                        for (x in 0 until 128) {
                                            pixelBuffer[rowOffset + x] = if (frame.getPixel(x, y)) activeColor else offColor
                                        }
                                    }

                                    oledBitmap.setPixels(pixelBuffer, 0, 128, 0, 0, 128, 64)

                                    drawImage(
                                        image = oledBitmap.asImageBitmap(),
                                        dstSize = IntSize(size.width.toInt(), size.height.toInt()),
                                        filterQuality = FilterQuality.None
                                    )
                                }
                            } else {
                                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                                    Icon(imageVector = Icons.Default.Image, contentDescription = null, tint = TextMuted, modifier = Modifier.size(32.dp))
                                    Text("NO PHOTO SELECTED", color = TextMuted, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                                    Text("Pick any photo or image to preview in 128x64 OLED", color = TextSecondary, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                                }
                            }
                        }

                        // Phosphor Color Selector
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text("OLED PHOSPHOR:", color = TextMuted, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
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
                                            .padding(horizontal = 6.dp, vertical = 2.dp)
                                    ) {
                                        Text(lbl, color = if (isSel) OledBlack else TextMuted, fontSize = 7.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                                    }
                                }
                            }
                        }
                    }
                }

                // Dithering & Image Fine-Tuning Controls
                TacticalCard(title = "DITHERING & QUANTIZATION CONTROLS", accentColor = TacticalGreen) {
                    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        // Dither Mode Chips
                        Text("ALGORITHM:", color = TextMuted, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                        Row(
                            modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                            horizontalArrangement = Arrangement.spacedBy(4.dp)
                        ) {
                            DitherMode.values().forEach { dm ->
                                val isSel = ditherMode == dm
                                Box(
                                    modifier = Modifier
                                        .clip(RoundedCornerShape(3.dp))
                                        .background(if (isSel) TacticalGreen else TacticalSurfaceVariant)
                                        .border(1.dp, if (isSel) TacticalGreen else TacticalBorder, RoundedCornerShape(3.dp))
                                        .clickable { ditherMode = dm }
                                        .padding(horizontal = 8.dp, vertical = 4.dp)
                                ) {
                                    Text(dm.label, color = if (isSel) OledBlack else TextPrimary, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                                }
                            }
                        }

                        // Fit Mode Chips
                        Text("ASPECT FIT:", color = TextMuted, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                        Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                            AspectFitMode.values().forEach { fm ->
                                val isSel = fitMode == fm
                                Box(
                                    modifier = Modifier
                                        .clip(RoundedCornerShape(3.dp))
                                        .background(if (isSel) TacticalCyan else TacticalSurfaceVariant)
                                        .border(1.dp, if (isSel) TacticalCyan else TacticalBorder, RoundedCornerShape(3.dp))
                                        .clickable { fitMode = fm }
                                        .padding(horizontal = 8.dp, vertical = 4.dp)
                                ) {
                                    Text(fm.label, color = if (isSel) OledBlack else TextPrimary, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                                }
                            }
                        }

                        // Threshold Slider
                        Column {
                            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                                Text("THRESHOLD", color = TextMuted, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                                Text("${threshold.toInt()}", color = TacticalGreen, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                            }
                            Slider(
                                value = threshold,
                                onValueChange = { threshold = it },
                                valueRange = 0f..255f,
                                colors = SliderDefaults.colors(thumbColor = TacticalGreen, activeTrackColor = TacticalGreen)
                            )
                        }

                        // Brightness Slider
                        Column {
                            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                                Text("BRIGHTNESS", color = TextMuted, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                                Text("${brightness.toInt()}", color = TacticalCyan, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                            }
                            Slider(
                                value = brightness,
                                onValueChange = { brightness = it },
                                valueRange = -100f..100f,
                                colors = SliderDefaults.colors(thumbColor = TacticalCyan, activeTrackColor = TacticalCyan)
                            )
                        }

                        // Contrast Slider
                        Column {
                            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                                Text("CONTRAST", color = TextMuted, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                                Text("${contrast.toInt()}", color = TacticalAmber, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                            }
                            Slider(
                                value = contrast,
                                onValueChange = { contrast = it },
                                valueRange = -100f..100f,
                                colors = SliderDefaults.colors(thumbColor = TacticalAmber, activeTrackColor = TacticalAmber)
                            )
                        }

                        // Invert Colors
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text("INVERT COLORS (WHITE-ON-BLACK)", color = TextPrimary, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                            Switch(
                                checked = invertColors,
                                onCheckedChange = { invertColors = it },
                                colors = SwitchDefaults.colors(checkedThumbColor = TacticalGreen)
                            )
                        }
                    }
                }

                // Deployment & Export Actions
                TacticalCard(title = "WATCH DEPLOYMENT & EXPORT", accentColor = TacticalGreen) {
                    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        val currentBmpBytes = remember(previewFrame, invertColors) {
                            previewFrame?.let { AnimEncoder.encodeStudioFrameToBmp(it, false) }
                        }

                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(3.dp))
                                .background(TacticalSurfaceLow)
                                .border(1.dp, TacticalBorder, RoundedCornerShape(3.dp))
                                .padding(horizontal = 8.dp, vertical = 6.dp),
                            horizontalArrangement = Arrangement.SpaceBetween
                        ) {
                            Text("RESOLUTION: 128x64", color = TacticalCyan, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                            Text("FORMAT: 1-BIT BMP", color = TacticalGreen, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                            Text("SIZE: ${currentBmpBytes?.size ?: 0} B", color = TacticalAmber, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                        }

                        if (isDeploying) {
                            Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                                Text("STREAMING PHOTO TO WATCH... ${(deployProgress * 100).toInt()}%", color = TacticalGreen, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                                LinearProgressIndicator(
                                    progress = { deployProgress },
                                    modifier = Modifier.fillMaxWidth().height(4.dp).clip(RoundedCornerShape(2.dp)),
                                    color = TacticalGreen,
                                    trackColor = TacticalSurfaceVariant
                                )
                            }
                        }

                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.spacedBy(6.dp)
                        ) {
                            // Deploy to /photos/
                            Button(
                                onClick = {
                                    if (!client.isConnected()) {
                                        statusMessage = "Watch not connected! Connect via Wi-Fi or BLE first."
                                        return@Button
                                    }
                                    val bytes = currentBmpBytes
                                    if (bytes == null) {
                                        statusMessage = "Select an image first!"
                                        return@Button
                                    }
                                    val sanitized = photoName.trim().replace(" ", "_").removeSuffix(".bmp").ifBlank { "photo" }
                                    val cleanName = "$sanitized.bmp"
                                    val remotePath = "/photos/$cleanName"
                                    isDeploying = true
                                    deployProgress = 0.1f

                                    scope.launch {
                                        val res = client.uploadFileWithProgress(remotePath, bytes) { prog ->
                                            deployProgress = prog.progress
                                        }
                                        isDeploying = false
                                        statusMessage = if (res.isSuccess && res.getOrDefault(false)) {
                                            "Photo deployed to $remotePath!"
                                        } else {
                                            "Deploy failed: ${res.exceptionOrNull()?.message ?: "upload failed"}"
                                        }
                                    }
                                },
                                enabled = !isDeploying && currentBmpBytes != null,
                                colors = ButtonDefaults.buttonColors(containerColor = TacticalGreenDim),
                                border = androidx.compose.foundation.BorderStroke(1.dp, TacticalGreen),
                                shape = RoundedCornerShape(4.dp),
                                modifier = Modifier.weight(1f)
                            ) {
                                Text("DEPLOY TO WATCH", color = TacticalGreen, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                            }

                            // Set as Watch Boot Screen
                            Button(
                                onClick = {
                                    if (!client.isConnected()) {
                                        statusMessage = "Watch not connected! Connect via Wi-Fi or BLE first."
                                        return@Button
                                    }
                                    val bytes = currentBmpBytes
                                    if (bytes == null) {
                                        statusMessage = "Select an image first!"
                                        return@Button
                                    }
                                    isDeploying = true
                                    deployProgress = 0.1f

                                    scope.launch {
                                        val res = client.uploadFileWithProgress("/boot/boot.bmp", bytes) { prog ->
                                            deployProgress = prog.progress
                                        }
                                        isDeploying = false
                                        statusMessage = if (res.isSuccess && res.getOrDefault(false)) {
                                            "Installed as /boot/boot.bmp splash!"
                                        } else {
                                            "Boot deploy failed: ${res.exceptionOrNull()?.message ?: "upload failed"}"
                                        }
                                    }
                                },
                                enabled = !isDeploying && currentBmpBytes != null,
                                colors = ButtonDefaults.buttonColors(containerColor = TacticalAmberDim),
                                border = androidx.compose.foundation.BorderStroke(1.dp, TacticalAmber),
                                shape = RoundedCornerShape(4.dp),
                                modifier = Modifier.weight(1f)
                            ) {
                                Text("SET AS BOOT SPLASH", color = TacticalAmber, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                            }
                        }

                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.spacedBy(6.dp)
                        ) {
                            // Export to Downloads
                            Button(
                                onClick = {
                                    val bytes = currentBmpBytes
                                    if (bytes != null) {
                                        try {
                                            val downloadsDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS).takeIf { it.exists() || it.mkdirs() }
                                                ?: context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS)
                                                ?: context.filesDir
                                            val cleanName = if (photoName.endsWith(".bmp")) photoName else "$photoName.bmp"
                                            val outFile = File(downloadsDir, cleanName)
                                            FileOutputStream(outFile).use { it.write(bytes) }
                                            statusMessage = "Exported to Downloads: ${outFile.name} (${outFile.length()} B)"
                                            Toast.makeText(context, "Exported to Downloads: ${outFile.name}", Toast.LENGTH_LONG).show()
                                        } catch (e: Exception) {
                                            statusMessage = "Export failed: ${e.message}"
                                        }
                                    }
                                },
                                enabled = currentBmpBytes != null,
                                colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceVariant),
                                border = androidx.compose.foundation.BorderStroke(1.dp, TacticalBorder),
                                shape = RoundedCornerShape(4.dp),
                                modifier = Modifier.weight(1f)
                            ) {
                                Text("EXPORT .BMP", color = TextPrimary, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                            }

                            // Edit in Anim Studio
                            if (onOpenInAnimStudio != null) {
                                Button(
                                    onClick = {
                                        val bytes = currentBmpBytes
                                        if (bytes != null) {
                                            onOpenInAnimStudio(bytes, photoName)
                                        }
                                    },
                                    enabled = currentBmpBytes != null,
                                    colors = ButtonDefaults.buttonColors(containerColor = TacticalCyanDim),
                                    border = androidx.compose.foundation.BorderStroke(1.dp, TacticalCyan),
                                    shape = RoundedCornerShape(4.dp),
                                    modifier = Modifier.weight(1f)
                                ) {
                                    Text("EDIT IN STUDIO", color = TacticalCyan, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                                }
                            }
                        }
                    }
                }
            }
        } else {
            // TAB 1: WATCH PHOTO GALLERY
            Column(
                modifier = Modifier.fillMaxSize(),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text("PHOTOS ON WATCH LITTLEFS (/photos, /images):", color = TextMuted, fontSize = 8.5.sp, fontFamily = FontFamily.Monospace)
                    Button(
                        onClick = { refreshWatchGallery() },
                        enabled = !isScanningGallery,
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalCyanDim),
                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalCyan),
                        shape = RoundedCornerShape(4.dp),
                        contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                        modifier = Modifier.height(26.dp)
                    ) {
                        Text(if (isScanningGallery) "SCANNING..." else "REFRESH", color = TacticalCyan, fontSize = 8.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                    }
                }

                if (watchPhotos.isEmpty()) {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(180.dp)
                            .clip(RoundedCornerShape(6.dp))
                            .background(TacticalSurfaceLow)
                            .border(1.dp, TacticalBorder, RoundedCornerShape(6.dp)),
                        contentAlignment = Alignment.Center
                    ) {
                        Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(4.dp)) {
                            Icon(imageVector = Icons.Default.PhotoLibrary, contentDescription = null, tint = TextMuted, modifier = Modifier.size(36.dp))
                            Text("NO PHOTOS STORED ON WATCH", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                            Text("Convert a photo in the CONVERT tab and deploy to /photos!", color = TextSecondary, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
                        }
                    }
                } else {
                    LazyColumn(
                        modifier = Modifier.fillMaxSize(),
                        verticalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        items(watchPhotos) { file ->
                            Row(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .clip(RoundedCornerShape(4.dp))
                                    .background(TacticalSurfaceVariant)
                                    .border(1.dp, TacticalBorder, RoundedCornerShape(4.dp))
                                    .padding(8.dp),
                                horizontalArrangement = Arrangement.SpaceBetween,
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                    Icon(imageVector = Icons.Default.Image, contentDescription = null, tint = TacticalGreen, modifier = Modifier.size(20.dp))
                                    Column {
                                        Text(file.name, color = TextPrimary, fontSize = 9.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                                        Text("${file.path} • ${file.sizeBytes} B", color = TextMuted, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace)
                                    }
                                }

                                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                                    // Preview button
                                    Button(
                                        onClick = {
                                            selectedWatchPhoto = file
                                            scope.launch {
                                                val res = client.downloadFile(file.path)
                                                if (res.isSuccess) {
                                                    val bytes = res.getOrNull()
                                                    if (bytes != null) {
                                                        val frame = AnimEncoder.decodeBmpToStudioFrame(bytes)
                                                        if (frame != null) {
                                                            previewFrame = frame
                                                            sourceBitmap = frame.toBitmap()
                                                            activeTab = 0
                                                            photoName = file.name.removeSuffix(".bmp")
                                                            statusMessage = "Previewing ${file.name}"
                                                        }
                                                    }
                                                } else {
                                                    statusMessage = "Failed to download ${file.name}"
                                                }
                                            }
                                        },
                                        colors = ButtonDefaults.buttonColors(containerColor = TacticalCyanDim),
                                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalCyan),
                                        shape = RoundedCornerShape(3.dp),
                                        contentPadding = PaddingValues(horizontal = 6.dp, vertical = 2.dp),
                                        modifier = Modifier.height(24.dp)
                                    ) {
                                        Text("VIEW", color = TacticalCyan, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                                    }

                                    // Set as Boot button
                                    Button(
                                        onClick = {
                                            if (!client.isConnected()) {
                                                statusMessage = "Watch not connected!"
                                                return@Button
                                            }
                                            scope.launch {
                                                val res = client.downloadFile(file.path)
                                                if (res.isSuccess) {
                                                    val bytes = res.getOrNull()
                                                    if (bytes != null) {
                                                        val upRes = client.uploadFile("/boot/boot.bmp", bytes)
                                                        statusMessage = if (upRes.isSuccess && upRes.getOrDefault(false)) "Set ${file.name} as boot splash!" else "Boot set failed: ${upRes.exceptionOrNull()?.message ?: "error"}"
                                                    }
                                                } else {
                                                    statusMessage = "Failed to download ${file.name}"
                                                }
                                            }
                                        },
                                        colors = ButtonDefaults.buttonColors(containerColor = TacticalAmberDim),
                                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalAmber),
                                        shape = RoundedCornerShape(3.dp),
                                        contentPadding = PaddingValues(horizontal = 6.dp, vertical = 2.dp),
                                        modifier = Modifier.height(24.dp)
                                    ) {
                                        Text("BOOT", color = TacticalAmber, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                                    }

                                    // Delete button
                                    Button(
                                        onClick = {
                                            scope.launch {
                                                val delRes = client.deleteFile(file.path)
                                                if (delRes.isSuccess) {
                                                    refreshWatchGallery()
                                                    statusMessage = "Deleted ${file.name}"
                                                } else {
                                                    statusMessage = "Delete failed"
                                                }
                                            }
                                        },
                                        colors = ButtonDefaults.buttonColors(containerColor = TacticalRedDim),
                                        border = androidx.compose.foundation.BorderStroke(1.dp, TacticalRed),
                                        shape = RoundedCornerShape(3.dp),
                                        contentPadding = PaddingValues(horizontal = 6.dp, vertical = 2.dp),
                                        modifier = Modifier.height(24.dp)
                                    ) {
                                        Text("DEL", color = TacticalRed, fontSize = 7.5.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
