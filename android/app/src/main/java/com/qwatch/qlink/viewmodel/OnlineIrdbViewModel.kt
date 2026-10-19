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

    private val _transmitFeedback = MutableStateFlow(IrTransmitFeedback())
    val transmitFeedback: StateFlow<IrTransmitFeedback> = _transmitFeedback.asStateFlow()

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
                        (cat == "LED/RGB" && (it.path.contains("LED", ignoreCase = true) || it.path.contains("Light", ignoreCase = true) || it.path.contains("RGB", ignoreCase = true))) ||
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

    fun transmitButton(btn: IrParsedButton) {
        viewModelScope.launch {
            if (!client.isConnected()) {
                _transmitFeedback.value = IrTransmitFeedback(
                    state = IrTransmitState.FAILED,
                    buttonName = btn.name,
                    message = "Watch not connected (BLE standby)",
                    error = "Not connected"
                )
                return@launch
            }

            if (btn.isProtocolBased) {
                val bits = btn.effectiveBits
                if (bits <= 0 || bits > 128) {
                    _transmitFeedback.value = IrTransmitFeedback(
                        state = IrTransmitState.UNSUPPORTED,
                        buttonName = btn.name,
                        message = "UNSUPPORTED: Invalid bit length ($bits) for protocol ${btn.protocol}",
                        error = "Unsupported bit length"
                    )
                    return@launch
                }

                _transmitFeedback.value = IrTransmitFeedback(
                    state = IrTransmitState.TRANSMITTING,
                    buttonName = btn.name,
                    message = "TRANSMITTING: ${btn.name} [${btn.protocol} ${bits}b]..."
                )

                val res = client.transmitIr(btn.protocol, btn.address, btn.command, bits)
                if (res.isSuccess && res.getOrNull() == true) {
                    _transmitFeedback.value = IrTransmitFeedback(
                        state = IrTransmitState.SUCCESS,
                        buttonName = btn.name,
                        message = "SENT: ${btn.name} [${btn.protocol} ${bits}b]"
                    )
                } else {
                    val err = res.exceptionOrNull()?.localizedMessage ?: "Transmission rejected by watch"
                    _transmitFeedback.value = IrTransmitFeedback(
                        state = IrTransmitState.FAILED,
                        buttonName = btn.name,
                        message = "FAILED: $err",
                        error = err
                    )
                }
            } else if (btn.isRaw || btn.type.equals("raw", ignoreCase = true)) {
                if (btn.rawTimings.isEmpty()) {
                    _transmitFeedback.value = IrTransmitFeedback(
                        state = IrTransmitState.UNSUPPORTED,
                        buttonName = btn.name,
                        message = "UNSUPPORTED: ${btn.name} has no timing data",
                        error = "Empty timings"
                    )
                    return@launch
                }

                if (btn.rawTimings.size > 1024) {
                    _transmitFeedback.value = IrTransmitFeedback(
                        state = IrTransmitState.UNSUPPORTED,
                        buttonName = btn.name,
                        message = "UNSUPPORTED: ${btn.name} timing count (${btn.rawTimings.size}) exceeds limit (1024)",
                        error = "Timings count exceeds 1024"
                    )
                    return@launch
                }

                if (btn.rawTimings.any { it <= 0 || it > 65535 }) {
                    _transmitFeedback.value = IrTransmitFeedback(
                        state = IrTransmitState.UNSUPPORTED,
                        buttonName = btn.name,
                        message = "UNSUPPORTED: ${btn.name} contains invalid timing values",
                        error = "Invalid timing duration"
                    )
                    return@launch
                }

                val freq = if (btn.frequency > 0) btn.frequency.toInt() else 38000
                _transmitFeedback.value = IrTransmitFeedback(
                    state = IrTransmitState.TRANSMITTING,
                    buttonName = btn.name,
                    message = "TRANSMITTING: ${btn.name} [RAW ${btn.rawTimings.size}t ${freq / 1000}kHz]..."
                )

                val res = client.transmitRawIr(btn.rawTimings, freq, btn.dutyCycle)
                if (res.isSuccess && res.getOrNull() == true) {
                    _transmitFeedback.value = IrTransmitFeedback(
                        state = IrTransmitState.SUCCESS,
                        buttonName = btn.name,
                        message = "SENT: ${btn.name} [RAW ${btn.rawTimings.size}t ${freq / 1000}kHz]"
                    )
                } else {
                    val err = res.exceptionOrNull()?.localizedMessage ?: "Watch rejected raw IR transmission"
                    _transmitFeedback.value = IrTransmitFeedback(
                        state = IrTransmitState.FAILED,
                        buttonName = btn.name,
                        message = "FAILED: $err",
                        error = err
                    )
                }
            } else {
                _transmitFeedback.value = IrTransmitFeedback(
                    state = IrTransmitState.UNSUPPORTED,
                    buttonName = btn.name,
                    message = "UNSUPPORTED: ${btn.name} has no valid protocol or raw data",
                    error = "No usable protocol or raw data"
                )
            }
        }
    }

    fun transmitSimulated(syntheticLabel: String) {
        _transmitFeedback.value = IrTransmitFeedback(
            state = IrTransmitState.SIMULATED,
            buttonName = syntheticLabel,
            message = "SIMULATED: No IR signal for $syntheticLabel in remote"
        )
    }

    fun clearTransmitFeedback() {
        _transmitFeedback.value = IrTransmitFeedback()
    }

    fun openSampleRemote(category: String) {
        val entry = when (category.uppercase()) {
            "AC" -> IrdbEntry(
                filename = "Daikin_Inverter.ir",
                deviceType = "Air_Conditioners",
                brand = "Daikin",
                model = "ARC433A",
                path = "Air_Conditioners/Daikin/ARC433A.ir"
            )
            "RGB", "LED" -> IrdbEntry(
                filename = "Magic_Home_RGB.ir",
                deviceType = "LED_Lighting",
                brand = "MagicHome",
                model = "24K_RGB_Strip",
                path = "LED_Lighting/MagicHome/24K.ir"
            )
            else -> IrdbEntry(
                filename = "Samsung_Smart_TV.ir",
                deviceType = "TVs",
                brand = "Samsung",
                model = "BN59-01259D",
                path = "TVs/Samsung/BN59-01259D.ir"
            )
        }
        val sampleButtons = when (category.uppercase()) {
            "AC" -> listOf(
                IrParsedButton("Power", "parsed", "Daikin", "0x11", "0x01"),
                IrParsedButton("Temp_up", "parsed", "Daikin", "0x11", "0x0A"),
                IrParsedButton("Temp_down", "parsed", "Daikin", "0x11", "0x0B"),
                IrParsedButton("Mode", "parsed", "Daikin", "0x11", "0x02"),
                IrParsedButton("Fan", "parsed", "Daikin", "0x11", "0x03"),
                IrParsedButton("Swing", "parsed", "Daikin", "0x11", "0x04"),
                IrParsedButton("Turbo", "parsed", "Daikin", "0x11", "0x05"),
                IrParsedButton("Sleep", "parsed", "Daikin", "0x11", "0x06"),
                IrParsedButton("Eco", "parsed", "Daikin", "0x11", "0x07"),
                IrParsedButton("Timer", "parsed", "Daikin", "0x11", "0x08"),
                IrParsedButton("Clean", "parsed", "Daikin", "0x11", "0x09")
            )
            "RGB", "LED" -> listOf(
                IrParsedButton("Bright_up", "parsed", "NECext", "00 F7 00 00", "00 FF 00 00"),
                IrParsedButton("Bright_down", "parsed", "NECext", "00 F7 00 00", "80 7F 00 00"),
                IrParsedButton("Off", "parsed", "NECext", "00 F7 00 00", "40 BF 00 00"),
                IrParsedButton("On", "parsed", "NECext", "00 F7 00 00", "C0 3F 00 00"),
                IrParsedButton("Red", "parsed", "NECext", "00 F7 00 00", "20 DF 00 00"),
                IrParsedButton("Green", "parsed", "NECext", "00 F7 00 00", "A0 5F 00 00"),
                IrParsedButton("Blue", "parsed", "NECext", "00 F7 00 00", "60 9F 00 00"),
                IrParsedButton("White", "parsed", "NECext", "00 F7 00 00", "E0 1F 00 00"),
                IrParsedButton("R1", "parsed", "NECext", "00 F7 00 00", "10 EF 00 00"),
                IrParsedButton("G1", "parsed", "NECext", "00 F7 00 00", "90 6F 00 00"),
                IrParsedButton("B1", "parsed", "NECext", "00 F7 00 00", "50 AF 00 00"),
                IrParsedButton("Flash", "parsed", "NECext", "00 F7 00 00", "D0 2F 00 00"),
                IrParsedButton("R2", "parsed", "NECext", "00 F7 00 00", "30 CF 00 00"),
                IrParsedButton("G2", "parsed", "NECext", "00 F7 00 00", "B0 4F 00 00"),
                IrParsedButton("B2", "parsed", "NECext", "00 F7 00 00", "70 8F 00 00"),
                IrParsedButton("Strobe", "parsed", "NECext", "00 F7 00 00", "F0 0F 00 00"),
                IrParsedButton("R3", "parsed", "NECext", "00 F7 00 00", "08 F7 00 00"),
                IrParsedButton("G3", "parsed", "NECext", "00 F7 00 00", "88 77 00 00"),
                IrParsedButton("B3", "parsed", "NECext", "00 F7 00 00", "48 B7 00 00"),
                IrParsedButton("Fade", "parsed", "NECext", "00 F7 00 00", "C8 37 00 00"),
                IrParsedButton("R4", "parsed", "NECext", "00 F7 00 00", "28 D7 00 00"),
                IrParsedButton("G4", "parsed", "NECext", "00 F7 00 00", "A8 57 00 00"),
                IrParsedButton("B4", "parsed", "NECext", "00 F7 00 00", "68 97 00 00"),
                IrParsedButton("Smooth", "parsed", "NECext", "00 F7 00 00", "E8 17 00 00")
            )
            else -> listOf(
                IrParsedButton("Power", "parsed", "NEC", "0x04", "0x08"),
                IrParsedButton("Mute", "parsed", "NEC", "0x04", "0x09"),
                IrParsedButton("Input", "parsed", "NEC", "0x04", "0x01"),
                IrParsedButton("Vol_up", "parsed", "NEC", "0x04", "0x02"),
                IrParsedButton("Vol_down", "parsed", "NEC", "0x04", "0x03"),
                IrParsedButton("Ch_up", "parsed", "NEC", "0x04", "0x12"),
                IrParsedButton("Ch_down", "parsed", "NEC", "0x04", "0x13"),
                IrParsedButton("Up", "parsed", "NEC", "0x04", "0x60"),
                IrParsedButton("Down", "parsed", "NEC", "0x04", "0x61"),
                IrParsedButton("Left", "parsed", "NEC", "0x04", "0x62"),
                IrParsedButton("Right", "parsed", "NEC", "0x04", "0x65"),
                IrParsedButton("Ok", "parsed", "NEC", "0x04", "0x68"),
                IrParsedButton("Home", "parsed", "NEC", "0x04", "0x79"),
                IrParsedButton("Back", "parsed", "NEC", "0x04", "0x58"),
                IrParsedButton("Exit", "parsed", "NEC", "0x04", "0x59"),
                IrParsedButton("0", "parsed", "NEC", "0x04", "0x11"),
                IrParsedButton("1", "parsed", "NEC", "0x04", "0x04"),
                IrParsedButton("2", "parsed", "NEC", "0x04", "0x05"),
                IrParsedButton("3", "parsed", "NEC", "0x04", "0x06"),
                IrParsedButton("Red", "parsed", "NEC", "0x04", "0x6C"),
                IrParsedButton("Green", "parsed", "NEC", "0x04", "0x6D"),
                IrParsedButton("Yellow", "parsed", "NEC", "0x04", "0x6E"),
                IrParsedButton("Blue", "parsed", "NEC", "0x04", "0x6F")
            )
        }
        _previewRemote.value = IrRemoteFile(
            entry = entry,
            filetype = "IR signals file",
            rawText = "",
            buttons = sampleButtons
        )
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
                bytesTransferred = 0L,
                totalBytes = bytes.size.toLong()
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
                    bytesTransferred = bytes.size.toLong(),
                    totalBytes = bytes.size.toLong()
                )
                // Trigger toast on watch
                client.sendNotification(
                    NotificationPayload(
                        appName = "IR Remote",
                        packageName = "com.qwatch.irdb",
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
