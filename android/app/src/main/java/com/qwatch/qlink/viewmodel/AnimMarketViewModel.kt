package com.qwatch.qlink.viewmodel

import android.content.Context
import androidx.compose.ui.graphics.Color
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.qwatch.qlink.anim.*
import com.qwatch.qlink.protocol.QLinkClient
import com.qwatch.qlink.ui.theme.*
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

class AnimMarketViewModel : ViewModel() {

    private val client = QLinkClient.instance

    private val _catalog = MutableStateFlow<List<AnimMarketItem>>(emptyList())
    val catalog: StateFlow<List<AnimMarketItem>> = _catalog.asStateFlow()

    private val _selectedItem = MutableStateFlow<AnimMarketItem?>(null)
    val selectedItem: StateFlow<AnimMarketItem?> = _selectedItem.asStateFlow()

    private val _activeAnimFile = MutableStateFlow<AnimFile?>(null)
    val activeAnimFile: StateFlow<AnimFile?> = _activeAnimFile.asStateFlow()

    private val _currentFrameIndex = MutableStateFlow(0)
    val currentFrameIndex: StateFlow<Int> = _currentFrameIndex.asStateFlow()

    private val _isPlaying = MutableStateFlow(true)
    val isPlaying: StateFlow<Boolean> = _isPlaying.asStateFlow()

    private val _speedMultiplier = MutableStateFlow(1.0f)
    val speedMultiplier: StateFlow<Float> = _speedMultiplier.asStateFlow()

    private val _selectedCategory = MutableStateFlow(AnimCategory.ALL)
    val selectedCategory: StateFlow<AnimCategory> = _selectedCategory.asStateFlow()

    private val _storageTotalBytes = MutableStateFlow(896L * 1024L) // 896 KB LittleFS default
    private val _storageUsedBytes = MutableStateFlow(480L * 1024L)  // 480 KB default
    val storageTotalBytes: StateFlow<Long> = _storageTotalBytes.asStateFlow()
    val storageUsedBytes: StateFlow<Long> = _storageUsedBytes.asStateFlow()

    private val _favoriteNames = MutableStateFlow<Set<String>>(emptySet())
    val favoriteNames: StateFlow<Set<String>> = _favoriteNames.asStateFlow()

    private val _selectedPhosphor = MutableStateFlow(PhosphorAmber)
    val selectedPhosphor: StateFlow<Color> = _selectedPhosphor.asStateFlow()

    private val _statusMessage = MutableStateFlow<String?>(null)
    val statusMessage: StateFlow<String?> = _statusMessage.asStateFlow()

    private val _isLoading = MutableStateFlow(false)
    val isLoading: StateFlow<Boolean> = _isLoading.asStateFlow()

    private var playbackJob: Job? = null

    init {
        startPlaybackLoop()
    }

    private fun startPlaybackLoop() {
        playbackJob?.cancel()
        playbackJob = viewModelScope.launch {
            while (isActive) {
                val anim = _activeAnimFile.value
                val playing = _isPlaying.value
                if (anim != null && anim.frames.isNotEmpty() && playing) {
                    val frameCount = anim.frames.size
                    _currentFrameIndex.value = (_currentFrameIndex.value + 1) % frameCount

                    val delayMs = ((anim.header.frameDelayMs / _speedMultiplier.value).toLong()).coerceIn(15L, 500L)
                    delay(delayMs)
                } else {
                    delay(50L)
                }
            }
        }
    }

