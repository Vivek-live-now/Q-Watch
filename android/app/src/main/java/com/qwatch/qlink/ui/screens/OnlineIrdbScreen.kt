package com.qwatch.qlink.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.lifecycle.viewmodel.compose.viewModel
import com.qwatch.qlink.irdb.*
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.ui.components.AdaptiveVirtualRemote
import com.qwatch.qlink.ui.components.TacticalCard
import com.qwatch.qlink.ui.theme.*
import com.qwatch.qlink.viewmodel.OnlineIrdbViewModel

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun OnlineIrdbScreen(
    onBack: (() -> Unit)? = null,
    viewModel: OnlineIrdbViewModel = viewModel()
) {
    val entries by viewModel.filteredEntries.collectAsState()
    val isLoading by viewModel.isLoading.collectAsState()
    val searchQuery by viewModel.searchQuery.collectAsState()
    val selectedCategory by viewModel.selectedCategory.collectAsState()
    val selectedBrand by viewModel.selectedBrand.collectAsState()
    val favorites by viewModel.favorites.collectAsState()
    val previewRemote by viewModel.previewRemote.collectAsState()
    val isPreviewLoading by viewModel.isPreviewLoading.collectAsState()
    val transferProgress by viewModel.transferProgress.collectAsState()

    val categories = listOf("ALL", "TVs", "ACs", "LED/RGB", "Monitors", "SoundBars", "Fans", "Projectors", "FAVORITES")
    val brands = listOf("ALL", "Samsung", "LG", "Sony", "Apple", "Panasonic", "Philips", "Daikin", "Gree", "Dyson", "TCL", "Toshiba", "Dell")

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(TacticalBackground)
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        // Top Header
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                if (onBack != null) {
                    IconButton(onClick = onBack) {
                        Icon(Icons.Default.ArrowBack, contentDescription = "Back", tint = TacticalCyan)
                    }
                }
                Column {
                    Text(
                        text = "ONLINE IRDB // FLIPPER REPO",
                        color = TacticalCyan,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold,
                        fontSize = 14.sp
                    )
                    Text(
                        text = "2,700+ REMOTES • BLUETOOTH FLASHING",
                        color = TextMuted,
                        fontFamily = FontFamily.Monospace,
                        fontSize = 9.sp
                    )
                }
            }

            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                Button(
                    onClick = { viewModel.openSampleRemote("TV") },
                    colors = ButtonDefaults.buttonColors(containerColor = TacticalCyanDim),
                    shape = RoundedCornerShape(4.dp),
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 4.dp),
                    modifier = Modifier.height(32.dp).border(1.dp, TacticalCyan, RoundedCornerShape(4.dp))
                ) {
                    Icon(Icons.Default.Tv, contentDescription = null, tint = TacticalCyan, modifier = Modifier.size(13.dp))
                    Spacer(modifier = Modifier.width(4.dp))
                    Text("REMOTE", color = TacticalCyan, fontSize = 9.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                }

                IconButton(onClick = { viewModel.loadDatabase(forceRefresh = true) }) {
                    Icon(Icons.Default.Refresh, contentDescription = "Refresh", tint = TacticalCyan)
                }
            }
        }

        // Search Input Bar
        OutlinedTextField(
            value = searchQuery,
            onValueChange = { viewModel.onSearchQueryChanged(it) },
            placeholder = { Text("Search brand, model, TV, AC, projector...", color = TextMuted, fontSize = 11.sp, fontFamily = FontFamily.Monospace) },
            leadingIcon = { Icon(Icons.Default.Search, contentDescription = null, tint = TacticalCyan) },
            trailingIcon = {
                if (searchQuery.isNotEmpty()) {
                    IconButton(onClick = { viewModel.onSearchQueryChanged("") }) {
                        Icon(Icons.Default.Close, contentDescription = "Clear", tint = TextMuted)
                    }
                }
            },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
            colors = OutlinedTextFieldDefaults.colors(
                focusedBorderColor = TacticalCyan,
                unfocusedBorderColor = TacticalBorder,
                focusedTextColor = TextPrimary,
                unfocusedTextColor = TextPrimary
            )
        )

        // Category Filter Chips
        LazyRow(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier.fillMaxWidth()
        ) {
            items(categories) { cat ->
                val isSelected = selectedCategory == cat
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(4.dp))
                        .background(if (isSelected) TacticalCyan else TacticalSurfaceVariant)
                        .clickable { viewModel.onCategorySelected(cat) }
                        .padding(horizontal = 10.dp, vertical = 5.dp)
                ) {
                    Text(
                        text = if (cat == "FAVORITES") "★ FAVORITES" else cat,
                        color = if (isSelected) OledBlack else TextSecondary,
                        fontSize = 10.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Normal
                    )
                }
            }
        }

        // Brand Filter Chips
        LazyRow(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier.fillMaxWidth()
        ) {
            items(brands) { brand ->
                val isSelected = selectedBrand == brand
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(4.dp))
                        .background(if (isSelected) TacticalAmber else TacticalSurface)
                        .clickable { viewModel.onBrandSelected(brand) }
                        .padding(horizontal = 8.dp, vertical = 4.dp)
                ) {
                    Text(
                        text = brand,
                        color = if (isSelected) OledBlack else TextMuted,
                        fontSize = 9.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Normal
                    )
                }
            }
        }

        // Status row
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "MATCHING: ${entries.size} REMOTES",
                color = TacticalAmber,
                fontSize = 10.sp,
                fontFamily = FontFamily.Monospace
            )
            val isBle = QLinkClient.instance.isBleConnected()
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                Icon(
                    Icons.Default.Bluetooth,
                    contentDescription = null,
                    tint = if (isBle) TacticalGreen else TextMuted,
                    modifier = Modifier.size(12.dp)
                )
                Text(
                    text = if (isBle) "BLE READY" else "BLE STANDBY",
                    color = if (isBle) TacticalGreen else TextMuted,
                    fontSize = 9.sp,
                    fontFamily = FontFamily.Monospace
                )
            }
        }

        // Remotes List
        if (isLoading) {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                CircularProgressIndicator(color = TacticalCyan)
            }
        } else if (entries.isEmpty()) {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Text(
                    text = "(NO MATCHING REMOTES FOUND)",
                    color = TextMuted,
                    fontFamily = FontFamily.Monospace,
                    fontSize = 12.sp
                )
            }
        } else {
            LazyColumn(
                modifier = Modifier.fillMaxSize(),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(entries) { entry ->
                    val isFav = favorites.contains(entry.path)
                    RemoteEntryCard(
                        entry = entry,
                        isFavorite = isFav,
                        onToggleFavorite = { viewModel.toggleFavorite(entry) },
                        onPreview = { viewModel.openPreview(entry) },
                        onFlash = { viewModel.flashRemoteToWatch(entry) }
                    )
                }
            }
        }
    }

    // Remote Preview Sheet / Dialog
    if (previewRemote != null || isPreviewLoading) {
        Dialog(onDismissRequest = { viewModel.closePreview() }) {
            Surface(
                modifier = Modifier
                    .fillMaxWidth()
                    .heightIn(max = 680.dp),
                shape = RoundedCornerShape(12.dp),
                color = TacticalSurface,
                border = androidx.compose.foundation.BorderStroke(1.dp, TacticalCyan)
            ) {
                var previewTab by remember { mutableStateOf("REMOTE") } // "REMOTE" or "CODES"
                val scrollState = rememberScrollState()

                Column(
                    modifier = Modifier
                        .padding(14.dp)
                        .verticalScroll(scrollState),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    if (isPreviewLoading) {
                        Box(modifier = Modifier.fillMaxWidth().height(180.dp), contentAlignment = Alignment.Center) {
                            CircularProgressIndicator(color = TacticalCyan)
                        }
                    } else if (previewRemote != null) {
                        val remote = previewRemote!!

                        // Top bar inside Dialog with Tab Switcher & Close
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Row(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(6.dp))
                                    .background(TacticalSurfaceLow)
                                    .border(1.dp, TacticalBorder, RoundedCornerShape(6.dp))
                                    .padding(2.dp)
                            ) {
                                Box(
                                    modifier = Modifier
                                        .clip(RoundedCornerShape(4.dp))
                                        .background(if (previewTab == "REMOTE") TacticalCyan.copy(alpha = 0.25f) else Color.Transparent)
                                        .clickable { previewTab = "REMOTE" }
                                        .padding(horizontal = 8.dp, vertical = 4.dp)
                                ) {
                                    Text(
                                        text = "VIRTUAL REMOTE",
                                        color = if (previewTab == "REMOTE") TacticalCyan else TextMuted,
                                        fontSize = 9.sp,
                                        fontFamily = FontFamily.Monospace,
                                        fontWeight = FontWeight.Bold
                                    )
                                }

                                Box(
                                    modifier = Modifier
                                        .clip(RoundedCornerShape(4.dp))
                                        .background(if (previewTab == "CODES") TacticalAmber.copy(alpha = 0.25f) else Color.Transparent)
                                        .clickable { previewTab = "CODES" }
                                        .padding(horizontal = 8.dp, vertical = 4.dp)
                                ) {
                                    Text(
                                        text = "SIGNAL CODES",
                                        color = if (previewTab == "CODES") TacticalAmber else TextMuted,
                                        fontSize = 9.sp,
                                        fontFamily = FontFamily.Monospace,
                                        fontWeight = FontWeight.Bold
                                    )
                                }
                            }

                            IconButton(onClick = { viewModel.closePreview() }) {
                                Icon(Icons.Default.Close, contentDescription = "Close", tint = TextMuted)
                            }
                        }

                        if (previewTab == "REMOTE") {
                            // Adaptive Virtual Remote (TV, AC, or RGB LED)
                            AdaptiveVirtualRemote(
                                remoteFile = remote,
                                onTransmit = { btn -> viewModel.transmitButton(btn) },
                                onFlashToWatch = {
                                    viewModel.closePreview()
                                    viewModel.flashRemoteToWatch(remote.entry)
                                }
                            )
                        } else {
                            // Raw Parsed Signal Codes
                            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                                Text(
                                    text = "PARSED SIGNALS (${remote.buttons.size}):",
                                    color = TextMuted,
                                    fontSize = 9.sp,
                                    fontFamily = FontFamily.Monospace
                                )

                                remote.buttons.forEach { btn ->
                                    Row(
                                        modifier = Modifier
                                            .fillMaxWidth()
                                            .clip(RoundedCornerShape(4.dp))
                                            .background(TacticalSurfaceVariant)
                                            .clickable { viewModel.transmitButton(btn) }
                                            .padding(horizontal = 8.dp, vertical = 6.dp),
                                        horizontalArrangement = Arrangement.SpaceBetween,
                                        verticalAlignment = Alignment.CenterVertically
                                    ) {
                                        Text(
                                            text = btn.name,
                                            color = TextPrimary,
                                            fontSize = 11.sp,
                                            fontFamily = FontFamily.Monospace,
                                            fontWeight = FontWeight.Bold
                                        )
                                        Text(
                                            text = if (btn.protocol.isNotEmpty()) "${btn.protocol} [${btn.command}]" else "RAW (${btn.rawTimingsCount}t)",
                                            color = TacticalAmber,
                                            fontSize = 9.sp,
                                            fontFamily = FontFamily.Monospace
                                        )
                                    }
                                }

                                Spacer(modifier = Modifier.height(6.dp))

                                Button(
                                    onClick = {
                                        viewModel.closePreview()
                                        viewModel.flashRemoteToWatch(remote.entry)
                                    },
                                    colors = ButtonDefaults.buttonColors(containerColor = TacticalCyan),
                                    shape = RoundedCornerShape(6.dp),
                                    modifier = Modifier.fillMaxWidth()
                                ) {
                                    Icon(Icons.Default.Bluetooth, contentDescription = null, tint = OledBlack, modifier = Modifier.size(16.dp))
                                    Spacer(modifier = Modifier.width(6.dp))
                                    Text("FLASH TO Q-WATCH VIA BLE", color = OledBlack, fontSize = 11.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Transfer Progress Dialog
    if (transferProgress.status != IrdbTransferStatus.IDLE) {
        Dialog(onDismissRequest = {
            if (transferProgress.status == IrdbTransferStatus.SUCCESS || transferProgress.status == IrdbTransferStatus.ERROR) {
                viewModel.dismissTransferDialog()
            }
        }) {
            Surface(
                modifier = Modifier.fillMaxWidth().padding(8.dp),
                shape = RoundedCornerShape(8.dp),
                color = TacticalSurface,
                border = androidx.compose.foundation.BorderStroke(
                    1.dp,
                    when (transferProgress.status) {
                        IrdbTransferStatus.SUCCESS -> TacticalGreen
                        IrdbTransferStatus.ERROR -> TacticalRed
                        else -> TacticalCyan
                    }
                )
            ) {
                Column(
                    modifier = Modifier.padding(20.dp),
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.spacedBy(14.dp)
                ) {
                    when (transferProgress.status) {
                        IrdbTransferStatus.DOWNLOADING, IrdbTransferStatus.CONNECTING_BLE, IrdbTransferStatus.UPLOADING -> {
                            CircularProgressIndicator(
                                progress = { transferProgress.percent / 100f },
                                color = TacticalCyan,
                                modifier = Modifier.size(54.dp)
                            )
                        }
                        IrdbTransferStatus.SUCCESS -> {
                            Icon(Icons.Default.CheckCircle, contentDescription = null, tint = TacticalGreen, modifier = Modifier.size(54.dp))
                        }
                        IrdbTransferStatus.ERROR -> {
                            Icon(Icons.Default.Error, contentDescription = null, tint = TacticalRed, modifier = Modifier.size(54.dp))
                        }
                        else -> {}
                    }

                    Text(
                        text = transferProgress.message,
                        color = TextPrimary,
                        fontSize = 12.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )

                    if (transferProgress.status == IrdbTransferStatus.UPLOADING) {
                        LinearProgressIndicator(
                            progress = { transferProgress.percent / 100f },
                            modifier = Modifier.fillMaxWidth(),
                            color = TacticalCyan
                        )
                        Text(
                            text = "${transferProgress.percent}% (${transferProgress.bytesTransferred} / ${transferProgress.totalBytes} B)",
                            color = TacticalAmber,
                            fontSize = 10.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }

                    if (transferProgress.error != null) {
                        Text(
                            text = transferProgress.error ?: "",
                            color = TacticalRed,
                            fontSize = 10.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }

                    if (transferProgress.status == IrdbTransferStatus.SUCCESS || transferProgress.status == IrdbTransferStatus.ERROR) {
                        Button(
                            onClick = { viewModel.dismissTransferDialog() },
                            colors = ButtonDefaults.buttonColors(
                                containerColor = if (transferProgress.status == IrdbTransferStatus.SUCCESS) TacticalGreen else TacticalRed
                            ),
                            shape = RoundedCornerShape(6.dp),
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Text("DONE", color = OledBlack, fontSize = 11.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun RemoteEntryCard(
    entry: IrdbEntry,
    isFavorite: Boolean,
    onToggleFavorite: () -> Unit,
    onPreview: () -> Unit,
    onFlash: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(6.dp))
            .background(TacticalSurface)
            .padding(12.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(3.dp))
                        .background(TacticalAmberDim)
                        .padding(horizontal = 4.dp, vertical = 2.dp)
                ) {
                    Text(
                        text = entry.deviceType.uppercase(),
                        color = TacticalAmber,
                        fontSize = 8.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                }
                Text(
                    text = entry.cleanDisplayName,
                    color = TextPrimary,
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
            }
            Spacer(modifier = Modifier.height(2.dp))
            Text(
                text = entry.filename,
                color = TextMuted,
                fontSize = 9.sp,
                fontFamily = FontFamily.Monospace,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            if (entry.additionalInfo.isNotBlank()) {
                Text(
                    text = entry.additionalInfo,
                    color = TextSecondary,
                    fontSize = 8.sp,
                    fontFamily = FontFamily.Monospace,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
            }
        }

        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            IconButton(onClick = onToggleFavorite, modifier = Modifier.size(32.dp)) {
                Icon(
                    if (isFavorite) Icons.Default.Star else Icons.Default.StarBorder,
                    contentDescription = "Favorite",
                    tint = if (isFavorite) TacticalAmber else TextMuted,
                    modifier = Modifier.size(18.dp)
                )
            }

            IconButton(onClick = onPreview, modifier = Modifier.size(32.dp)) {
                Icon(
                    Icons.Default.Visibility,
                    contentDescription = "Preview",
                    tint = TacticalCyan,
                    modifier = Modifier.size(18.dp)
                )
            }

            Button(
                onClick = onFlash,
                colors = ButtonDefaults.buttonColors(containerColor = TacticalCyanDim),
                contentPadding = PaddingValues(horizontal = 8.dp, vertical = 4.dp),
                shape = RoundedCornerShape(4.dp),
                modifier = Modifier.height(28.dp)
            ) {
                Icon(Icons.Default.Bluetooth, contentDescription = null, tint = TacticalCyan, modifier = Modifier.size(12.dp))
                Spacer(modifier = Modifier.width(3.dp))
                Text("FLASH", color = TacticalCyan, fontSize = 9.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
            }
        }
    }
}
