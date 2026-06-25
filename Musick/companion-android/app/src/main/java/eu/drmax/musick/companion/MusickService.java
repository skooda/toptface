package eu.drmax.musick.companion;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.media.AudioManager;
import android.media.MediaMetadata;
import android.media.session.MediaController;
import android.media.session.MediaSessionManager;
import android.media.session.PlaybackState;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;

import com.getpebble.android.kit.PebbleKit;
import com.getpebble.android.kit.util.PebbleDictionary;

import java.util.List;

/**
 * Long-lived foreground service that bridges the phone's media player and the
 * Musick watchapp. Running in the foreground (with an ongoing notification)
 * keeps the process — and therefore the PebbleKit command receiver — alive
 * through Doze / battery optimisation, which is what previously caused the
 * bridge to "stop working" after a while.
 *
 * It reads the active {@link MediaSession} (authorised by the notification
 * access granted to {@link MusickNotificationListener}) and:
 *   - forwards title / artist / play state to the watch;
 *   - applies playback commands the watch sends back.
 */
public class MusickService extends Service
        implements MediaSessionManager.OnActiveSessionsChangedListener {

    private static final String TAG = "MusickService";
    private static final String CHANNEL_ID = "musick_bridge";
    private static final int NOTIF_ID = 1;

    private MediaSessionManager sessionManager;
    private MediaController controller;
    private PebbleKit.PebbleDataReceiver dataReceiver;

    private final MediaController.Callback controllerCallback = new MediaController.Callback() {
        @Override public void onMetadataChanged(MediaMetadata metadata) { pushNowPlaying(); }
        @Override public void onPlaybackStateChanged(PlaybackState state) { pushNowPlaying(); }
        @Override public void onSessionDestroyed() { setController(null); }
    };

    /** Start (or restart) the bridge as a foreground service. */
    public static void start(Context ctx) {
        Intent i = new Intent(ctx, MusickService.class);
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                ctx.startForegroundService(i);
            } else {
                ctx.startService(i);
            }
        } catch (RuntimeException e) {
            // e.g. a background-start restriction before the battery exemption
            // is granted — don't crash the listener/boot path over it.
            Log.w(TAG, "Could not start bridge service", e);
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        goForeground();
        registerPebbleReceiver();
        connectSessions();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // Re-attach in case we were restarted after being killed.
        connectSessions();
        return START_STICKY;
    }

    // ==================== Media sessions ====================

    private void connectSessions() {
        if (sessionManager == null) {
            sessionManager = (MediaSessionManager) getSystemService(Context.MEDIA_SESSION_SERVICE);
        }
        ComponentName nls = new ComponentName(this, MusickNotificationListener.class);
        try {
            sessionManager.addOnActiveSessionsChangedListener(this, nls);
            onActiveSessionsChanged(sessionManager.getActiveSessions(nls));
        } catch (SecurityException e) {
            Log.w(TAG, "Notification access not granted yet", e);
        }
    }

    @Override
    public void onActiveSessionsChanged(List<MediaController> controllers) {
        setController(controllers != null && !controllers.isEmpty() ? controllers.get(0) : null);
    }

    private void setController(MediaController next) {
        if (controller != null) {
            controller.unregisterCallback(controllerCallback);
        }
        controller = next;
        if (controller != null) {
            controller.registerCallback(controllerCallback);
        }
        pushNowPlaying();
    }

    private boolean isPlaying() {
        if (controller == null) return false;
        PlaybackState s = controller.getPlaybackState();
        return s != null && s.getState() == PlaybackState.STATE_PLAYING;
    }

    /** Send the current track + playback state to the watch. */
    private void pushNowPlaying() {
        String title = "Not playing";
        String artist = "";
        if (controller != null) {
            MediaMetadata m = controller.getMetadata();
            if (m != null) {
                String t = m.getString(MediaMetadata.METADATA_KEY_TITLE);
                String a = m.getString(MediaMetadata.METADATA_KEY_ARTIST);
                if (t != null) title = t;
                if (a != null) artist = a;
            }
        }

        PebbleDictionary dict = new PebbleDictionary();
        dict.addString(PebbleConstants.KEY_TITLE, title);
        dict.addString(PebbleConstants.KEY_ARTIST, artist);
        dict.addUint8(PebbleConstants.KEY_STATE, (byte) (isPlaying() ? 1 : 0));
        PebbleKit.sendDataToPebble(getApplicationContext(), PebbleConstants.APP_UUID, dict);
    }

    // ==================== Watch commands ====================

    private void registerPebbleReceiver() {
        dataReceiver = new PebbleKit.PebbleDataReceiver(PebbleConstants.APP_UUID) {
            @Override
            public void receiveData(Context context, int transactionId, PebbleDictionary data) {
                PebbleKit.sendAckToPebble(context, transactionId);
                Long cmd = data.getUnsignedIntegerAsLong(PebbleConstants.KEY_COMMAND);
                if (cmd != null) {
                    handleCommand(cmd.intValue());
                }
            }
        };
        PebbleKit.registerReceivedDataHandler(this, dataReceiver);
    }

    private void handleCommand(int cmd) {
        MediaController.TransportControls tc =
                controller != null ? controller.getTransportControls() : null;

        switch (cmd) {
            case PebbleConstants.CMD_PLAY_PAUSE:
                if (tc != null) {
                    if (isPlaying()) tc.pause(); else tc.play();
                }
                break;
            case PebbleConstants.CMD_NEXT:
                if (tc != null) tc.skipToNext();
                break;
            case PebbleConstants.CMD_PREVIOUS:
                if (tc != null) tc.skipToPrevious();
                break;
            case PebbleConstants.CMD_VOLUME_UP:
                adjustVolume(AudioManager.ADJUST_RAISE);
                break;
            case PebbleConstants.CMD_VOLUME_DOWN:
                adjustVolume(AudioManager.ADJUST_LOWER);
                break;
            default:
                Log.w(TAG, "Unknown command: " + cmd);
        }
        // The controller callback will push the authoritative state shortly;
        // this gives the watch immediate feedback for non-playback commands.
        pushNowPlaying();
    }

    private void adjustVolume(int direction) {
        AudioManager am = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        if (am != null) {
            am.adjustStreamVolume(AudioManager.STREAM_MUSIC, direction, AudioManager.FLAG_SHOW_UI);
        }
    }

    // ==================== Foreground notification ====================

    private void goForeground() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationManager nm = getSystemService(NotificationManager.class);
            NotificationChannel ch = new NotificationChannel(
                    CHANNEL_ID, "Musick bridge", NotificationManager.IMPORTANCE_LOW);
            ch.setShowBadge(false);
            nm.createNotificationChannel(ch);
        }

        Notification notification = new NotificationCompat.Builder(this, CHANNEL_ID)
                .setContentTitle("Musick")
                .setContentText("Controlling music from your Pebble")
                .setSmallIcon(android.R.drawable.stat_notify_sync)
                .setOngoing(true)
                .setPriority(NotificationCompat.PRIORITY_LOW)
                .build();

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                startForeground(NOTIF_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
            } else {
                startForeground(NOTIF_ID, notification);
            }
        } catch (RuntimeException e) {
            Log.w(TAG, "startForeground failed; stopping", e);
            stopSelf();
        }
    }

    @Override
    public void onDestroy() {
        if (sessionManager != null) {
            sessionManager.removeOnActiveSessionsChangedListener(this);
        }
        setController(null);
        if (dataReceiver != null) {
            try { unregisterReceiver(dataReceiver); } catch (IllegalArgumentException ignored) {}
            dataReceiver = null;
        }
        super.onDestroy();
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
