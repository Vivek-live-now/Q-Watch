package com.qwatch.qlink.ui.screens

import android.widget.Toast
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material.icons.filled.InsertDriveFile
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qwatch.qlink.model.WatchFile
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.ui.components.TacticalCard
import com.qwatch.qlink.ui.theme.*
import kotlinx.coroutines.launch

@Composable
fun FilesScreen() {
    val client = QLinkClient.instance
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    var currentPath by remember { mutableStateOf("/") }
    var fileList by remember { mutableStateOf<List<WatchFile>>(emptyList()) }
    var isLoading by remember { mutableStateOf(false) }

    fun refreshFiles() {
        scope.launch {
            isLoading = true
            val res = client.listFiles(currentPath)
            if (res.isSuccess) {
                fileList = res.getOrDefault(emptyList())
            } else {
                Toast.makeText(context, "Failed to load files from $currentPath", Toast.LENGTH_SHORT).show()
            }
            isLoading = false
        }
    }

    LaunchedEffect(currentPath) {
        refreshFiles()
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(TacticalBackground)
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        // Top Path Bar & Refresh
        TacticalCard(title = "LITTLEFS EXPLORER", accentColor = TacticalCyan) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "PATH: $currentPath",
                    color = TacticalAmber,
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace
                )

                IconButton(onClick = { refreshFiles() }) {
                    Icon(imageVector = Icons.Default.Refresh, contentDescription = "Refresh", tint = TacticalCyan)
                }
            }

            // Quick Directory Shortcuts
            Row(
                modifier = Modifier.fillMaxWidth().padding(top = 8.dp),
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                listOf("/", "/apps", "/sounds", "/anim", "/boot", "/config").forEach { dir ->
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(4.dp))
                            .background(if (currentPath == dir) TacticalCyanDim else TacticalSurfaceVariant)
                            .clickable { currentPath = dir }
                            .padding(horizontal = 8.dp, vertical = 4.dp)
                    ) {
                        Text(
                            text = dir,
                            color = if (currentPath == dir) TacticalCyan else TextPrimary,
                            fontSize = 10.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }
            }
        }

        // File List
        if (isLoading) {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                CircularProgressIndicator(color = TacticalCyan)
            }
        } else if (fileList.isEmpty()) {
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
                items(fileList) { file ->
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(6.dp))
                            .background(TacticalSurface)
                            .padding(12.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            Icon(
                                imageVector = if (file.isDirectory) Icons.Default.Folder else Icons.Default.InsertDriveFile,
                                contentDescription = null,
                                tint = if (file.isDirectory) TacticalAmber else TacticalCyan,
                                modifier = Modifier.size(20.dp)
                            )
                            Column {
                                Text(
                                    text = file.name,
                                    color = TextPrimary,
                                    fontSize = 12.sp,
                                    fontFamily = FontFamily.Monospace
                                )
                                Text(
                                    text = if (file.isDirectory) "DIR" else "${file.sizeBytes} B",
                                    color = TextMuted,
                                    fontSize = 10.sp,
                                    fontFamily = FontFamily.Monospace
                                )
                            }
                        }

                        IconButton(
                            onClick = {
                                scope.launch {
                                    val res = client.deleteFile(file.path)
                                    if (res.isSuccess) {
                                        Toast.makeText(context, "Deleted ${file.name}", Toast.LENGTH_SHORT).show()
                                        refreshFiles()
                                    }
                                }
                            }
                        ) {
                            Icon(imageVector = Icons.Default.Delete, contentDescription = "Delete", tint = TacticalRed, modifier = Modifier.size(18.dp))
                        }
                    }
                }
            }
        }
    }
}
