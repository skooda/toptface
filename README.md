# Watch apps monorepo

Monorepo for my Pebble watch apps. Each app lives in its own top-level folder
and is a self-contained Pebble project (its own `package.json`, `wscript`,
`src/` and `resources/`), built independently with the Pebble SDK.

## Apps

| Folder       | Type      | Target platform     | Description |
|--------------|-----------|---------------------|-------------|
| `Totpface/`  | Watchface | diorite (Pebble 2)  | TOTP (RFC 6238) authenticator watchface with time/date |
| `Musick/`    | Watchapp  | emery (Pebble Time 2) | Music remote — clone of the built-in Music app with button hints on the **left**; driven by an Android companion |

`Musick/` additionally contains `companion-android/`, a PebbleKit Android app
that reads the phone's media session and exchanges now-playing data + playback
commands with the watch. See `Musick/companion-android/README.md`.

## Building

Each watch app builds on its own. From inside an app folder:

```sh
cd Totpface                          # or: cd Musick
pebble build
pebble install --emulator emery      # match the app's target platform
```

Build artifacts (`build/`, `*.pbw`, Gradle outputs) are git-ignored at the repo root.
