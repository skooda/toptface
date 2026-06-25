# Watch apps monorepo

Monorepo for my Pebble watch apps. Each app lives in its own top-level folder
and is a self-contained Pebble project (its own `package.json`, `wscript`,
`src/` and `resources/`), built independently with the Pebble SDK.

## Apps

| Folder       | Type      | Description                                            |
|--------------|-----------|--------------------------------------------------------|
| `Totpface/`  | Watchface | TOTP (RFC 6238) authenticator watchface with time/date |
| `Musick/`    | Watchapp  | Music player remote — play/pause, next, previous        |

## Building

Each app builds on its own. From inside an app folder:

```sh
cd Totpface   # or: cd Musick
pebble build
pebble install --emulator diorite   # or --phone for a real watch
```

Build artifacts (`build/`, `*.pbw`) are git-ignored at the repo root.
