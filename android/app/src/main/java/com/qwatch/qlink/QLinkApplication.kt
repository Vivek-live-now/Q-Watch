package com.qwatch.qlink

import android.app.Application
import com.qwatch.qlink.protocol.QLinkClient

class QLinkApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        // Initialize the Q-Link protocol engine
        QLinkClient.instance.init(this)
    }
}
