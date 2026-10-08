package com.qwatch.qlink.irdb

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.IOException
import java.util.concurrent.TimeUnit

class IrdbRepository(private val context: Context) {

    private val httpClient = OkHttpClient.Builder()
        .connectTimeout(15, TimeUnit.SECONDS)
        .readTimeout(20, TimeUnit.SECONDS)
        .build()

    private val prefs = context.getSharedPreferences("irdb_prefs", Context.MODE_PRIVATE)
    private val cacheFile = File(context.cacheDir, "flipper_irdb_database.json")

    private var memoryCache: List<IrdbEntry>? = null

    suspend fun getDatabase(forceRefresh: Boolean = false): List<IrdbEntry> = withContext(Dispatchers.IO) {
        if (!forceRefresh && memoryCache != null) {
            return@withContext memoryCache!!
        }

        // Try local disk cache first if not forcing refresh
        if (!forceRefresh && cacheFile.exists() && cacheFile.length() > 1000) {
            try {
                val jsonStr = cacheFile.readText(Charsets.UTF_8)
                val list = parseDatabaseJson(jsonStr)
                if (list.isNotEmpty()) {
                    memoryCache = list
                    return@withContext list
                }
            } catch (_: Exception) {}
        }

        // Try downloading online database from search.flippertools.net
        try {
            val req = Request.Builder()
                .url("https://search.flippertools.net/flipper_irdb_database.json")
                .header("User-Agent", "Q-Link-Android/1.0")
                .build()

            val resp = httpClient.newCall(req).execute()
            if (resp.isSuccessful) {
                val bodyStr = resp.body?.string() ?: ""
                if (bodyStr.isNotBlank()) {
                    cacheFile.writeText(bodyStr, Charsets.UTF_8)
                    val list = parseDatabaseJson(bodyStr)
                    if (list.isNotEmpty()) {
                        memoryCache = list
                        return@withContext list
                    }
                }
            }
        } catch (_: Exception) {}

        // Fallback: If cache exists, use it
        if (cacheFile.exists()) {
            try {
                val jsonStr = cacheFile.readText(Charsets.UTF_8)
                val list = parseDatabaseJson(jsonStr)
                if (list.isNotEmpty()) {
                    memoryCache = list
                    return@withContext list
                }
            } catch (_: Exception) {}
        }

        // Embedded offline fallback of curated popular remotes
        val fallback = getCuratedFallbackDatabase()
        memoryCache = fallback
        fallback
    }

    suspend fun fetchRemoteContent(entry: IrdbEntry): Result<IrRemoteFile> = withContext(Dispatchers.IO) {
        val targetFile = File(context.cacheDir, "ir_remotes/" + entry.filename)
        if (targetFile.exists() && targetFile.length() > 50) {
            val text = targetFile.readText(Charsets.UTF_8)
            return@withContext Result.success(IrdbParser.parse(entry, text))
        }

        try {
            val req = Request.Builder()
                .url(entry.rawDownloadUrl)
                .header("User-Agent", "Q-Link-Android/1.0")
                .build()

            val resp = httpClient.newCall(req).execute()
            if (!resp.isSuccessful) {
                return@withContext Result.failure(IOException("HTTP error ${resp.code} downloading remote"))
            }

            val text = resp.body?.string() ?: ""
            if (text.isBlank()) {
                return@withContext Result.failure(IOException("Empty remote file downloaded"))
            }

            targetFile.parentFile?.mkdirs()
            targetFile.writeText(text, Charsets.UTF_8)
            Result.success(IrdbParser.parse(entry, text))
        } catch (e: Exception) {
            // Check if we have embedded sample for this entry
            val sample = getSampleRemoteContent(entry)
            if (sample != null) {
                return@withContext Result.success(IrdbParser.parse(entry, sample))
            }
            Result.failure(e)
        }
    }

    fun getFavorites(): Set<String> {
        return prefs.getStringSet("favorite_paths", emptySet()) ?: emptySet()
    }

    fun toggleFavorite(path: String): Boolean {
        val favs = getFavorites().toMutableSet()
        val isFav = if (favs.contains(path)) {
            favs.remove(path)
            false
        } else {
            favs.add(path)
            true
        }
        prefs.edit().putStringSet("favorite_paths", favs).apply()
        return isFav
    }

    private fun parseDatabaseJson(jsonStr: String): List<IrdbEntry> {
        val list = mutableListOf<IrdbEntry>()
        val array = JSONArray(jsonStr)
        for (i in 0 until array.length()) {
            val obj = array.getJSONObject(i)
            val fn = obj.optString("filename", "")
            val path = obj.optString("path", "")
            if (fn.isBlank() || path.isBlank()) continue

            list.add(
                IrdbEntry(
                    filename = fn,
                    deviceType = obj.optString("device_type", "Misc"),
                    brand = obj.optString("brand", "Generic"),
                    model = obj.optString("model", fn.removeSuffix(".ir")),
                    series = obj.optString("series", ""),
                    path = path,
                    additionalInfo = obj.optString("additional_info", "")
                )
            )
        }
        return list
    }

