package eu.drmax.musick.companion;

import android.service.notification.NotificationListenerService;

/**
 * Minimal notification listener. Its only jobs are:
 *   1. Existing so the user can grant "Notification access" — that grant is
 *      what authorises {@link android.media.session.MediaSessionManager
 *      #getActiveSessions} (called from {@link MusickService} with this
 *      component as the token).
 *   2. Kicking the foreground service back to life whenever the system
 *      (re)binds the listener, e.g. after the process was killed.
 *
 * All the real work lives in {@link MusickService} so it survives independently
 * of this listener's binding lifecycle.
 */
public class MusickNotificationListener extends NotificationListenerService {

    @Override
    public void onListenerConnected() {
        MusickService.start(this);
    }
}
