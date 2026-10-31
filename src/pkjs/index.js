'use strict';

var STORAGE_KEY = 'asciinalog_settings_v1';

var DEFAULTS = {
  MIN_CHAR_SIZE: 2,
  MAX_CHAR_SIZE: 5,
  BIG_CHAR_BIAS: 72,
  PUNCT_RATIO: 12,
  IMPERFECTION: 30,
  SHOW_DATE: 1,
  INVERT_COLORS: 0,
  FILL_MODE: 0,          // 0 = Circular Dial, 1 = Full Screen Rectangle
  LAYOUT_STYLE: 0,       // 0 = Semi-Grid Multi-Scale, 1 = Organic Collage
  GREY_COMPLICATIONS: 1  // 0 = Off (solid), 1 = On (dithered on B&W, light grey on color)
};

function loadSettings() {
  try {
    var raw = localStorage.getItem(STORAGE_KEY);
    if (raw) {
      var parsed = JSON.parse(raw);
      var out = {};
      for (var k in DEFAULTS) {
        if (DEFAULTS.hasOwnProperty(k)) {
          out[k] = (parsed[k] !== undefined) ? parsed[k] : DEFAULTS[k];
        }
      }
      return out;
    }
  } catch (e) {
    console.log('Failed to load settings: ' + e);
  }
  return DEFAULTS;
}

