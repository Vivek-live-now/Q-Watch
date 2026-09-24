package com.qwatch.qlink.service

import android.app.Notification
import android.content.Context
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import com.qwatch.qlink.model.NotificationPayload
import com.qwatch.qlink.protocol.QLinkClient
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch

class QWatchNotificationListenerService : NotificationListenerService() {

    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    override fun onNotificationPosted(sbn: StatusBarNotification?) {
        if (sbn == null) return

        val prefs = getSharedPreferences("qlink_prefs", Context.MODE_PRIVATE)
        val forwardingEnabled = prefs.getBoolean("forward_notifications", true)
        if (!forwardingEnabled) return

        val notification = sbn.notification ?: return
        val extras = notification.extras ?: return

        // Skip ongoing or silent progress bars (music playback, downloads, foreground services)
        val isOngoing = (notification.flags and Notification.FLAG_ONGOING_EVENT) != 0
        if (isOngoing) return

        val pkgName = sbn.packageName ?: ""
        // Skip own app notifications to prevent feedback loops
        if (pkgName == packageName) return

        val title = extras.getCharSequence(Notification.EXTRA_TITLE)?.toString() ?: ""
        val text = extras.getCharSequence(Notification.EXTRA_TEXT)?.toString() ?: ""

        if (title.isBlank() && text.isBlank()) return

        val appName = try {
            val pm = applicationContext.packageManager
            val appInfo = pm.getApplicationInfo(pkgName, 0)
            pm.getApplicationLabel(appInfo).toString()
        } catch (_: Exception) {
            pkgName.substringAfterLast('.')
        }

        val category = when {
            notification.category == Notification.CATEGORY_CALL -> "CALL"
            notification.category == Notification.CATEGORY_MESSAGE -> "MESSAGE"
            notification.category == Notification.CATEGORY_EMAIL -> "EMAIL"
            else -> "MESSAGE"
        }

        val payload = NotificationPayload(
            appName = appName,
            packageName = pkgName,
            title = title,
            body = text,
            category = category,
            alertStyle = "CHIME",
            ledColorHex = if (category == "CALL") "#FF0055" else "#00E5FF",
            timestampMs = System.currentTimeMillis()
        )

        scope.launch {
            if (QLinkClient.instance.isConnected()) {
                QLinkClient.instance.sendNotification(payload)
            }
        }
    }

    override fun onNotificationRemoved(sbn: StatusBarNotification?) {
        // Optional dismiss sync
    }
}
