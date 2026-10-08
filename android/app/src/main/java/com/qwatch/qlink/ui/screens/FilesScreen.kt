package com.qwatch.qlink.ui.screens

import android.net.Uri
import android.os.Environment
import android.provider.OpenableColumns
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.model.ConnectionState
import com.qwatch.qlink.model.StorageTelemetry
import com.qwatch.qlink.model.WatchFile
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.ui.components.TacticalCard
import com.qwatch.qlink.ui.theme.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream

enum class ClipboardMode { COPY, CUT }
data class ClipboardItem(val file: WatchFile, val mode: ClipboardMode)

@Composable
fun FilesScreen() {
    val client = QLinkClient.instance
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    var currentPath by remember { mutableStateOf("/") }
    var fileList by remember { mutableStateOf<List<WatchFile>>(emptyList()) }
    var storageTelemetry by remember { mutableStateOf<StorageTelemetry?>(null) }
    var isLoading by remember { mutableStateOf(false) }
    var isUploading by remember { mutableStateOf(false) }
    var uploadProgressPct by remember { mutableStateOf(0) }
    var uploadStatusText by remember { mutableStateOf("") }

    val connectionState by client.connectionState.collectAsState(initial = ConnectionState.DISCONNECTED)

    // Clipboard state for Copy / Cut / Paste
    var clipboard by remember { mutableStateOf<ClipboardItem?>(null) }

    // Dialog states
    var showNewFolderDialog by remember { mutableStateOf(false) }
    var newFolderName by remember { mutableStateOf("") }

    var renameTarget by remember { mutableStateOf<WatchFile?>(null) }
    var renameNewName by remember { mutableStateOf("") }

    var deleteTarget by remember { mutableStateOf<WatchFile?>(null) }

    fun refreshFiles() {
        scope.launch {
            isLoading = true
            val res = client.listFiles(currentPath)
            if (res.isSuccess) {
                fileList = res.getOrDefault(emptyList()).sortedWith(
                    compareBy<WatchFile> { !it.isDirectory }.thenBy { it.name.lowercase() }
                )
            } else {
                Toast.makeText(context, "Failed to load files from $currentPath", Toast.LENGTH_SHORT).show()
            }
            // Fetch storage stats
            val stRes = client.getStorageTelemetry()
            if (stRes.isSuccess) {
                storageTelemetry = stRes.getOrNull()
            }
            isLoading = false
        }
    }

    LaunchedEffect(currentPath) {
        refreshFiles()
    }

    // Helper for resolving filename from Uri
    fun getFileName(uri: Uri): String {
        var name = ""
        try {
            val cursor = context.contentResolver.query(uri, null, null, null, null)
            cursor?.use {
                if (it.moveToFirst()) {
                    val idx = it.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                    if (idx >= 0) name = it.getString(idx)
                }
            }
        } catch (_: Exception) {}
        if (name.isEmpty()) {
            name = uri.lastPathSegment ?: "upload_file.bin"
        }
        return name
    }

    // Activity launcher for file upload
    val uploadLauncher = rememberLauncherForActivityResult(ActivityResultContracts.GetContent()) { uri: Uri? ->
        if (uri != null) {
            scope.launch {
                isUploading = true
                uploadProgressPct = 0
                val fileName = getFileName(uri)
                val targetPath = if (currentPath == "/") "/$fileName" else "$currentPath/$fileName"
                uploadStatusText = "Uploading $fileName..."
                try {
                    val bytes = withContext(Dispatchers.IO) {
                        context.contentResolver.openInputStream(uri)?.use { it.readBytes() }
                    }
                    if (bytes != null && bytes.isNotEmpty()) {
                        val res = client.uploadFileWithProgress(targetPath, bytes) { prog ->
                            uploadProgressPct = prog.percent
                        }
                        if (res.isSuccess) {
                            Toast.makeText(context, "Uploaded $fileName (${bytes.size} B)", Toast.LENGTH_SHORT).show()
                            refreshFiles()
                        } else {
                            Toast.makeText(context, "Upload failed: ${res.exceptionOrNull()?.message}", Toast.LENGTH_LONG).show()
                        }
                    } else {
                        Toast.makeText(context, "File was empty or unreadable", Toast.LENGTH_SHORT).show()
                    }
                } catch (e: Exception) {
                    Toast.makeText(context, "Error reading file: ${e.message}", Toast.LENGTH_LONG).show()
                } finally {
                    isUploading = false
                    uploadProgressPct = 0
                    uploadStatusText = ""
                }
            }
        }
    }

    fun downloadWatchFile(file: WatchFile) {
        scope.launch {
            Toast.makeText(context, "Downloading ${file.name}...", Toast.LENGTH_SHORT).show()
            val res = client.downloadFile(file.path)
            if (res.isSuccess) {
                val bytes = res.getOrDefault(ByteArray(0))
                try {
                    val downloadsDir = context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS) ?: context.filesDir
                    val outFile = File(downloadsDir, file.name)
                    withContext(Dispatchers.IO) {
                        FileOutputStream(outFile).use { fos ->
                            fos.write(bytes)
                        }
                    }
                    Toast.makeText(context, "Saved to ${outFile.name} (${bytes.size} B)", Toast.LENGTH_LONG).show()
                } catch (e: Exception) {
                    Toast.makeText(context, "Downloaded (${bytes.size} B) but save failed: ${e.message}", Toast.LENGTH_LONG).show()
                }
            } else {
                Toast.makeText(context, "Download failed: ${res.exceptionOrNull()?.message}", Toast.LENGTH_LONG).show()
            }
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(TacticalBackground)
            .padding(14.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        // Top Card: Storage Telemetry & Connection Status
        TacticalCard(title = "LITTLEFS EXPLORER", accentColor = TacticalCyan) {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    // Transport badge
                    val badgeColor = when (connectionState) {
                        ConnectionState.CONNECTED_WIFI -> TacticalGreen
                        ConnectionState.CONNECTED_BLE -> TacticalCyan
                        ConnectionState.CONNECTING -> TacticalAmber
                        else -> TacticalRed
                    }
                    val badgeText = when (connectionState) {
                        ConnectionState.CONNECTED_WIFI -> "WI-FI LINK"
                        ConnectionState.CONNECTED_BLE -> "BLE LINK"
                        ConnectionState.CONNECTING -> "LINKING..."
                        else -> "DISCONNECTED"
                    }

                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(4.dp))
                            .background(badgeColor.copy(alpha = 0.2f))
                            .padding(horizontal = 8.dp, vertical = 3.dp)
                    ) {
                        Text(
                            text = badgeText,
                            color = badgeColor,
                            fontSize = 10.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                    }

                    // Storage summary if available
                    storageTelemetry?.let { st ->
                        val usedKb = st.fsUsedBytes / 1024L
                        val totalKb = st.fsTotalBytes / 1024L
                        Text(
                            text = "$usedKb / $totalKb KB (${st.freePct.toInt()}% FREE)",
                            color = TacticalAmber,
                            fontSize = 11.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }

                // Storage usage gauge bar
                storageTelemetry?.let { st ->
                    val usedFrac = if (st.fsTotalBytes > 0) (st.fsUsedBytes.toFloat() / st.fsTotalBytes.toFloat()).coerceIn(0f, 1f) else 0f
                    LinearProgressIndicator(
                        progress = { usedFrac },
                        modifier = Modifier.fillMaxWidth().height(4.dp).clip(RoundedCornerShape(2.dp)),
                        color = if (usedFrac > 0.85f) TacticalRed else TacticalCyan,
                        trackColor = TacticalSurfaceVariant
                    )
                }
            }
        }

        // Path & Directory Navigation Bar
        TacticalCard(title = "NAVIGATION & ACTIONS", accentColor = TacticalAmber) {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(4.dp)
                    ) {
                        if (currentPath != "/") {
                            IconButton(
                                onClick = {
                                    val parent = currentPath.substringBeforeLast('/').ifEmpty { "/" }
                                    currentPath = parent
                                },
                                modifier = Modifier.size(28.dp)
                            ) {
                                Icon(
                                    imageVector = Icons.Default.ArrowUpward,
                                    contentDescription = "Up",
                                    tint = TacticalAmber,
                                    modifier = Modifier.size(18.dp)
                                )
                            }
                        }

                        Text(
                            text = "PATH: $currentPath",
                            color = TextPrimary,
                            fontSize = 12.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                    }

                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(4.dp)
                    ) {
                        IconButton(
                            onClick = { refreshFiles() },
                            modifier = Modifier.size(28.dp)
                        ) {
                            Icon(
                                imageVector = Icons.Default.Refresh,
                                contentDescription = "Refresh",
                                tint = TacticalCyan,
                                modifier = Modifier.size(18.dp)
                            )
                        }
                    }
                }

                // Action Buttons Row (Upload, New Folder)
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Button(
                        onClick = { uploadLauncher.launch("*/*") },
                        modifier = Modifier.weight(1f).height(36.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalCyanDim),
                        shape = RoundedCornerShape(4.dp),
                        contentPadding = PaddingValues(horizontal = 8.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Default.Upload,
                            contentDescription = "Upload",
                            tint = TacticalCyan,
                            modifier = Modifier.size(16.dp)
                        )
                        Spacer(modifier = Modifier.width(6.dp))
                        Text("UPLOAD", color = TacticalCyan, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                    }

                    Button(
                        onClick = {
                            newFolderName = ""
                            showNewFolderDialog = true
                        },
                        modifier = Modifier.weight(1f).height(36.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = TacticalAmberDim),
                        shape = RoundedCornerShape(4.dp),
                        contentPadding = PaddingValues(horizontal = 8.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Default.CreateNewFolder,
                            contentDescription = "New Folder",
                            tint = TacticalAmber,
                            modifier = Modifier.size(16.dp)
                        )
                        Spacer(modifier = Modifier.width(6.dp))
                        Text("+ FOLDER", color = TacticalAmber, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                    }
                }

                // Active Clipboard Paste Toolbar
                clipboard?.let { clip ->
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(4.dp))
                            .background(TacticalSurfaceHigh)
                            .padding(horizontal = 8.dp, vertical = 6.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(6.dp)
                        ) {
                            Icon(
                                imageVector = Icons.Default.ContentPaste,
                                contentDescription = null,
                                tint = TacticalGreen,
                                modifier = Modifier.size(16.dp)
                            )
                            Text(
                                text = "${clip.mode.name}: ${clip.file.name}",
                                color = TextPrimary,
                                fontSize = 11.sp,
                                fontFamily = FontFamily.Monospace
                            )
                        }

                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(4.dp)
                        ) {
                            Button(
                                onClick = {
                                    scope.launch {
                                        val destPath = if (currentPath == "/") "/${clip.file.name}" else "$currentPath/${clip.file.name}"
                                        val res = if (clip.mode == ClipboardMode.COPY) {
                                            client.copyFile(clip.file.path, destPath)
                                        } else {
                                            val r = client.renameFile(clip.file.path, destPath)
                                            clipboard = null
                                            r
                                        }
                                        if (res.isSuccess) {
                                            Toast.makeText(context, "${clip.mode.name} completed to $destPath", Toast.LENGTH_SHORT).show()
                                            refreshFiles()
                                        } else {
                                            Toast.makeText(context, "Operation failed", Toast.LENGTH_SHORT).show()
                                        }
                                    }
                                },
                                colors = ButtonDefaults.buttonColors(containerColor = TacticalGreenDim),
                                shape = RoundedCornerShape(4.dp),
                                contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                                modifier = Modifier.height(28.dp)
                            ) {
                                Text("PASTE HERE", color = TacticalGreen, fontSize = 10.sp, fontFamily = FontFamily.Monospace)
                            }

                            IconButton(
                                onClick = { clipboard = null },
                                modifier = Modifier.size(24.dp)
                            ) {
                                Icon(imageVector = Icons.Default.Close, contentDescription = "Cancel", tint = TextMuted, modifier = Modifier.size(14.dp))
                            }
                        }
                    }
                }

                // Quick Directory Jump Shortcuts
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    listOf("/", "/apps", "/anim", "/ir", "/sounds", "/boot", "/config").forEach { dir ->
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(4.dp))
                                .background(if (currentPath == dir) TacticalCyanDim else TacticalSurfaceVariant)
                                .clickable { currentPath = dir }
                                .padding(horizontal = 8.dp, vertical = 4.dp)
                        ) {
                            Text(
                                text = dir,
                                color = if (currentPath == dir) TacticalCyan else TextSecondary,
                                fontSize = 10.sp,
                                fontFamily = FontFamily.Monospace
                            )
                        }
                    }
                }
            }
        }

        // Uploading Progress HUD
        if (isUploading) {
            TacticalCard(title = "UPLOADING", accentColor = TacticalCyan) {
                Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Text(
                        text = "$uploadStatusText ($uploadProgressPct%)",
                        color = TacticalCyan,
                        fontSize = 11.sp,
                        fontFamily = FontFamily.Monospace
                    )
                    LinearProgressIndicator(
                        progress = { uploadProgressPct / 100f },
                        modifier = Modifier.fillMaxWidth().height(4.dp).clip(RoundedCornerShape(2.dp)),
                        color = TacticalCyan,
                        trackColor = TacticalSurfaceVariant
                    )
                }
            }
        }

        // File and Directory List
        if (isLoading) {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                CircularProgressIndicator(color = TacticalCyan)
            }
        } else if (fileList.isEmpty() && currentPath == "/") {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Text(
                    text = "(EMPTY DIRECTORY)",
                    color = TextMuted,
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace
                )
            }
        } else {
            LazyColumn(
                modifier = Modifier.fillMaxSize(),
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                // Parent folder navigation row if inside a subfolder
                if (currentPath != "/") {
                    item {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(6.dp))
                                .background(TacticalSurfaceLow)
                                .clickable {
                                    val parent = currentPath.substringBeforeLast('/').ifEmpty { "/" }
                                    currentPath = parent
                                }
                                .padding(horizontal = 12.dp, vertical = 10.dp),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            Icon(
                                imageVector = Icons.Default.ArrowUpward,
                                contentDescription = "Parent Directory",
                                tint = TacticalAmber,
                                modifier = Modifier.size(20.dp)
                            )
                            Text(
                                text = ".. (Parent Directory)",
                                color = TacticalAmber,
                                fontSize = 12.sp,
                                fontFamily = FontFamily.Monospace,
                                fontWeight = FontWeight.Bold
                            )
                        }
                    }
                }

                // File and directory items
                items(fileList) { file ->
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(6.dp))
                            .background(TacticalSurface)
                            .clickable {
                                if (file.isDirectory) {
                                    currentPath = file.path
                                }
                            }
                            .padding(horizontal = 10.dp, vertical = 8.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        // Left: Icon + Name + Size
                        Row(
                            modifier = Modifier.weight(1f),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            Icon(
                                imageVector = if (file.isDirectory) Icons.Default.Folder else Icons.Default.InsertDriveFile,
                                contentDescription = null,
                                tint = if (file.isDirectory) TacticalAmber else TacticalCyan,
                                modifier = Modifier.size(22.dp)
                            )
                            Column {
                                Text(
                                    text = file.name,
                                    color = TextPrimary,
                                    fontSize = 12.sp,
                                    fontFamily = FontFamily.Monospace,
                                    fontWeight = if (file.isDirectory) FontWeight.Bold else FontWeight.Normal
                                )
                                Text(
                                    text = if (file.isDirectory) "DIR (Click to enter)" else "${file.sizeBytes} B",
                                    color = if (file.isDirectory) TacticalAmber else TextMuted,
                                    fontSize = 10.sp,
                                    fontFamily = FontFamily.Monospace
                                )
                            }
                        }

                        // Right: Actions (Download, Copy, Cut, Rename, Delete)
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(2.dp)
                        ) {
                            if (!file.isDirectory) {
                                // Download file
                                IconButton(
                                    onClick = { downloadWatchFile(file) },
                                    modifier = Modifier.size(28.dp)
                                ) {
                                    Icon(
                                        imageVector = Icons.Default.Download,
                                        contentDescription = "Download",
                                        tint = TacticalGreen,
                                        modifier = Modifier.size(16.dp)
                                    )
                                }

                                // Copy file
                                IconButton(
                                    onClick = {
                                        clipboard = ClipboardItem(file, ClipboardMode.COPY)
                                        Toast.makeText(context, "Copied ${file.name}", Toast.LENGTH_SHORT).show()
                                    },
                                    modifier = Modifier.size(28.dp)
                                ) {
                                    Icon(
                                        imageVector = Icons.Default.ContentCopy,
                                        contentDescription = "Copy",
                                        tint = TacticalCyan,
                                        modifier = Modifier.size(16.dp)
                                    )
                                }

                                // Cut file
                                IconButton(
                                    onClick = {
                                        clipboard = ClipboardItem(file, ClipboardMode.CUT)
                                        Toast.makeText(context, "Cut ${file.name}", Toast.LENGTH_SHORT).show()
                                    },
                                    modifier = Modifier.size(28.dp)
                                ) {
                                    Icon(
                                        imageVector = Icons.Default.ContentCut,
                                        contentDescription = "Cut",
                                        tint = TacticalAmber,
                                        modifier = Modifier.size(16.dp)
                                    )
                                }
                            }

                            // Rename (files and directories)
                            IconButton(
                                onClick = {
                                    renameTarget = file
                                    renameNewName = file.name
                                },
                                modifier = Modifier.size(28.dp)
                            ) {
                                Icon(
                                    imageVector = Icons.Default.Edit,
                                    contentDescription = "Rename",
                                    tint = TextSecondary,
                                    modifier = Modifier.size(16.dp)
                                )
                            }

                            // Delete
                            IconButton(
                                onClick = { deleteTarget = file },
                                modifier = Modifier.size(28.dp)
                            ) {
                                Icon(
                                    imageVector = Icons.Default.Delete,
                                    contentDescription = "Delete",
                                    tint = TacticalRed,
                                    modifier = Modifier.size(16.dp)
                                )
                            }
                        }
                    }
                }
            }
        }
    }

    // New Folder Dialog
    if (showNewFolderDialog) {
        AlertDialog(
            onDismissRequest = { showNewFolderDialog = false },
            title = {
                Text("CREATE FOLDER", color = TacticalAmber, fontFamily = FontFamily.Monospace, fontSize = 14.sp)
            },
            text = {
                OutlinedTextField(
                    value = newFolderName,
                    onValueChange = { newFolderName = it },
                    label = { Text("Folder Name") },
                    singleLine = true
                )
            },
            confirmButton = {
                Button(
                    onClick = {
                        val cleanName = newFolderName.trim().removePrefix("/").removeSuffix("/")
                        if (cleanName.isNotEmpty()) {
                            val newPath = if (currentPath == "/") "/$cleanName" else "$currentPath/$cleanName"
                            scope.launch {
                                val res = client.createDirectory(newPath)
                                if (res.isSuccess) {
                                    Toast.makeText(context, "Created folder $cleanName", Toast.LENGTH_SHORT).show()
                                    refreshFiles()
                                } else {
                                    Toast.makeText(context, "Failed to create folder", Toast.LENGTH_SHORT).show()
                                }
                            }
                        }
                        showNewFolderDialog = false
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalAmber)
                ) {
                    Text("CREATE", color = OledBlack, fontFamily = FontFamily.Monospace)
                }
            },
            dismissButton = {
                TextButton(onClick = { showNewFolderDialog = false }) {
                    Text("CANCEL", color = TextMuted, fontFamily = FontFamily.Monospace)
                }
            },
            containerColor = TacticalSurface
        )
    }

    // Rename Dialog
    renameTarget?.let { target ->
        AlertDialog(
            onDismissRequest = { renameTarget = null },
            title = {
                Text("RENAME ITEM", color = TacticalCyan, fontFamily = FontFamily.Monospace, fontSize = 14.sp)
            },
            text = {
                OutlinedTextField(
                    value = renameNewName,
                    onValueChange = { renameNewName = it },
                    label = { Text("New Name") },
                    singleLine = true
                )
            },
            confirmButton = {
                Button(
                    onClick = {
                        val cleanName = renameNewName.trim().removePrefix("/").removeSuffix("/")
                        if (cleanName.isNotEmpty() && cleanName != target.name) {
                            val parentDir = target.path.substringBeforeLast('/').ifEmpty { "" }
                            val newPath = if (parentDir.isEmpty()) "/$cleanName" else "$parentDir/$cleanName"
                            scope.launch {
                                val res = client.renameFile(target.path, newPath)
                                if (res.isSuccess) {
                                    Toast.makeText(context, "Renamed to $cleanName", Toast.LENGTH_SHORT).show()
                                    refreshFiles()
                                } else {
                                    Toast.makeText(context, "Rename failed", Toast.LENGTH_SHORT).show()
                                }
                            }
                        }
                        renameTarget = null
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalCyan)
                ) {
                    Text("RENAME", color = OledBlack, fontFamily = FontFamily.Monospace)
                }
            },
            dismissButton = {
                TextButton(onClick = { renameTarget = null }) {
                    Text("CANCEL", color = TextMuted, fontFamily = FontFamily.Monospace)
                }
            },
            containerColor = TacticalSurface
        )
    }

    // Delete Confirmation Dialog
    deleteTarget?.let { target ->
        AlertDialog(
            onDismissRequest = { deleteTarget = null },
            title = {
                Text("CONFIRM DELETION", color = TacticalRed, fontFamily = FontFamily.Monospace, fontSize = 14.sp)
            },
            text = {
                Text(
                    text = "Are you sure you want to permanently delete \"${target.name}\"?\nPath: ${target.path}",
                    color = TextPrimary,
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace
                )
            },
            confirmButton = {
                Button(
                    onClick = {
                        scope.launch {
                            val res = client.deleteFile(target.path)
                            if (res.isSuccess) {
                                Toast.makeText(context, "Deleted ${target.name}", Toast.LENGTH_SHORT).show()
                                refreshFiles()
                            } else {
                                Toast.makeText(context, "Delete failed", Toast.LENGTH_SHORT).show()
                            }
                        }
                        deleteTarget = null
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalRed)
                ) {
                    Text("DELETE", color = TextPrimary, fontFamily = FontFamily.Monospace)
                }
            },
            dismissButton = {
                TextButton(onClick = { deleteTarget = null }) {
                    Text("CANCEL", color = TextMuted, fontFamily = FontFamily.Monospace)
                }
            },
            containerColor = TacticalSurface
        )
    }
}