function buildConfigHtml(s) {
  return [
    '<!DOCTYPE html><html><head><meta charset="utf-8">',
    '<meta name="viewport" content="width=device-width,initial-scale=1">',
    '<title>ASCIInalog Settings</title>',
    '<style>',
    'body{font-family:monospace,sans-serif;background:#111;color:#eee;margin:0;padding:16px;}',
    'h1{font-size:20px;margin:0 0 4px;letter-spacing:1px;}',
    '.sub{color:#888;font-size:12px;margin-bottom:16px;}',
    '.card{background:#1c1c1c;border:1px solid #333;border-radius:8px;padding:12px 14px;margin-bottom:12px;}',
    '.row{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;}',
    'label{font-size:13px;font-weight:bold;}',
    '.val{color:#4af;font-size:13px;font-weight:bold;}',
    '.hint{color:#888;font-size:11px;margin-top:4px;}',
    'input[type=range]{width:100%;accent-color:#fff;}',
    'input[type=checkbox]{transform:scale(1.3);}',
    'select{background:#262626;color:#fff;border:1px solid #444;border-radius:6px;padding:6px 8px;font-family:monospace;font-size:12px;width:100%;margin-top:4px;}',
    '.presets{display:flex;gap:8px;margin-bottom:12px;}',
    '.presets button{flex:1;background:#262626;color:#ddd;border:1px solid #444;border-radius:6px;padding:8px 4px;font-family:monospace;font-size:11px;cursor:pointer;}',
    '.save{width:100%;background:#fff;color:#000;border:none;border-radius:8px;padding:14px;font-family:monospace;font-size:15px;font-weight:bold;cursor:pointer;margin-top:6px;}',
    '</style></head><body>',
    '<h1>*ASCIInalog*</h1>',
    '<div class="sub">Multi-scale ASCII analog watchface</div>',
    '<div class="presets">',
    '<button type="button" onclick="preset(0,0,2,5,75,12,30)">Semi-Grid</button>',
    '<button type="button" onclick="preset(1,0,2,5,75,12,30)">Full Rect</button>',
    '<button type="button" onclick="preset(0,1,2,5,70,12,40)">Organic</button>',
    '</div>',

    '<div class="card"><div class="row"><label>Screen Fill Mode</label></div>',
    '<select id="fill_mode">',
    '<option value="0"' + (s.FILL_MODE === 0 ? ' selected' : '') + '>Circular Dial (Classic Clock)</option>',
    '<option value="1"' + (s.FILL_MODE === 1 ? ' selected' : '') + '>Full Screen Rectangle (Edge to Edge)</option>',
    '</select>',
    '<div class="hint">Full Screen fills rectangular watches edge-to-edge; hands reach screen borders.</div></div>',

    '<div class="card"><div class="row"><label>Layout Arrangement</label></div>',
    '<select id="layout_style">',
    '<option value="0"' + (s.LAYOUT_STYLE === 0 ? ' selected' : '') + '>Semi-Grid Rows with Big Hero Glyphs (whattimeisit style)</option>',
    '<option value="1"' + (s.LAYOUT_STYLE === 1 ? ' selected' : '') + '>Organic Multi-scale Collage</option>',
    '</select>',
    '<div class="hint">Semi-Grid retains row-aligned flow with large glyphs; Organic scatters irregular blocks.</div></div>',

    '<div class="card"><div class="row"><label>Min Character Size</label><span class="val" id="v_min">' + s.MIN_CHAR_SIZE + '</span></div>',
    '<input type="range" id="min_size" min="1" max="4" step="1" value="' + s.MIN_CHAR_SIZE + '" oninput="sync()">',
    '<div class="hint">1 = Tiny (~8px) &middot; 2 = Small (~12px) &middot; 3 = Medium &middot; 4 = Large</div></div>',

    '<div class="card"><div class="row"><label>Max Character Size</label><span class="val" id="v_max">' + s.MAX_CHAR_SIZE + '</span></div>',
    '<input type="range" id="max_size" min="3" max="6" step="1" value="' + s.MAX_CHAR_SIZE + '" oninput="sync()">',
    '<div class="hint">3 = Medium (~20px) &middot; 5 = X-Large (~36px) &middot; 6 = Giant (~46px)</div></div>',

    '<div class="card"><div class="row"><label>Big Character Focus</label><span class="val" id="v_bias">' + s.BIG_CHAR_BIAS + '%</span></div>',
    '<input type="range" id="bias" min="0" max="100" step="5" value="' + s.BIG_CHAR_BIAS + '" oninput="sync()">',
    '<div class="hint">Higher = fills open bands aggressively with large hero letters/digits</div></div>',

    '<div class="card"><div class="row"><label>Punctuation Allowance</label><span class="val" id="v_punct">' + s.PUNCT_RATIO + '%</span></div>',
    '<input type="range" id="punct" min="0" max="50" step="2" value="' + s.PUNCT_RATIO + '" oninput="sync()">',
    '<div class="hint">0% = pure letters &amp; digits &middot; 10-15% = sparse organic symbols</div></div>',

    '<div class="card"><div class="row"><label>Imperfection / Raggedness</label><span class="val" id="v_imp">' + s.IMPERFECTION + '%</span></div>',
    '<input type="range" id="imp" min="0" max="100" step="5" value="' + s.IMPERFECTION + '" oninput="sync()">',
    '<div class="hint">Adds positional jitter and leaves small ragged edge gaps</div></div>',

    '<div class="card"><div class="row"><label>Show Day/Date Word</label>',
    '<input type="checkbox" id="show_date"' + (s.SHOW_DATE ? ' checked' : '') + '></div>',
    '<div class="hint">Embeds current day &amp; date (e.g. SAT-31) into the watchface</div></div>',

    '<div class="card"><div class="row"><label>Greyscale / Dithered Complications</label>',
    '<input type="checkbox" id="grey_comp"' + (s.GREY_COMPLICATIONS ? ' checked' : '') + '></div>',
    '<div class="hint">Renders complications in greyscale (50% stipple on B&W, light grey on color)</div></div>',

    '<div class="card"><div class="row"><label>Invert Colors (Black on White)</label>',
    '<input type="checkbox" id="invert"' + (s.INVERT_COLORS ? ' checked' : '') + '></div></div>',

    '<button class="save" type="button" onclick="save()">SAVE TO WATCH</button>',
    '<script>',
    'function el(id){return document.getElementById(id);}',
    'function sync(){',
    '  var mn=parseInt(el("min_size").value,10),mx=parseInt(el("max_size").value,10);',
    '  if(mn>mx){mx=mn;el("max_size").value=mx;}',
    '  el("v_min").textContent=mn;',
    '  el("v_max").textContent=mx;',
    '  el("v_bias").textContent=el("bias").value+"%";',
    '  el("v_punct").textContent=el("punct").value+"%";',
    '  el("v_imp").textContent=el("imp").value+"%";',
    '}',
    'function preset(fm,ls,mn,mx,b,p,i){',
    '  el("fill_mode").value=fm;el("layout_style").value=ls;',
    '  el("min_size").value=mn;el("max_size").value=mx;el("bias").value=b;',
    '  el("punct").value=p;el("imp").value=i;sync();',
    '}',
    'function getReturnTo(){',
    '  var m=location.search.match(/[?&]return_to=([^&]+)/);',
    '  return m?decodeURIComponent(m[1]):"pebblejs://close#";',
    '}',
    'function save(){',
    '  sync();',
    '  var data={',
    '    FILL_MODE:parseInt(el("fill_mode").value,10),',
    '    LAYOUT_STYLE:parseInt(el("layout_style").value,10),',
    '    MIN_CHAR_SIZE:parseInt(el("min_size").value,10),',
    '    MAX_CHAR_SIZE:parseInt(el("max_size").value,10),',
    '    BIG_CHAR_BIAS:parseInt(el("bias").value,10),',
    '    PUNCT_RATIO:parseInt(el("punct").value,10),',
    '    IMPERFECTION:parseInt(el("imp").value,10),',
    '    SHOW_DATE:el("show_date").checked?1:0,',
    '    GREY_COMPLICATIONS:el("grey_comp").checked?1:0,',
    '    INVERT_COLORS:el("invert").checked?1:0',
    '  };',
    '  location.href=getReturnTo()+encodeURIComponent(JSON.stringify(data));',
    '}',
    '</script></body></html>'
  ].join('');
}

Pebble.addEventListener('ready', function() {
  console.log('ASCIInalog PKJS ready');
});

Pebble.addEventListener('showConfiguration', function() {
  var settings = loadSettings();
  var url = 'data:text/html;charset=utf-8,' + encodeURIComponent(buildConfigHtml(settings));
  Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) {
    return;
  }
  try {
    var decoded = decodeURIComponent(e.response);
    var data = JSON.parse(decoded);
    localStorage.setItem(STORAGE_KEY, JSON.stringify(data));
    Pebble.sendAppMessage(data, function() {
      console.log('Settings sent to watch');
    }, function(err) {
      console.log('Error sending settings: ' + JSON.stringify(err));
    });
  } catch (err) {
    console.log('Failed to parse config response: ' + err);
  }
});
