function b64(str) {
  var chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  var out = '', i = 0, len = str.length;
  while (i < len) {
    var a = str.charCodeAt(i++);
    var b = i < len ? str.charCodeAt(i++) : -1;
    var c = i < len ? str.charCodeAt(i++) : -1;
    out += chars[a >> 2];
    out += chars[((a & 3) << 4) | (b < 0 ? 0 : b >> 4)];
    out += b < 0 ? '=' : chars[((b & 15) << 2) | (c < 0 ? 0 : c >> 6)];
    out += c < 0 ? '=' : chars[c & 63];
  }
  return out;
}

Pebble.addEventListener('ready', function() {
  console.log('[TOTP] JS ready');
});

Pebble.addEventListener('showConfiguration', function() {
  console.log('[TOTP] showConfiguration fired');
  var html = '<html><body style="font-family:sans-serif;background:#111;color:#eee;padding:20px">' +
    '<h2>TOTP Secret</h2>' +
    '<p style="color:#aaa;font-size:13px">Base32 secret z authenticator app</p>' +
    '<input id="s" type="text" placeholder="JBSWY3DPEHPK3PXP" style="width:100%;padding:10px;background:#222;color:#fff;border:1px solid #444;font-size:15px;box-sizing:border-box"/>' +
    '<div id="e" style="color:red;display:none">Neplatny secret</div>' +
    '<button onclick="save()" style="width:100%;padding:14px;background:#1976D2;color:#fff;border:none;font-size:17px;margin-top:8px">Ulozit</button>' +
    '<script>function save(){var v=document.getElementById("s").value.trim().toUpperCase().replace(/\\s+/g,"");if(!v||!/^[A-Z2-7]+=*$/.test(v)){document.getElementById("e").style.display="block";return;}location.href="pebblejs://close#"+encodeURIComponent(v);}</script>' +
    '</body></html>';
  var url = 'data:text/html;base64,' + b64(html);
  console.log('[TOTP] opening URL length=' + url.length);
  Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', function(e) {
  console.log('[TOTP] webviewclosed response=' + e.response);
  if (!e.response || e.response === '') return;
  var secret = decodeURIComponent(e.response);
  if (!secret) return;
  console.log('[TOTP] sending secret length=' + secret.length);
  Pebble.sendAppMessage(
    { 'KEY_SECRET': secret },
    function() { console.log('[TOTP] secret sent OK'); },
    function(err) { console.log('[TOTP] send error: ' + err.error); }
  );
});
