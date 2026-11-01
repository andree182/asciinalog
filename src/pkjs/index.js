'use strict';

var STORAGE_KEY = 'asciinalog_settings_v3';

function isRoundWatch() {
  try {
    if (Pebble.getActiveWatchInfo) {
      var info = Pebble.getActiveWatchInfo();
      if (info && (info.platform === 'chalk' || (info.model && info.model.indexOf('round') !== -1))) {
        return true;
      }
    }
  } catch (e) {}
  return false;
}

var DEFAULTS = {
  MIN_CHAR_SIZE: 2,
  MAX_CHAR_SIZE: 5,
  BIG_CHAR_BIAS: 20,
  PUNCT_RATIO: 12,
  IMPERFECTION: 30,
  SHOW_DATE: 1,
  SHOW_MONTH: 1,
  SHOW_BATTERY: 1,
  SHOW_HAND_HOUR: 0,
  SHOW_HAND_MINUTE: 0,
  INVERT_COLORS: 0,
  FILL_MODE: 1,          // 0 = Circular Dial, 1 = Full Screen Rectangle (default on rectangle watches)
  LAYOUT_STYLE: 0,       // 0 = Semi-Grid Multi-Scale, 1 = Organic Collage
  CONTRAST_MODE: 0,      // 0 = White Comp / Grey Noise, 1 = Grey Comp / White Noise, 2 = Uniform Solid
  RANDOM_COLORS: 0       // 0 = Off (white/fg), 1 = On (random light colors on color watches)
};