    fun loadCatalog(context: Context) {
        viewModelScope.launch {
            _isLoading.value = true
            try {
                // Load favorites from local SharedPreferences
                val prefs = context.getSharedPreferences("qlink_prefs", Context.MODE_PRIVATE)
                val favs = prefs.getStringSet("favorite_anims", emptySet())?.toSet() ?: emptySet()
                _favoriteNames.value = favs

                // 1. Fetch installed animations on watch via Q-Link
                val installedFilenames = mutableSetOf<String>()
                if (client.isConnected()) {
                    val res = client.listFiles("/mochi")
                    if (res.isSuccess) {
                        res.getOrNull()?.forEach { file ->
                            if (file.name.endsWith(".anim")) {
                                installedFilenames.add(file.name.lowercase())
                            }
                        }
                    }

                    // Query dedicated storage telemetry or fallback to device info
                    val storageRes = client.getStorageTelemetry()
                    if (storageRes.isSuccess) {
                        storageRes.getOrNull()?.let { st ->
                            _storageTotalBytes.value = st.fsTotalBytes
                            _storageUsedBytes.value = st.fsUsedBytes
                        }
                    } else {
                        val infoRes = client.getDeviceInfo()
                        if (infoRes.isSuccess) {
                            infoRes.getOrNull()?.let { info ->
                                if (info.fsTotalBytes > 0) {
                                    _storageTotalBytes.value = info.fsTotalBytes
                                    _storageUsedBytes.value = info.fsUsedBytes
                                }
                            }
                        }
                    }
                }

                // 2. Discover offline animations bundled in Android assets
                val assetList = try {
                    context.assets.list("mochi_market") ?: emptyArray()
                } catch (_: Exception) {
                    emptyArray()
                }

                val items = mutableListOf<AnimMarketItem>()
                for (filename in assetList.sorted()) {
                    if (!filename.endsWith(".anim")) continue
                    val cleanFilename = filename.lowercase()
                    val isInstalled = installedFilenames.contains(cleanFilename)
                    val isFav = favs.contains(filename)

                    // Peek header for frame count, FPS, and validation
                    var frames = 20
                    var delayMs = 50
                    var size = 20L * 1024L
                    try {
                        context.assets.open("mochi_market/$filename").use { stream ->
                            val headerBytes = ByteArray(16)
                            val read = stream.read(headerBytes)
                            if (read == 16) {
                                AnimParser.parseHeader(headerBytes)?.let { hdr ->
                                    frames = hdr.frameCount
                                    delayMs = hdr.frameDelayMs
                                    size = 16L + (frames * 1024L)
                                }
                            }
                        }
                    } catch (_: Exception) {}

                    val fps = if (delayMs > 0) (1000 / delayMs) else 20
                    val durationSec = (frames * delayMs) / 1000f

                    items.add(
                        AnimMarketItem(
                            name = filename,
                            displayName = AnimClassifier.formatDisplayName(filename),
                            category = AnimClassifier.classify(filename),
                            frameCount = frames,
                            sizeBytes = size,
                            isInstalled = isInstalled,
                            deployState = if (isInstalled) AnimDeployState.DEPLOYED else AnimDeployState.IDLE,
                            isFavorite = isFav,
                            isDuplicate = isInstalled,
                            fps = fps,
                            durationSec = durationSec
                        )
                    )
                }

                // Sort: pinned favorites first, then alphabetical
                _catalog.value = items.sortedWith(compareByDescending<AnimMarketItem> { it.isFavorite }.thenBy { it.displayName })

                // Auto-select first animation for preview if none selected
                if (_selectedItem.value == null && items.isNotEmpty()) {
                    selectAnim(context, items.first())
                }
            } catch (e: Exception) {
                _statusMessage.value = "Error loading catalog: ${e.message}"
            } finally {
                _isLoading.value = false
            }
        }
    }

    fun toggleFavorite(context: Context, item: AnimMarketItem) {
        val currentFavs = _favoriteNames.value.toMutableSet()
        val willBeFav = !currentFavs.contains(item.name)
        if (willBeFav) {
            currentFavs.add(item.name)
        } else {
            currentFavs.remove(item.name)
        }
        _favoriteNames.value = currentFavs

        // Persist to SharedPreferences
        val prefs = context.getSharedPreferences("qlink_prefs", Context.MODE_PRIVATE)
        prefs.edit().putStringSet("favorite_anims", currentFavs).apply()

        // Update items and sort with favorites pinned to top
        _catalog.value = _catalog.value.map {
            if (it.name == item.name) it.copy(isFavorite = willBeFav) else it
        }.sortedWith(compareByDescending<AnimMarketItem> { it.isFavorite }.thenBy { it.displayName })
    }

