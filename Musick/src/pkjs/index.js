// Command values sent from the watch (must match the MusicCommand enum in src/c/main.c).
var CMD_PLAY_PAUSE = 0;
var CMD_NEXT       = 1;
var CMD_PREVIOUS   = 2;

// Push the currently playing track to the watch.
//
// PebbleKit JS has no direct access to the phone's media player, so the
// now-playing info is a placeholder here. Wire this up to your music source
// (e.g. a companion app or web API) and call sendNowPlaying() to update it.
function sendNowPlaying(title, artist) {
  Pebble.sendAppMessage({
    'KEY_TITLE': title || '',
    'KEY_ARTIST': artist || ''
  });
}

Pebble.addEventListener('ready', function() {
  console.log('Musick PebbleKit JS ready');
  sendNowPlaying('Musick', 'Ready to play');
});

Pebble.addEventListener('appmessage', function(e) {
  var cmd = e.payload.KEY_COMMAND;
  if (cmd === undefined) return;

  switch (cmd) {
    case CMD_PLAY_PAUSE:
      console.log('Command: play/pause');
      break;
    case CMD_NEXT:
      console.log('Command: next track');
      break;
    case CMD_PREVIOUS:
      console.log('Command: previous track');
      break;
    default:
      console.log('Unknown command: ' + cmd);
  }

  // TODO: forward the command to your music source, then call sendNowPlaying()
  // with the updated track once it changes.
});
