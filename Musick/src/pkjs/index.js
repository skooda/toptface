// Musick is driven by its Android companion app (PebbleKit Android), which
// reads the phone's MediaSession and exchanges now-playing data and playback
// commands directly with the watchapp over Bluetooth. PebbleKit JS is not used
// for music control; this file only exists to satisfy the build's JS entry
// point and log the bridge status.
Pebble.addEventListener('ready', function() {
  console.log('Musick ready — control is handled by the Android companion app');
});
