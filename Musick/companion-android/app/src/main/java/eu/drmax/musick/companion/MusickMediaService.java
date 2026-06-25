package eu.drmax.musick.companion;

import android.content.ComponentName;
import android.content.Context;
import android.media.AudioManager;
import android.media.MediaMetadata;
import android.media.session.MediaController;
import android.media.session.MediaSessionManager;
import android.media.session.PlaybackState;
import android.service.notification.NotificationListenerService;
import android.util.Log;

import com.getpebble.android.kit.PebbleKit;
import com.getpebble.android.kit.util.PebbleDictionary;

import java.util.List;

/**
 * The companion's workhorse. As a NotificationListenerService it is allowed to
 * read the phone's active {@link MediaSession}s, so it can:
 *   - forward the current track + playback state to the watch, and
 *   - apply playback commands the watch sends back.
 *
 * It is long-lived (the system keeps notification listeners bound), which makes
 * it a good home for the Pebble data receiver too.
 */
public class MusickMediaService extends NotificationListenerService
        implements MediaSessionManager.OnActiveSessionsChangedListener {

    private static final String TAG = "MusickMedia";

    private MediaSessionManager sessionManager;
    private MediaController controller;
    private PebbleKit.PebbleDataReceiver dataReceiver;

    private final MediaController.Callback controllerCallback = new MediaController.Callback() {
        @Override public void onMetadataChanged(MediaMetadata metadata) { pushNowPlaying(); }
        @Override public void onPlaybackStateChanged(PlaybackState state) { pushNowPlaying(); }
        @Override public void onSessionDestroyed() { setController(null); }
    };

    @Override
    public void onListenerConnected() {
        sessionManager = (MediaSessionManager) getSystemService(Context.MEDIA_SESSION_SERVICE);
        ComponentName self = new ComponentName(this, MusickMediaService.class);
        try {
            sessionManager.addOnActiveSessionsChangedListener(this, self);
            onActiveSessionsChanged(sessionManager.getActiveSessions(self));
        } catch (SecurityException e) {
            Log.w(TAG, "Notification access not granted yet", e);
        }
        registerPebbleReceiver();
    }

    @Override
    public void onListenerDisconnected() {
        if (sessionManager != null) {
            sessionManager.removeOnActiveSessionsChangedListener(this);
        }
        setController(null);
        if (dataReceiver != null) {
            try { unregisterReceiver(dataReceiver); } catch (IllegalArgumentException ignored) {}
            dataReceiver = null;
        }
    }

    @Override
    public void onActiveSessionsChanged(List<MediaController> controllers) {
        // The first session is the most recently active one.
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
}
