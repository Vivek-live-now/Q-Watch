package com.qwatch.qlink.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.qwatch.qlink.irdb.*
import com.qwatch.qlink.model.NotificationPayload
import com.qwatch.qlink.protocol.QLinkClient
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class OnlineIrdbViewModel(application: Application) : AndroidViewModel(application) {

    private val repository = IrdbRepository(application.applicationContext)
    private val client = QLinkClient.instance

    private val _allEntries = MutableStateFlow<List<IrdbEntry>>(emptyList())
    private val _filteredEntries = MutableStateFlow<List<IrdbEntry>>(emptyList())
    val filteredEntries: StateFlow<List<IrdbEntry>> = _filteredEntries.asStateFlow()

    private val _isLoading = MutableStateFlow(false)
    val isLoading: StateFlow<Boolean> = _isLoading.asStateFlow()

    private val _searchQuery = MutableStateFlow("")
    val searchQuery: StateFlow<String> = _searchQuery.asStateFlow()

    private val _selectedCategory = MutableStateFlow("ALL")
    val selectedCategory: StateFlow<String> = _selectedCategory.asStateFlow()

    private val _selectedBrand = MutableStateFlow("ALL")
    val selectedBrand: StateFlow<String> = _selectedBrand.asStateFlow()

    private val _favorites = MutableStateFlow<Set<String>>(emptySet())
    val favorites: StateFlow<Set<String>> = _favorites.asStateFlow()

    private val _previewRemote = MutableStateFlow<IrRemoteFile?>(null)
    val previewRemote: StateFlow<IrRemoteFile?> = _previewRemote.asStateFlow()

    private val _isPreviewLoading = MutableStateFlow(false)
    val isPreviewLoading: StateFlow<Boolean> = _isPreviewLoading.asStateFlow()

    private val _transferProgress = MutableStateFlow(IrdbTransferProgress())
    val transferProgress: StateFlow<IrdbTransferProgress> = _transferProgress.asStateFlow()

    init {
        _favorites.value = repository.getFavorites()
        loadDatabase()
    }

    fun loadDatabase(forceRefresh: Boolean = false) {
        viewModelScope.launch {
            _isLoading.value = true
            try {
                val list = repository.getDatabase(forceRefresh)
                _allEntries.value = list
                applyFilters()
            } catch (_: Exception) {}
            _isLoading.value = false
        }
    }

    fun onSearchQueryChanged(q: String) {
        _searchQuery.value = q
        applyFilters()
    }

    fun onCategorySelected(cat: String) {
        _selectedCategory.value = cat
        applyFilters()
    }

    fun onBrandSelected(brand: String) {
        _selectedBrand.value = brand
        applyFilters()
    }

    fun toggleFavorite(entry: IrdbEntry) {
        repository.toggleFavorite(entry.path)
        _favorites.value = repository.getFavorites()
        if (_selectedCategory.value == "FAVORITES") {
            applyFilters()
        }
    }

    private fun applyFilters() {
        val query = _searchQuery.value.trim().lowercase()
        val cat = _selectedCategory.value
        val brand = _selectedBrand.value
        val favs = _favorites.value

        var list = _allEntries.value

        if (cat == "FAVORITES") {
            list = list.filter { favs.contains(it.path) }
        } else if (cat != "ALL") {
            list = list.filter {
                it.deviceType.contains(cat, ignoreCase = true) ||
                        (cat == "TVs" && it.path.contains("TV", ignoreCase = true)) ||
                        (cat == "ACs" && (it.path.contains("AC", ignoreCase = true) || it.path.contains("Air", ignoreCase = true))) ||
                        (cat == "Monitors" && it.path.contains("Monitor", ignoreCase = true))
            }
        }

        if (brand != "ALL") {
            list = list.filter { it.brand.equals(brand, ignoreCase = true) }
        }

        if (query.isNotEmpty()) {
            list = list.filter {
                it.brand.lowercase().contains(query) ||
                        it.model.lowercase().contains(query) ||
                        it.filename.lowercase().contains(query) ||
                        it.deviceType.lowercase().contains(query) ||
                        it.additionalInfo.lowercase().contains(query)
            }
        }

        _filteredEntries.value = list
    }

    fun openPreview(entry: IrdbEntry) {
        viewModelScope.launch {
            _isPreviewLoading.value = true
            val res = repository.fetchRemoteContent(entry)
            if (res.isSuccess) {
                _previewRemote.value = res.getOrNull()
            }
            _isPreviewLoading.value = false
        }
    }

    fun closePreview() {
        _previewRemote.value = null
    }

    fun flashRemoteToWatch(entry: IrdbEntry) {
        viewModelScope.launch {
            _transferProgress.value = IrdbTransferProgress(
                status = IrdbTransferStatus.DOWNLOADING,
                message = "Downloading remote from Flipper-IRDB repository...",
                percent = 5
            )

            // 1. Download file content
            val res = repository.fetchRemoteContent(entry)
            if (res.isFailure) {
                _transferProgress.value = IrdbTransferProgress(
                    status = IrdbTransferStatus.ERROR,
                    message = "Download Failed",
                    error = res.exceptionOrNull()?.localizedMessage ?: "Failed to download .ir file"
                )
                return@launch
            }
            val remoteFile = res.getOrNull()!!
            val bytes = remoteFile.rawText.toByteArray(Charsets.UTF_8)

            // 2. Check connection (BLE or Wi-Fi)
            if (!client.isConnected()) {
                _transferProgress.value = IrdbTransferProgress(
                    status = IrdbTransferStatus.CONNECTING_BLE,
                    message = "Connecting to Q-Watch via Bluetooth LE...",
                    percent = 20
                )
                val targetMac = client.getSavedBleMac()
                val bleRes = client.connectBle(targetMac)
                if (bleRes.isFailure) {
                    _transferProgress.value = IrdbTransferProgress(
                        status = IrdbTransferStatus.ERROR,
                        message = "Watch Not Connected",
                        error = "Unable to connect to Q-Watch ($targetMac). Please check Bluetooth settings."
                    )
                    return@launch
                }
            }

            // 3. Upload file with progress
            _transferProgress.value = IrdbTransferProgress(
                status = IrdbTransferStatus.UPLOADING,
                message = "Streaming ${entry.filename} to Q-Watch (/ir/)...",
                percent = 30,
                bytesTransferred = 0,
                totalBytes = bytes.size
            )

            val destPath = "/ir/" + entry.filename
            val uploadRes = client.uploadFileWithProgress(destPath, bytes) { p ->
                val calculatedPct = 30 + ((p.percent * 0.65f).toInt())
                _transferProgress.value = IrdbTransferProgress(
                    status = IrdbTransferStatus.UPLOADING,
                    message = "Loading ${entry.cleanDisplayName} to Q-Watch...",
                    percent = calculatedPct.coerceIn(30, 95),
                    bytesTransferred = p.bytesTransferred,
                    totalBytes = p.totalBytes
                )
            }

            if (uploadRes.isSuccess) {
                _transferProgress.value = IrdbTransferProgress(
                    status = IrdbTransferStatus.SUCCESS,
                    message = "Loaded to Q-Watch!",
                    percent = 100,
                    bytesTransferred = bytes.size,
                    totalBytes = bytes.size
                )
                // Trigger toast on watch
                client.sendNotification(
                    NotificationPayload(
                        appName = "IR Remote",
                        title = "Remote Loaded",
                        body = entry.cleanDisplayName,
                        alertStyle = "CHIME"
                    )
                )
            } else {
                _transferProgress.value = IrdbTransferProgress(
                    status = IrdbTransferStatus.ERROR,
                    message = "Transfer Failed",
                    error = uploadRes.exceptionOrNull()?.localizedMessage ?: "Bluetooth file write failed"
                )
            }
        }
    }

    fun dismissTransferDialog() {
        _transferProgress.value = IrdbTransferProgress(status = IrdbTransferStatus.IDLE)
    }
}
