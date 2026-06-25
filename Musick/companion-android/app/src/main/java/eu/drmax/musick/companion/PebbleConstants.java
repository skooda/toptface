package eu.drmax.musick.companion;

import java.util.UUID;

/**
 * Shared AppMessage contract with the Musick watchapp.
 * These integer keys and command values MUST match src/c/main.c.
 */
public final class PebbleConstants {

    private PebbleConstants() {}

    public static final UUID APP_UUID =
            UUID.fromString("3e58ab85-63f9-41f7-aea5-a4827c328c5a");

    // AppMessage keys (watch -> phone and phone -> watch).
    public static final int KEY_TITLE   = 0;
    public static final int KEY_ARTIST  = 1;
    public static final int KEY_COMMAND = 2;
    public static final int KEY_STATE   = 3;

    // KEY_COMMAND values sent by the watch.
    public static final int CMD_PLAY_PAUSE  = 0;
    public static final int CMD_NEXT        = 1;
    public static final int CMD_PREVIOUS    = 2;
    public static final int CMD_VOLUME_UP   = 3;
    public static final int CMD_VOLUME_DOWN = 4;
}