function loadSettings() {
  var isRound = isRoundWatch();
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
      if (isRound) out.FILL_MODE = 0;
      return out;
    }
  } catch (e) {
    console.log('Failed to load settings: ' + e);
  }
  var d = {};
  for (var k in DEFAULTS) {
    if (DEFAULTS.hasOwnProperty(k)) d[k] = DEFAULTS[k];
  }
  if (isRound) d.FILL_MODE = 0;
  return d;
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
    'input[type=range]{width:100%;accent-color:#4af;}',
    '.btn-group{display:flex;background:#262626;border-radius:8px;padding:3px;gap:4px;border:1px solid #444;margin-top:6px;}',
    '.btn-opt{flex:1;background:transparent;border:none;color:#999;padding:9px 6px;font-family:monospace;font-size:12px;font-weight:bold;border-radius:6px;cursor:pointer;transition:0.15s;}',
    '.btn-opt.active{background:#4af;color:#000;}',
    '.toggle-btn{display:flex;background:#262626;border-radius:8px;padding:3px;gap:4px;border:1px solid #444;width:120px;}',
    '.toggle-btn .btn-opt{padding:6px 4px;font-size:11px;}',
    '.presets{display:flex;gap:8px;margin-bottom:12px;}',
    '.presets button{flex:1;background:#262626;color:#ddd;border:1px solid #444;border-radius:6px;padding:8px 4px;font-family:monospace;font-size:11px;cursor:pointer;}',
    'details.adv-card{background:#181818;border:1px dashed #444;border-radius:8px;padding:10px 14px;margin-bottom:12px;}',
    'details.adv-card summary{cursor:pointer;font-weight:bold;font-size:13px;color:#4af;outline:none;user-select:none;padding:4px 0;}',
    '.adv-content{margin-top:12px;}',
    '.save{width:100%;background:#4af;color:#000;border:none;border-radius:8px;padding:14px;font-family:monospace;font-size:15px;font-weight:bold;cursor:pointer;margin-top:10px;}',
    '</style></head><body>',
    '<h1>*ASCIInalog*</h1>',
    '<div class="sub">Multi-scale ASCII analog watchface</div>',
    '<div class="presets" style="display:none;">',
    '<button type="button" onclick="preset(1,0,2,5,20,12,30)">Default (Rect)</button>',
    '<button type="button" onclick="preset(0,0,2,5,20,12,30)">Circle</button>',
    '<button type="button" onclick="preset(1,1,2,5,20,12,30)">Organic</button>',
    '</div>',

    // Screen Fill Mode
    '<div class="card"><div class="row"><label>Screen Fill Mode</label></div>',
    '<div class="btn-group" id="grp_fill_mode">',
    '<button type="button" class="btn-opt' + (s.FILL_MODE === 0 ? ' active' : '') + '" onclick="setVal(\'fill_mode\',0)">Circular Dial</button>',
    '<button type="button" class="btn-opt' + (s.FILL_MODE === 1 ? ' active' : '') + '" onclick="setVal(\'fill_mode\',1)">Full Rectangle</button>',
    '</div><input type="hidden" id="fill_mode" value="' + s.FILL_MODE + '">',
    '<div class="hint">Full Screen fills rectangular watches edge-to-edge; hands reach borders.</div></div>',

    // Layout Arrangement
    '<div class="card"><div class="row"><label>Layout Arrangement</label></div>',
    '<div class="btn-group" id="grp_layout_style">',
    '<button type="button" class="btn-opt' + (s.LAYOUT_STYLE === 0 ? ' active' : '') + '" onclick="setVal(\'layout_style\',0)">Semi-Grid</button>',
    '<button type="button" class="btn-opt' + (s.LAYOUT_STYLE === 1 ? ' active' : '') + '" onclick="setVal(\'layout_style\',1)">Organic</button>',
    '</div><input type="hidden" id="layout_style" value="' + s.LAYOUT_STYLE + '">',
    '<div class="hint">Semi-Grid retains row flow with hero glyphs; Organic scatters irregular blocks.</div></div>',

    // Max Character Size
    '<div class="card"><div class="row"><label>Max Character Size</label><span class="val" id="v_max">' + s.MAX_CHAR_SIZE + '</span></div>',
    '<input type="range" id="max_size" min="3" max="6" step="1" value="' + s.MAX_CHAR_SIZE + '" oninput="sync()">',
    '<div class="hint">3 = Medium (~20px) &middot; 5 = X-Large (~36px) &middot; 6 = Giant (~46px)</div></div>',

    // Big Character Focus
    '<div class="card"><div class="row"><label>Big Character Focus</label><span class="val" id="v_bias">' + s.BIG_CHAR_BIAS + '%</span></div>',
    '<input type="range" id="bias" min="0" max="100" step="5" value="' + s.BIG_CHAR_BIAS + '" oninput="sync()">',
    '<div class="hint">Higher = fills open spaces aggressively with large hero characters</div></div>',

    // Complications Section
    '<div class="card"><div class="row"><label>Day / Date Word (e.g. SUN-01)</label>',
    '<div class="toggle-btn" id="grp_show_date">',
    '<button type="button" class="btn-opt' + (!s.SHOW_DATE ? ' active' : '') + '" onclick="setVal(\'show_date\',0)">OFF</button>',
    '<button type="button" class="btn-opt' + (s.SHOW_DATE ? ' active' : '') + '" onclick="setVal(\'show_date\',1)">ON</button>',
    '</div><input type="hidden" id="show_date" value="' + (s.SHOW_DATE ? 1 : 0) + '"></div></div>',

    '<div class="card"><div class="row"><label>Month (3 chars, e.g. NOV)</label>',
    '<div class="toggle-btn" id="grp_show_month">',
    '<button type="button" class="btn-opt' + (!s.SHOW_MONTH ? ' active' : '') + '" onclick="setVal(\'show_month\',0)">OFF</button>',
    '<button type="button" class="btn-opt' + (s.SHOW_MONTH ? ' active' : '') + '" onclick="setVal(\'show_month\',1)">ON</button>',
    '</div><input type="hidden" id="show_month" value="' + (s.SHOW_MONTH ? 1 : 0) + '"></div></div>',

    '<div class="card"><div class="row"><label>Battery (Filled Rectangle Glyphs)</label>',
    '<div class="toggle-btn" id="grp_show_battery">',
    '<button type="button" class="btn-opt' + (!s.SHOW_BATTERY ? ' active' : '') + '" onclick="setVal(\'show_battery\',0)">OFF</button>',
    '<button type="button" class="btn-opt' + (s.SHOW_BATTERY ? ' active' : '') + '" onclick="setVal(\'show_battery\',1)">ON</button>',
    '</div><input type="hidden" id="show_battery" value="' + (s.SHOW_BATTERY ? 1 : 0) + '"></div>',
    '<div class="hint">One-character battery symbol with fill level proportional to charge</div></div>',

    '<div class="card"><div class="row"><label>Hour Digits at Hand Tip</label>',
    '<div class="toggle-btn" id="grp_show_hand_hour">',
    '<button type="button" class="btn-opt' + (!s.SHOW_HAND_HOUR ? ' active' : '') + '" onclick="setVal(\'show_hand_hour\',0)">OFF</button>',
    '<button type="button" class="btn-opt' + (s.SHOW_HAND_HOUR ? ' active' : '') + '" onclick="setVal(\'show_hand_hour\',1)">ON</button>',
    '</div><input type="hidden" id="show_hand_hour" value="' + (s.SHOW_HAND_HOUR ? 1 : 0) + '"></div>',
    '<div class="hint">Shows hour number right around the end of the analog hour hand</div></div>',

    '<div class="card"><div class="row"><label>Minute Digits at Hand Tip</label>',
    '<div class="toggle-btn" id="grp_show_hand_minute">',
    '<button type="button" class="btn-opt' + (!s.SHOW_HAND_MINUTE ? ' active' : '') + '" onclick="setVal(\'show_hand_minute\',0)">OFF</button>',
    '<button type="button" class="btn-opt' + (s.SHOW_HAND_MINUTE ? ' active' : '') + '" onclick="setVal(\'show_hand_minute\',1)">ON</button>',
    '</div><input type="hidden" id="show_hand_minute" value="' + (s.SHOW_HAND_MINUTE ? 1 : 0) + '"></div>',
    '<div class="hint">Shows minute number right around the end of the analog minute hand</div></div>',

    // Colors
    '<div class="card"><div class="row"><label>Random Pastel Colors (Color watches)</label>',
    '<div class="toggle-btn" id="grp_random_colors">',
    '<button type="button" class="btn-opt' + (!s.RANDOM_COLORS ? ' active' : '') + '" onclick="setVal(\'random_colors\',0)">OFF</button>',
    '<button type="button" class="btn-opt' + (s.RANDOM_COLORS ? ' active' : '') + '" onclick="setVal(\'random_colors\',1)">ON</button>',
    '</div><input type="hidden" id="random_colors" value="' + (s.RANDOM_COLORS ? 1 : 0) + '"></div>',
    '<div class="hint">Draws background in light pastel colors while complications stay readable</div></div>',

    '<div class="card"><div class="row"><label>Invert Colors (Black on White)</label>',
    '<div class="toggle-btn" id="grp_invert">',
    '<button type="button" class="btn-opt' + (!s.INVERT_COLORS ? ' active' : '') + '" onclick="setVal(\'invert\',0)">OFF</button>',
    '<button type="button" class="btn-opt' + (s.INVERT_COLORS ? ' active' : '') + '" onclick="setVal(\'invert\',1)">ON</button>',
    '</div><input type="hidden" id="invert" value="' + (s.INVERT_COLORS ? 1 : 0) + '"></div></div>',

    // ADVANCED SECTION
    '<details class="adv-card">',
    '<summary>⚙️ Advanced Options</summary>',
    '<div class="adv-content">',

    '<div class="card"><div class="row"><label>Min Character Size</label><span class="val" id="v_min">' + s.MIN_CHAR_SIZE + '</span></div>',
    '<input type="range" id="min_size" min="1" max="4" step="1" value="' + s.MIN_CHAR_SIZE + '" oninput="sync()">',
    '<div class="hint">1 = Tiny (~8px, full gap filling) &middot; 2 = Small (~12px) &middot; 3 = Medium &middot; 4 = Large (no tiny filler)</div></div>',

    '<div class="card"><div class="row"><label>Imperfection / Raggedness</label><span class="val" id="v_imp">' + s.IMPERFECTION + '%</span></div>',
    '<input type="range" id="imp" min="0" max="100" step="5" value="' + s.IMPERFECTION + '" oninput="sync()">',
    '<div class="hint">Adds jitter and leaves ragged edge gaps near outer dial rim</div></div>',

    '<div class="card"><div class="row"><label>Punctuation Allowance</label><span class="val" id="v_punct">' + s.PUNCT_RATIO + '%</span></div>',
    '<input type="range" id="punct" min="0" max="50" step="2" value="' + s.PUNCT_RATIO + '" oninput="sync()">',
    '<div class="hint">0% = pure alphanumeric &middot; 10-15% = sparse organic punctuation</div></div>',

    '<div class="card"><div class="row"><label>Contrast / Shading Mode</label></div>',
    '<div class="btn-group" id="grp_contrast_mode">',
    '<button type="button" class="btn-opt' + (s.CONTRAST_MODE === 0 ? ' active' : '') + '" onclick="setVal(\'contrast_mode\',0)">White Comp / Grey Noise</button>',
    '<button type="button" class="btn-opt' + (s.CONTRAST_MODE === 1 ? ' active' : '') + '" onclick="setVal(\'contrast_mode\',1)">Grey Comp / White Noise</button>',
    '<button type="button" class="btn-opt' + (s.CONTRAST_MODE === 2 ? ' active' : '') + '" onclick="setVal(\'contrast_mode\',2)">Uniform</button>',
    '</div><input type="hidden" id="contrast_mode" value="' + s.CONTRAST_MODE + '">',
    '<div class="hint">In Inverted mode, White becomes Black and vice versa.</div></div>',

    '</div></details>',

    '<button class="save" type="button" onclick="save()">SAVE TO WATCH</button>',
    '<script>',
    'function el(id){return document.getElementById(id);}',
    'function setVal(id,val){',
    '  el(id).value=val;',
    '  var grp=el("grp_"+id);',
    '  if(grp){',
    '    var btns=grp.querySelectorAll(".btn-opt");',
    '    btns.forEach(function(b,i){',
    '      if(i===val) b.classList.add("active");',
    '      else b.classList.remove("active");',
    '    });',
    '  }',
    '  sync();',
    '}',
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
    '  setVal("fill_mode",fm);setVal("layout_style",ls);',
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
    '    SHOW_DATE:parseInt(el("show_date").value,10),',
    '    SHOW_MONTH:parseInt(el("show_month").value,10),',
    '    SHOW_BATTERY:parseInt(el("show_battery").value,10),',
    '    SHOW_HAND_HOUR:parseInt(el("show_hand_hour").value,10),',
    '    SHOW_HAND_MINUTE:parseInt(el("show_hand_minute").value,10),',
    '    CONTRAST_MODE:parseInt(el("contrast_mode").value,10),',
    '    RANDOM_COLORS:parseInt(el("random_colors").value,10),',
    '    INVERT_COLORS:parseInt(el("invert").value,10)',
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
