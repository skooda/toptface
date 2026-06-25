# Musick — Android companion

Bridges the phone's music player to the Musick watchapp using
[PebbleKit Android](https://github.com/pebble/pebblekit-android). It runs as a
**foreground service** (`MusickService`) so Android's Doze / battery
optimisation can't kill it in the background — that's restarted on boot and
whenever the notification listener rebinds. It reads the phone's active
`MediaSession` (authorised by the `NotificationListenerService`) and:

- pushes the current **title**, **artist** and **play/pause state** to the watch;
- applies playback commands the watch sends — play/pause, next, previous, and
  volume up/down.

## How it talks to the watch

AppMessage, with integer keys that must stay in sync with the watchapp
(`../src/c/main.c`) — see `PebbleConstants.java`:

| Key | Id | Direction | Meaning |
|-----|----|-----------|---------|
| `KEY_TITLE`   | 0 | phone → watch | track title |
| `KEY_ARTIST`  | 1 | phone → watch | track artist |
| `KEY_COMMAND` | 2 | watch → phone | playback command (see below) |
| `KEY_STATE`   | 3 | phone → watch | 1 = playing, 0 = paused |

Commands (`KEY_COMMAND`): `0` play/pause, `1` next, `2` previous,
`3` volume up, `4` volume down.

## Build & run

1. Open `companion-android/` in Android Studio (or run `gradle wrapper` once to
   generate `gradlew`, then `./gradlew assembleDebug`).
2. Install the APK on the phone paired with your Pebble.
3. Launch it and tap **Grant notification access** — required to read media
   sessions — and **Disable battery optimisation** so the bridge isn't killed
   in the background. On Android 13+ also allow the notification prompt.
4. Start playing music and open Musick on the watch.

## Notes / caveats

- **PebbleKit dependency.** The original jcenter artifact is gone; the build
  pulls PebbleKit from GitHub via JitPack (`com.github.pebble:pebblekit-android`).
  If Core Devices publishes an official PebbleKit artifact, switch
  `app/build.gradle` to that coordinate.
- **Package visibility (Android 11+).** PebbleKit broadcasts to the Pebble app;
  `AndroidManifest.xml` lists the known package ids in `<queries>`. If you run a
  different Pebble app build (e.g. the Core Devices app), add its package id
  there.
- No `gradlew` wrapper jar is committed; generate it with `gradle wrapper` or
  just open the project in Android Studio.
