package com.robotics.uwb

import android.Manifest
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.View
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.robotics.uwb.databinding.ActivityMainBinding
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.UUID

/**
 * MainActivity – connects to the ESP32 UWB Tag over BLE and displays
 * real-time distances to each anchor.
 *
 * The ESP32 tag exposes:
 *   Service UUID  : 4FAFC201-1FB5-459E-8FCC-C5C9C331914B
 *   Characteristic: BEB5483E-36E1-4688-B7F5-EA07361B26A8  (NOTIFY)
 *
 * Notification payload (12 bytes):
 *   [0]   anchor ID  (uint8)
 *   [1-4] distance   (float, little-endian)
 *
 * Add BLE server code to the Tag sketch (UWB_DW1000_Tag_BLE.ino) to
 * broadcast distances via the above service.
 */
class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    // BLE UUIDs – must match ESP32 tag firmware
    companion object {
        private const val TAG = "UWB_BLE"
        val SERVICE_UUID: UUID       = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
        val CHAR_DISTANCE_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8")
        val CCCD_UUID: UUID          = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        private const val SCAN_TIMEOUT_MS = 10_000L
        private val REQUIRED_PERMISSIONS = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }
        private const val REQUEST_CODE_PERMISSIONS = 101
        private const val ESP32_DEVICE_NAME = "UWB_Tag"
    }

    private var bluetoothAdapter: BluetoothAdapter? = null
    private var bleScanner: BluetoothLeScanner?     = null
    private var gatt: BluetoothGatt?                = null
    private var scanning = false
    private val handler = Handler(Looper.getMainLooper())

    // Distance map: anchorId -> metres
    private val distances = mutableMapOf<Int, Float>()

    // ─────────────────────────────────────────────────────────────────────────
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        val bm = getSystemService(BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bm.adapter
        bleScanner = bluetoothAdapter?.bluetoothLeScanner

        binding.btnScan.setOnClickListener {
            if (scanning) stopScan() else checkPermissionsAndScan()
        }

        binding.btnDisconnect.setOnClickListener { disconnect() }

        updateUi(connected = false)
    }

    override fun onDestroy() {
        super.onDestroy()
        disconnect()
    }

    // ── Permission handling ───────────────────────────────────────────────────
    private fun checkPermissionsAndScan() {
        val missing = REQUIRED_PERMISSIONS.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        if (missing.isEmpty()) startScan()
        else ActivityCompat.requestPermissions(this, missing.toTypedArray(), REQUEST_CODE_PERMISSIONS)
    }

    override fun onRequestPermissionsResult(
        requestCode: Int, permissions: Array<out String>, grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == REQUEST_CODE_PERMISSIONS &&
            grantResults.all { it == PackageManager.PERMISSION_GRANTED }
        ) startScan()
        else toast("Bluetooth permissions denied")
    }

    // ── BLE Scan ──────────────────────────────────────────────────────────────
    private fun startScan() {
        if (bluetoothAdapter?.isEnabled != true) { toast("Enable Bluetooth first"); return }
        scanning = true
        binding.btnScan.text = "Stop Scan"
        binding.tvStatus.text = "Scanning…"
        binding.progressBar.visibility = View.VISIBLE

        val filter = ScanFilter.Builder()
            .setServiceUuid(android.os.ParcelUuid(SERVICE_UUID))
            .build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        bleScanner?.startScan(listOf(filter), settings, scanCallback)

        // Auto-stop after timeout
        handler.postDelayed({
            if (scanning) {
                stopScan()
                toast("Device not found")
            }
        }, SCAN_TIMEOUT_MS)
    }

    private fun stopScan() {
        scanning = false
        binding.btnScan.text = "Scan"
        binding.progressBar.visibility = View.GONE
        bleScanner?.stopScan(scanCallback)
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device
            if (device.name == ESP32_DEVICE_NAME || result.scanRecord
                    ?.serviceUuids?.contains(android.os.ParcelUuid(SERVICE_UUID)) == true
            ) {
                stopScan()
                binding.tvStatus.text = "Found ${device.name ?: device.address} – connecting…"
                connectToDevice(device)
            }
        }
    }

    // ── GATT connection ───────────────────────────────────────────────────────
    private fun connectToDevice(device: BluetoothDevice) {
        gatt = device.connectGatt(this, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
    }

    private fun disconnect() {
        gatt?.disconnect()
        gatt?.close()
        gatt = null
        runOnUiThread { updateUi(connected = false) }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    Log.i(TAG, "Connected – discovering services")
                    runOnUiThread { binding.tvStatus.text = "Connected – discovering services…" }
                    g.discoverServices()
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    Log.i(TAG, "Disconnected")
                    runOnUiThread { updateUi(connected = false) }
                }
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            val characteristic = g.getService(SERVICE_UUID)
                ?.getCharacteristic(CHAR_DISTANCE_UUID)
            if (characteristic != null) {
                enableNotifications(g, characteristic)
                runOnUiThread { updateUi(connected = true) }
            } else {
                Log.e(TAG, "UWB service/characteristic not found")
                runOnUiThread { binding.tvStatus.text = "Service not found on device" }
            }
        }

        override fun onCharacteristicChanged(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic
        ) {
            parseDistancePayload(characteristic.value)
        }

        // API 33+
        override fun onCharacteristicChanged(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            parseDistancePayload(value)
        }
    }

    // ── Enable BLE notifications ──────────────────────────────────────────────
    private fun enableNotifications(
        g: BluetoothGatt,
        characteristic: BluetoothGattCharacteristic
    ) {
        g.setCharacteristicNotification(characteristic, true)
        val descriptor = characteristic.getDescriptor(CCCD_UUID)
        descriptor?.let {
            it.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
            g.writeDescriptor(it)
        }
    }

    // ── Parse incoming distance packet ────────────────────────────────────────
    private fun parseDistancePayload(data: ByteArray) {
        if (data.size < 5) return
        val anchorId = data[0].toInt() and 0xFF
        val dist = ByteBuffer.wrap(data, 1, 4).order(ByteOrder.LITTLE_ENDIAN).float
        distances[anchorId] = dist
        Log.d(TAG, "Anchor $anchorId -> ${"%.2f".format(dist)} m")
        runOnUiThread { updateDistanceDisplay() }
    }

    // ── UI helpers ────────────────────────────────────────────────────────────
    private fun updateUi(connected: Boolean) {
        binding.btnScan.isEnabled       = !connected
        binding.btnDisconnect.isEnabled = connected
        if (!connected) {
            binding.tvStatus.text = "Disconnected"
            binding.tvAnchor1.text = "Anchor 1: --"
            binding.tvAnchor2.text = "Anchor 2: --"
            binding.tvAnchor3.text = "Anchor 3: --"
            distances.clear()
        } else {
            binding.tvStatus.text = "Connected – ranging…"
        }
    }

    private fun updateDistanceDisplay() {
        binding.tvAnchor1.text = "Anchor 1: ${distances[1]?.let { "${"%.2f".format(it)} m" } ?: "--"}"
        binding.tvAnchor2.text = "Anchor 2: ${distances[2]?.let { "${"%.2f".format(it)} m" } ?: "--"}"
        binding.tvAnchor3.text = "Anchor 3: ${distances[3]?.let { "${"%.2f".format(it)} m" } ?: "--"}"
    }

    private fun toast(msg: String) =
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show()
}
