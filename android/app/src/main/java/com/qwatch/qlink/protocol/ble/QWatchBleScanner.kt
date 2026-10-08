package com.qwatch.qlink.protocol.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.ParcelUuid
import com.qwatch.qlink.protocol.QLinkConstants
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

data class DiscoveredBleDevice(
    val name: String,
    val address: String,
    val rssi: Int,
    val isBonded: Boolean
)

@SuppressLint("MissingPermission")
class QWatchBleScanner(
    private val context: Context,
    private val scope: CoroutineScope
) {
    private val bluetoothManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager
    private val bluetoothAdapter: BluetoothAdapter? = bluetoothManager?.adapter

    private val _isScanning = MutableStateFlow(false)
    val isScanning: StateFlow<Boolean> = _isScanning.asStateFlow()

    private val _scanResults = MutableStateFlow<List<DiscoveredBleDevice>>(emptyList())
    val scanResults: StateFlow<List<DiscoveredBleDevice>> = _scanResults.asStateFlow()

    private val deviceMap = mutableMapOf<String, DiscoveredBleDevice>()

    private val bondStateReceiver = object : BroadcastReceiver() {
        override fun onReceive(c: Context?, intent: Intent?) {
            if (intent?.action == BluetoothDevice.ACTION_BOND_STATE_CHANGED) {
                val device = intent.getParcelableExtra<BluetoothDevice>(BluetoothDevice.EXTRA_DEVICE)
                val bondState = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, BluetoothDevice.BOND_NONE)
                if (device != null) {
                    val addr = device.address
                    val isBonded = bondState == BluetoothDevice.BOND_BONDED
                    deviceMap[addr]?.let { existing ->
                        deviceMap[addr] = existing.copy(isBonded = isBonded)
                        _scanResults.value = deviceMap.values.sortedByDescending { it.rssi }
                    }
                }
            }
        }
    }

    init {
        try {
            val filter = IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED)
            context.registerReceiver(bondStateReceiver, filter)
        } catch (_: Exception) {}
        loadBondedDevices()
    }

    fun loadBondedDevices() {
        try {
            val bonded = bluetoothAdapter?.bondedDevices ?: emptySet()
            for (dev in bonded) {
                val name = dev.name ?: "Bonded Device"
                deviceMap[dev.address] = DiscoveredBleDevice(
                    name = name,
                    address = dev.address,
                    rssi = -50,
                    isBonded = true
                )
            }
            _scanResults.value = deviceMap.values.sortedByDescending { it.rssi }
        } catch (_: Exception) {}
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult?) {
            result ?: return
            val dev = result.device ?: return
            val name = dev.name ?: result.scanRecord?.deviceName ?: "Unknown BLE Device"
            val addr = dev.address
            val rssi = result.rssi
            val isBonded = dev.bondState == BluetoothDevice.BOND_BONDED

            // Only add devices with a name, or matching Q-Watch service/name
            val hasQWatchService = result.scanRecord?.serviceUuids?.any {
                it.uuid == QLinkConstants.SERVICE_UUID
            } == true
            val isQWatchName = name.contains("Q-Watch", ignoreCase = true) ||
                    name.contains("QWatch", ignoreCase = true) ||
                    name.contains("Watch", ignoreCase = true)

            // Prioritize Q-Watch devices, but also include discovered peripherals with valid names
            if (isQWatchName || hasQWatchService || dev.bondState == BluetoothDevice.BOND_BONDED || name != "Unknown BLE Device") {
                deviceMap[addr] = DiscoveredBleDevice(
                    name = if (isQWatchName && name == "Unknown BLE Device") "Q-Watch" else name,
                    address = addr,
                    rssi = rssi,
                    isBonded = isBonded
                )
                scope.launch(Dispatchers.Main) {
                    _scanResults.value = deviceMap.values.sortedByDescending { it.rssi }
                }
            }
        }

        override fun onBatchScanResults(results: MutableList<ScanResult>?) {
            results?.forEach { onScanResult(ScanSettings.CALLBACK_TYPE_ALL_MATCHES, it) }
        }

        override fun onScanFailed(errorCode: Int) {
            _isScanning.value = false
        }
    }

    fun startScan() {
        if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled) return
        val scanner = bluetoothAdapter.bluetoothLeScanner ?: return
        if (_isScanning.value) return

        deviceMap.clear()
        loadBondedDevices()

        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        val filters = listOf(
            ScanFilter.Builder().setServiceUuid(ParcelUuid(QLinkConstants.SERVICE_UUID)).build()
        )

        try {
            _isScanning.value = true
            // Start scanning: with filter fallback to all if needed
            scanner.startScan(null, settings, scanCallback)
        } catch (_: Exception) {
            _isScanning.value = false
        }
    }

    fun stopScan() {
        if (!_isScanning.value) return
        try {
            val scanner = bluetoothAdapter?.bluetoothLeScanner
            scanner?.stopScan(scanCallback)
        } catch (_: Exception) {}
        _isScanning.value = false
    }

    fun createBond(address: String): Boolean {
        try {
            val dev = bluetoothAdapter?.getRemoteDevice(address) ?: return false
            if (dev.bondState == BluetoothDevice.BOND_BONDED) return true
            return dev.createBond()
        } catch (_: Exception) {
            return false
        }
    }

    fun removeBond(address: String): Boolean {
        try {
            val dev = bluetoothAdapter?.getRemoteDevice(address) ?: return false
            val method = dev.javaClass.getMethod("removeBond")
            return method.invoke(dev) as? Boolean ?: false
        } catch (_: Exception) {
            return false
        }
    }

    fun cleanUp() {
        stopScan()
        try {
            context.unregisterReceiver(bondStateReceiver)
        } catch (_: Exception) {}
    }
}