    fun selectAnim(context: Context, item: AnimMarketItem) {
        _selectedItem.value = item
        _currentFrameIndex.value = 0
        viewModelScope.launch {
            val anim = if (item.isCustom) {
                // If custom item, activeAnimFile is already held
                _activeAnimFile.value
            } else {
                AnimParser.loadFromAssets(context, "mochi_market/${item.name}")
            }
            _activeAnimFile.value = anim
        }
    }

    fun togglePlayPause() {
        _isPlaying.value = !_isPlaying.value
    }

    fun seekFrame(index: Int) {
        val anim = _activeAnimFile.value ?: return
        if (index in anim.frames.indices) {
            _currentFrameIndex.value = index
        }
    }

    fun stepForward() {
        val anim = _activeAnimFile.value ?: return
        if (anim.frames.isNotEmpty()) {
            _currentFrameIndex.value = (_currentFrameIndex.value + 1) % anim.frames.size
        }
    }

    fun stepBackward() {
        val anim = _activeAnimFile.value ?: return
        if (anim.frames.isNotEmpty()) {
            _currentFrameIndex.value = if (_currentFrameIndex.value > 0) _currentFrameIndex.value - 1 else anim.frames.size - 1
        }
    }

    fun cycleSpeed() {
        _speedMultiplier.value = when (_speedMultiplier.value) {
            1.0f -> 1.5f
            1.5f -> 2.0f
            2.0f -> 0.5f
            else -> 1.0f
        }
    }

    fun setCategory(category: AnimCategory) {
        _selectedCategory.value = category
    }

    fun setPhosphorColor(color: Color) {
        _selectedPhosphor.value = color
    }

    fun deployToWatch(context: Context, item: AnimMarketItem, isReplace: Boolean = false) {
        viewModelScope.launch {
            if (!client.isConnected()) {
                _statusMessage.value = "Watch not connected! Connect Wi-Fi or BLE first."
                return@launch
            }

            // Available / Free Space Headroom Gating
            val freeBytes = (_storageTotalBytes.value - _storageUsedBytes.value).coerceAtLeast(0L)
            if (!isReplace && item.sizeBytes > freeBytes) {
                _statusMessage.value = "Insufficient LittleFS space! Free: ${freeBytes / 1024} KB, Required: ${item.sizeBytes / 1024} KB."
                return@launch
            }

            updateItemDeployState(item.name, AnimDeployState.DEPLOYING)
            _statusMessage.value = if (isReplace) "Replacing ${item.name} on watch..." else "Streaming ${item.name} to watch /mochi/..."

            try {
                val bytes = if (item.isCustom) {
                    val anim = _activeAnimFile.value
                    if (anim != null) {
                        AnimEncoder.encodeAnimation(
                            bitmaps = anim.frames.map { AnimParser.decodeFrameToBitmap(it) },
                            delayMs = anim.header.frameDelayMs,
                            loop = anim.header.isLooping
                        )
                    } else {
                        throw IllegalStateException("Custom animation data missing")
                    }
                } else {
                    context.assets.open("mochi_market/${item.name}").use { it.readBytes() }
                }

                // Animation Validation using QANM header before transmitting
                val validation = AnimParser.validateQanm(bytes)
                if (!validation.isValid) {
                    updateItemDeployState(item.name, AnimDeployState.FAILED)
                    _statusMessage.value = "QANM validation rejected: ${validation.error}"
                    return@launch
                }

                val targetPath = "/mochi/${item.name}"

                // Stream upload with live progress tracking and failed-transfer recovery
                val res = client.uploadFileWithProgress(targetPath, bytes) { progress ->
                    updateItemProgress(item.name, progress)
                }

                if (res.isSuccess) {
                    updateItemDeployState(item.name, AnimDeployState.DEPLOYED, installed = true)
                    if (!isReplace) {
                        _storageUsedBytes.value += bytes.size
                    }
                    _statusMessage.value = "Deployed ${item.name} successfully!"
                } else {
                    updateItemDeployState(item.name, AnimDeployState.FAILED)
                    _statusMessage.value = "Deploy failed: ${res.exceptionOrNull()?.message}. Partial file purged."
                }
            } catch (e: Exception) {
                updateItemDeployState(item.name, AnimDeployState.FAILED)
                _statusMessage.value = "Deploy error: ${e.message}"
            }
        }
    }