    private fun getCuratedFallbackDatabase(): List<IrdbEntry> {
        return listOf(
            IrdbEntry("Samsung_BN59.ir", "TVs", "Samsung", "BN59 Series Universal", "", "TVs/Samsung/Samsung_BN59.ir", "Samsung Smart TV Universal Remote"),
            IrdbEntry("Samsung_Smart_TV.ir", "TVs", "Samsung", "Smart TV 4K", "", "TVs/Samsung/Samsung_Smart_TV.ir", "Samsung QLED/Crystal 4K Remote"),
            IrdbEntry("LG_AKB75095307.ir", "TVs", "LG", "AKB75095307 Universal", "", "TVs/LG/LG_AKB75095307.ir", "LG WebOS TV Remote"),
            IrdbEntry("LG_Magic_Remote.ir", "TVs", "LG", "Magic Remote OLED/NanoCell", "", "TVs/LG/LG_Magic_Remote.ir", "LG OLED/NanoCell IR Codes"),
            IrdbEntry("Sony_RMT-TX100U.ir", "TVs", "Sony", "Bravia RMT-TX100U", "", "TVs/Sony/Sony_RMT-TX100U.ir", "Sony Bravia Android TV"),
            IrdbEntry("Apple_TV_A1294.ir", "Media", "Apple", "Apple TV A1294 Silver", "", "Media_Players/Apple/Apple_TV_A1294.ir", "Apple TV 2nd/3rd/4th Gen IR"),
            IrdbEntry("Panasonic_N2QAYB.ir", "TVs", "Panasonic", "N2QAYB Viera", "", "TVs/Panasonic/Panasonic_N2QAYB.ir", "Panasonic Viera Plasma & LED"),
            IrdbEntry("Philips_RC4703.ir", "TVs", "Philips", "RC4703 Ambilight", "", "TVs/Philips/Philips_RC4703.ir", "Philips Ambilight 4K TV"),
            IrdbEntry("TCL_RC802V.ir", "TVs", "TCL", "RC802V Roku & Android TV", "", "TVs/TCL/TCL_RC802V.ir", "TCL Roku / Google TV"),
            IrdbEntry("Toshiba_CT-90325.ir", "TVs", "Toshiba", "CT-90325 Regza", "", "TVs/Toshiba/Toshiba_CT-90325.ir", "Toshiba Regza LCD TV"),
            IrdbEntry("Vizio_XRT136.ir", "TVs", "Vizio", "SmartCast XRT136", "", "TVs/Vizio/Vizio_XRT136.ir", "Vizio SmartCast D/E/M-Series"),
            IrdbEntry("Daikin_ARC433.ir", "ACs", "Daikin", "Inverter ARC433", "", "ACs/Daikin/Daikin_ARC433.ir", "Daikin Split Air Conditioner"),
            IrdbEntry("Gree_YAP1F.ir", "ACs", "Gree", "YAP1F Split AC", "", "ACs/Gree/Gree_YAP1F.ir", "Gree / Cooper&Hunter Air Conditioner"),
            IrdbEntry("Mitsubishi_MSZ.ir", "ACs", "Mitsubishi", "MSZ Electric Inverter", "", "ACs/Mitsubishi/Mitsubishi_MSZ.ir", "Mitsubishi Electric Heavy Industries"),
            IrdbEntry("LG_43UD79-B.ir", "Monitors", "LG", "43UD79-B 4K UHD", "", "Monitors/LG/LG_43UD79-B.ir", "LG 4K UHD IPS LED Monitor"),
            IrdbEntry("Dell_U3818DW.ir", "Monitors", "Dell", "UltraSharp U3818DW", "", "Monitors/Dell/Dell_U3818DW.ir", "Dell UltraSharp Curved Monitor"),
            IrdbEntry("Bose_Solo_5.ir", "SoundBars", "Bose", "Solo 5 Soundbar", "", "Audio/Bose/Bose_Solo_5.ir", "Bose Solo 5 TV Sound System"),
            IrdbEntry("Sonos_Playbar.ir", "SoundBars", "Sonos", "Playbar & Arc", "", "Audio/Sonos/Sonos_Playbar.ir", "Sonos Optical & Arc Soundbar"),
            IrdbEntry("Epson_EMP.ir", "Projectors", "Epson", "PowerLite EMP Projector", "", "Projectors/Epson/Epson_EMP.ir", "Epson 3LCD Classroom Projector"),
            IrdbEntry("Dyson_AM07.ir", "Fans", "Dyson", "Cool Tower AM07", "", "Fans/Dyson/Dyson_AM07.ir", "Dyson Bladeless Fan")
        )
    }

    private fun getSampleRemoteContent(entry: IrdbEntry): String? {
        if (entry.brand.equals("Samsung", ignoreCase = true)) {
            return """
Filetype: IR signals file
Version: 1
#
name: Power
type: parsed
protocol: Samsung32
address: 07 07 00 00
command: 02 02 00 00
#
name: Vol_up
type: parsed
protocol: Samsung32
address: 07 07 00 00
command: 07 07 00 00
#
name: Vol_dn
type: parsed
protocol: Samsung32
address: 07 07 00 00
command: 0B 0B 00 00
#
name: Mute
type: parsed
protocol: Samsung32
address: 07 07 00 00
command: 0F 0F 00 00
#
name: Source
type: parsed
protocol: Samsung32
address: 07 07 00 00
command: 01 01 00 00
#
name: Menu
type: parsed
protocol: Samsung32
address: 07 07 00 00
command: 1A 1A 00 00
            """.trimIndent()
        } else if (entry.brand.equals("LG", ignoreCase = true)) {
            return """
Filetype: IR signals file
Version: 1
#
name: Power
type: parsed
protocol: NEC
address: 04 00 00 00
command: 08 00 00 00
#
name: Vol_up
type: parsed
protocol: NEC
address: 04 00 00 00
command: 02 00 00 00
#
name: Vol_dn
type: parsed
protocol: NEC
address: 04 00 00 00
command: 03 00 00 00
#
name: Mute
type: parsed
protocol: NEC
address: 04 00 00 00
command: 09 00 00 00
#
name: Input
type: parsed
protocol: NEC
address: 04 00 00 00
command: 0B 00 00 00
            """.trimIndent()
        }
        return null
    }
}
