package com.zopapa.control

import android.os.Handler
import android.os.Looper
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.EventChannel
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
    private val methodChannelName = "com.zopapa.control/bt"
    private val eventChannelName = "com.zopapa.control/bt/events"
    private val mainHandler = Handler(Looper.getMainLooper())

    private var eventSink: EventChannel.EventSink? = null
    private lateinit var btLink: BtLink

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)

        btLink = BtLink { type, data ->
            mainHandler.post { eventSink?.success(mapOf("type" to type, "data" to data)) }
        }

        EventChannel(flutterEngine.dartExecutor.binaryMessenger, eventChannelName).setStreamHandler(
            object : EventChannel.StreamHandler {
                override fun onListen(arguments: Any?, sink: EventChannel.EventSink) {
                    eventSink = sink
                }

                override fun onCancel(arguments: Any?) {
                    eventSink = null
                }
            }
        )

        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, methodChannelName).setMethodCallHandler { call, result ->
            when (call.method) {
                "bondedDevices" -> result.success(btLink.bondedDevices())

                "connect" -> {
                    val address = call.argument<String>("address")
                    if (address == null) {
                        result.error("bad_args", "Falta 'address'", null)
                        return@setMethodCallHandler
                    }
                    btLink.connect(address) { ok, error ->
                        mainHandler.post {
                            if (ok) result.success(null) else result.error("connect_failed", error, null)
                        }
                    }
                }

                "disconnect" -> {
                    btLink.disconnect()
                    result.success(null)
                }

                "write" -> {
                    val line = call.argument<String>("line")
                    if (line == null) {
                        result.error("bad_args", "Falta 'line'", null)
                        return@setMethodCallHandler
                    }
                    btLink.write(line) { ok, error ->
                        mainHandler.post {
                            if (ok) result.success(null) else result.error("write_failed", error, null)
                        }
                    }
                }

                else -> result.notImplemented()
            }
        }
    }

    override fun onDestroy() {
        if (::btLink.isInitialized) btLink.disconnect()
        super.onDestroy()
    }
}