    fun deleteFromWatch(item: AnimMarketItem) {
        viewModelScope.launch {
            if (!client.isConnected()) {
                _statusMessage.value = "Watch not connected!"
                return@launch
            }

            _statusMessage.value = "Deleting ${item.name} from watch..."
            val targetPath = "/mochi/${item.name}"

            // Safe deletion: watch firmware automatically halts playback before unlinking
            val res = client.deleteFile(targetPath)

            if (res.isSuccess) {
                updateItemDeployState(item.name, AnimDeployState.IDLE, installed = false)
                _storageUsedBytes.value = (_storageUsedBytes.value - item.sizeBytes).coerceAtLeast(0L)
                _statusMessage.value = "Safely deleted ${item.name} from LittleFS."
            } else {
                _statusMessage.value = "Delete failed: ${res.exceptionOrNull()?.message}"
            }
        }
    }

    fun setAsBootAnimation(context: Context, item: AnimMarketItem) {
        viewModelScope.launch {
            if (!client.isConnected()) {
                _statusMessage.value = "Watch not connected!"
                return@launch
            }

            _statusMessage.value = "Writing ${item.name} to /boot/boot.anim..."
            try {
                val bytes = context.assets.open("mochi_market/${item.name}").use { it.readBytes() }

                // QANM validation
                val validation = AnimParser.validateQanm(bytes)
                if (!validation.isValid) {
                    _statusMessage.value = "Invalid QANM: ${validation.error}"
                    return@launch
                }

                val res = client.uploadFile("/boot/boot.anim", bytes)

                if (res.isSuccess) {
                    _statusMessage.value = "${item.displayName} assigned as Cold Boot Splash!"
                } else {
                    _statusMessage.value = "Failed to assign boot splash."
                }
            } catch (e: Exception) {
                _statusMessage.value = "Boot splash error: ${e.message}"
            }
        }
    }

    fun importCustomAnim(name: String, bytes: ByteArray) {
        val validation = AnimParser.validateQanm(bytes)
        if (!validation.isValid) {
            _statusMessage.value = "Validation rejected: ${validation.error}"
            return
        }

        val anim = AnimParser.parseAnim(name, bytes) ?: run {
            _statusMessage.value = "Failed to parse animation!"
            return
        }

        val item = AnimMarketItem(
            name = name,
            displayName = AnimClassifier.formatDisplayName(name),
            category = AnimCategory.ACTIONS,
            frameCount = anim.header.frameCount,
            sizeBytes = bytes.size.toLong(),
            isInstalled = false,
            isCustom = true,
            fps = anim.header.fps,
            durationSec = anim.header.durationSec
        )

        _catalog.value = listOf(item) + _catalog.value
        _selectedItem.value = item
        _activeAnimFile.value = anim
        _currentFrameIndex.value = 0
        _statusMessage.value = "Custom animation $name validated and loaded into previewer!"
    }

    private fun updateItemDeployState(name: String, state: AnimDeployState, installed: Boolean? = null) {
        _catalog.value = _catalog.value.map {
            if (it.name == name) {
                it.copy(
                    deployState = state,
                    isInstalled = installed ?: it.isInstalled,
                    isDuplicate = installed ?: it.isDuplicate,
                    transferProgress = if (state != AnimDeployState.DEPLOYING) null else it.transferProgress
                )
            } else {
                it
            }
        }
    }

    private fun updateItemProgress(name: String, progress: TransferProgress) {
        _catalog.value = _catalog.value.map {
            if (it.name == name) {
                it.copy(transferProgress = progress)
            } else {
                it
            }
        }
    }

    fun clearStatus() {
        _statusMessage.value = null
    }

    override fun onCleared() {
        super.onCleared()
        playbackJob?.cancel()
    }
}
