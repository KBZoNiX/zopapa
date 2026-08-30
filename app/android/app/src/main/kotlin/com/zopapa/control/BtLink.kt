package com.zopapa.control

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothSocket
import java.io.ByteArrayOutputStream
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.util.UUID
import java.util.concurrent.Executors

// Standard Serial Port Profile UUID — the only one an HC-05/HC-06-style module exposes.
private val SPP_UUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

/**
 * Thin wrapper around a classic Bluetooth RFCOMM socket. One connection at a time;
 * [connect] tears down any previous socket first. [connect] and [write] run on a
 * single background executor so callers never block the platform-channel/UI thread.
 * The blocking read loop runs on its own dedicated thread, separate from that
 * executor — it never returns for the life of the connection, so sharing the
 * executor with it would starve every subsequent write() forever.
 */
class BtLink(private val onEvent: (type: String, data: String) -> Unit) {
    private val adapter: BluetoothAdapter? = BluetoothAdapter.getDefaultAdapter()
    private val executor = Executors.newSingleThreadExecutor()

    @Volatile private var socket: BluetoothSocket? = null
    @Volatile private var input: InputStream? = null
    @Volatile private var output: OutputStream? = null
    @Volatile private var reading = false
    private var readThread: Thread? = null

    @SuppressLint("MissingPermission")
    fun bondedDevices(): List<Map<String, String>> {
        val a = adapter ?: return emptyList()
        return a.bondedDevices.map { mapOf("name" to (it.name ?: it.address), "address" to it.address) }
    }

    @SuppressLint("MissingPermission")
    fun connect(address: String, onResult: (Boolean, String?) -> Unit) {
        executor.execute {
            try {
                disconnectInternal()
                val a = adapter ?: throw IOException("Bluetooth no disponible en este dispositivo")
                val device: BluetoothDevice = a.getRemoteDevice(address)
                // No cancelDiscovery() here: we only ever connect to an already-bonded
                // device by address (no discovery UI), and cancelDiscovery() requires
                // BLUETOOTH_SCAN, which this app deliberately doesn't request/declare —
                // calling it threw an uncaught SecurityException that crashed the app.
                val sock = device.createRfcommSocketToServiceRecord(SPP_UUID)
                sock.connect()
                socket = sock
                input = sock.inputStream
                output = sock.outputStream
                reading = true
                onResult(true, null)
                onEvent("state", "connected")
                readThread = Thread(::readLoop, "BtLink-read").also { it.start() }
            } catch (e: Exception) {
                onResult(false, e.message ?: "Error de conexión")
                disconnectInternal()
            }
        }
    }

    private fun readLoop() {
        val buffer = ByteArray(1024)
        // Buffered as raw bytes, not chars: the firmware's Spanish strings are
        // UTF-8 (accented chars are multi-byte), and decoding byte-by-byte via
        // Byte.toChar() mangles them — which silently broke every exact-string
        // match against a line containing an accent (e.g. "Calibración
        // finalizada."), making those commands hang until their timeout.
        val lineBytes = ByteArrayOutputStream()
        val stream = input ?: return
        while (reading) {
            try {
                val count = stream.read(buffer)
                if (count < 0) break
                for (i in 0 until count) {
                    val b = buffer[i]
                    if (b == '\n'.code.toByte()) {
                        val bytes = lineBytes.toByteArray()
                        var len = bytes.size
                        if (len > 0 && bytes[len - 1] == '\r'.code.toByte()) len -= 1
                        lineBytes.reset()
                        if (len > 0) onEvent("line", String(bytes, 0, len, Charsets.UTF_8))
                    } else {
                        lineBytes.write(b.toInt())
                    }
                }
            } catch (e: IOException) {
                break
            }
        }
        disconnectInternal()
    }

    fun write(line: String, onResult: (Boolean, String?) -> Unit) {
        executor.execute {
            try {
                val out = output ?: throw IOException("No conectado")
                out.write((line + "\n").toByteArray(Charsets.US_ASCII))
                out.flush()
                onResult(true, null)
            } catch (e: IOException) {
                onResult(false, e.message ?: "Error de escritura")
            }
        }
    }

    fun disconnect() {
        executor.execute { disconnectInternal() }
    }

    private fun disconnectInternal() {
        val wasReading = reading
        reading = false
        try { input?.close() } catch (_: IOException) {}
        try { output?.close() } catch (_: IOException) {}
        try { socket?.close() } catch (_: IOException) {}
        input = null
        output = null
        socket = null
        readThread = null
        if (wasReading) onEvent("state", "disconnected")
    }
}
