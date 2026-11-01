#include <pebble.h>

// ============================================================================
// ASCIInalog Watchface
// Multi-scale ASCII analog watchface with negative-space hands, AAlib quadrant
// pixel-coverage contouring, configurable screen fill (circular vs full rect),
// layout styles (multi-scale semi-grid vs organic collage), and greyscale
// (50% dithered on B&W, light grey on color) complications.
// ============================================================================

#define SUB_N 48
#define SUB_CELLS (SUB_N * SUB_N)
#define MAX_GRID_ROWS 36
#define SETTINGS_PERSIST_KEY 106

typedef struct {
    uint8_t min_char_size;      // 1..4 (default 2)
    uint8_t max_char_size;      // 3..6 (default 5)
    uint8_t big_char_bias;      // 0..100 (default 72)
    uint8_t punct_ratio;        // 0..50 (default 12)
    uint8_t imperfection;       // 0..100 (default 30)
    uint8_t show_date;          // 0 or 1 (default 1)
    uint8_t show_month;         // 0 or 1 (default 1)
    uint8_t show_battery;       // 0 or 1 (default 1)
    uint8_t show_hand_hour;     // 0 or 1 (default 1)
    uint8_t show_hand_minute;   // 0 or 1 (default 1)
    uint8_t invert_colors;      // 0 or 1 (default 0)
    uint8_t fill_mode;          // 0 = Circular Dial, 1 = Full Screen Rectangle
    uint8_t layout_style;       // 0 = Semi-Grid Multi-Scale, 1 = Organic Collage
    uint8_t contrast_mode;      // 0 = White Comp / Grey Noise, 1 = Grey Comp / White Noise, 2 = Uniform
    uint8_t random_colors;      // 0 = Off, 1 = Random Light Colors (Color watches)
} WatchSettings;

static WatchSettings s_settings;
static Window *s_main_window;
static Layer *s_canvas_layer;
static struct tm s_current_time;

// ============================================================================
// 8x8 BOLD RETRO PIXEL FONT (Active 7x7 in rows 0..6, bits 7..1; 1px spacing)
// ============================================================================

#define GF_DENSE 0x01
#define GF_PUNCT 0x02
#define GF_MICRO 0x04

typedef struct {
    char ch;
    uint8_t flags;
    uint8_t rows[8];
} GlyphDef;

static const GlyphDef s_glyphs[] = {
    // --- Bold Full-Height Digits & Uppercase Letters ---
    { '0', GF_DENSE, { 0x7C, 0xFE, 0xEE, 0xFE, 0xEE, 0xFE, 0x7C, 0x00 } },
    { '1', 0,        { 0x38, 0x78, 0xF8, 0x38, 0x38, 0xFE, 0xFE, 0x00 } },
    { '2', GF_DENSE, { 0x7C, 0xFE, 0x8E, 0x3C, 0x70, 0xFE, 0xFE, 0x00 } },
    { '3', GF_DENSE, { 0xFC, 0xFE, 0x0E, 0x7C, 0x0E, 0xFE, 0xFC, 0x00 } },
    { '4', GF_DENSE, { 0x3C, 0x7C, 0xEC, 0xFE, 0xFE, 0x3C, 0x3C, 0x00 } },
    { '5', GF_DENSE, { 0xFE, 0xFE, 0xE0, 0xFC, 0x0E, 0xFE, 0xFC, 0x00 } },
    { '6', GF_DENSE, { 0x7C, 0xFE, 0xE0, 0xFC, 0xEE, 0xFE, 0x7C, 0x00 } },
    { '7', 0,        { 0xFE, 0xFE, 0x0E, 0x1C, 0x38, 0x70, 0x70, 0x00 } },
    { '8', GF_DENSE, { 0x7C, 0xFE, 0xEE, 0x7C, 0xEE, 0xFE, 0x7C, 0x00 } },
    { '9', GF_DENSE, { 0x7C, 0xFE, 0xEE, 0xFE, 0x0E, 0xFE, 0x7C, 0x00 } },

    { 'A', GF_DENSE, { 0x7C, 0xFE, 0xEE, 0xFE, 0xFE, 0xEE, 0xEE, 0x00 } },
    { 'B', GF_DENSE, { 0xFC, 0xFE, 0xEE, 0xFC, 0xEE, 0xFE, 0xFC, 0x00 } },
    { 'C', 0,        { 0x7E, 0xFE, 0xE0, 0xE0, 0xE0, 0xFE, 0x7E, 0x00 } },
    { 'D', GF_DENSE, { 0xFC, 0xFE, 0xEE, 0xEE, 0xEE, 0xFE, 0xFC, 0x00 } },
    { 'E', GF_DENSE, { 0xFE, 0xFE, 0xE0, 0xFC, 0xE0, 0xFE, 0xFE, 0x00 } },
    { 'F', 0,        { 0xFE, 0xFE, 0xE0, 0xFC, 0xE0, 0xE0, 0xE0, 0x00 } },
    { 'G', GF_DENSE, { 0x7E, 0xFE, 0xE0, 0xEE, 0xEE, 0xFE, 0x7E, 0x00 } },
    { 'H', GF_DENSE, { 0xEE, 0xEE, 0xEE, 0xFE, 0xFE, 0xEE, 0xEE, 0x00 } },
    { 'I', 0,        { 0xFE, 0xFE, 0x38, 0x38, 0x38, 0xFE, 0xFE, 0x00 } },
    { 'J', 0,        { 0x1E, 0x1E, 0x0E, 0x0E, 0xCE, 0xFE, 0x7C, 0x00 } },
    { 'K', GF_DENSE, { 0xEE, 0xF6, 0xFC, 0xF8, 0xFC, 0xF6, 0xEE, 0x00 } },
    { 'L', 0,        { 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xFE, 0xFE, 0x00 } },
    { 'M', GF_DENSE, { 0xC6, 0xEE, 0xFE, 0xFE, 0xD6, 0xC6, 0xC6, 0x00 } },
    { 'N', GF_DENSE, { 0xCE, 0xEE, 0xFE, 0xFE, 0xF6, 0xEE, 0xE6, 0x00 } },
    { 'O', GF_DENSE, { 0x7C, 0xFE, 0xEE, 0xEE, 0xEE, 0xFE, 0x7C, 0x00 } },
    { 'P', GF_DENSE, { 0xFC, 0xFE, 0xEE, 0xFE, 0xFC, 0xE0, 0xE0, 0x00 } },
    { 'Q', GF_DENSE, { 0x7C, 0xFE, 0xEE, 0xEE, 0xF6, 0xFE, 0x7E, 0x00 } },
    { 'R', GF_DENSE, { 0xFC, 0xFE, 0xEE, 0xFC, 0xF8, 0xEE, 0xEE, 0x00 } },
    { 'S', GF_DENSE, { 0x7E, 0xFE, 0xE0, 0x7C, 0x0E, 0xFE, 0xFC, 0x00 } },
    { 'T', 0,        { 0xFE, 0xFE, 0x38, 0x38, 0x38, 0x38, 0x38, 0x00 } },
    { 'U', GF_DENSE, { 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xFE, 0x7C, 0x00 } },
    { 'V', 0,        { 0xC6, 0xC6, 0xC6, 0xEE, 0x7C, 0x38, 0x10, 0x00 } },
    { 'W', GF_DENSE, { 0xC6, 0xD6, 0xD6, 0xFE, 0xFE, 0xEE, 0xC6, 0x00 } },
    { 'X', GF_DENSE, { 0xEE, 0xEE, 0x7C, 0x38, 0x7C, 0xEE, 0xEE, 0x00 } },
    { 'Y', 0,        { 0xC6, 0xEE, 0x7C, 0x38, 0x38, 0x38, 0x38, 0x00 } },
    { 'Z', GF_DENSE, { 0xFE, 0xFE, 0x1C, 0x38, 0x70, 0xFE, 0xFE, 0x00 } },

    // --- Lowercase Letters ---
    { 'a', GF_DENSE, { 0x7C, 0xFE, 0x0E, 0x7E, 0xEE, 0xFE, 0x7E, 0x00 } },
    { 'b', GF_DENSE, { 0xE0, 0xE0, 0xFC, 0xFE, 0xEE, 0xFE, 0xFC, 0x00 } },
    { 'c', 0,        { 0x00, 0x00, 0x7E, 0xE0, 0xE0, 0xE0, 0x7E, 0x00 } },
    { 'd', GF_DENSE, { 0x0E, 0x0E, 0x7E, 0xFE, 0xEE, 0xFE, 0x7E, 0x00 } },
    { 'e', GF_DENSE, { 0x7C, 0xFE, 0xEE, 0xFE, 0xE0, 0xFE, 0x7C, 0x00 } },
    { 'g', GF_DENSE, { 0x7E, 0xFE, 0xEE, 0xFE, 0x7E, 0x0E, 0xFC, 0x00 } },
    { 'h', GF_DENSE, { 0xE0, 0xE0, 0xFC, 0xFE, 0xEE, 0xEE, 0xEE, 0x00 } },
    { 'i', 0,        { 0x38, 0x00, 0x78, 0x38, 0x38, 0x38, 0x7C, 0x00 } },
    { 'j', 0,        { 0x1C, 0x00, 0x3C, 0x1C, 0x1C, 0xDC, 0x78, 0x00 } },
    { 'k', GF_DENSE, { 0xE0, 0xEC, 0xF8, 0xF0, 0xF8, 0xEC, 0xE6, 0x00 } },
    { 'm', GF_DENSE, { 0xEC, 0xFE, 0xFE, 0xD6, 0xD6, 0xD6, 0xD6, 0x00 } },
    { 'n', 0,        { 0x00, 0x00, 0xFC, 0xFE, 0xE6, 0xE6, 0xE6, 0x00 } },
    { 'o', 0,        { 0x00, 0x00, 0x7C, 0xFE, 0xC6, 0xFE, 0x7C, 0x00 } },
    { 'p', GF_DENSE, { 0xFC, 0xFE, 0xEE, 0xFE, 0xFC, 0xE0, 0xE0, 0x00 } },
    { 'q', GF_DENSE, { 0x7E, 0xFE, 0xEE, 0xFE, 0x7E, 0x0E, 0x0E, 0x00 } },
    { 'r', 0,        { 0x00, 0x00, 0xDC, 0xFE, 0xE0, 0xE0, 0xE0, 0x00 } },
    { 's', GF_DENSE, { 0x7E, 0xFE, 0xE0, 0x7C, 0x0E, 0xFE, 0x7C, 0x00 } },
    { 't', 0,        { 0x70, 0x70, 0xFC, 0x70, 0x70, 0x76, 0x3C, 0x00 } },
    { 'u', 0,        { 0x00, 0x00, 0xE6, 0xE6, 0xE6, 0xFE, 0x7E, 0x00 } },
    { 'v', 0,        { 0x00, 0x00, 0xC6, 0xC6, 0xEE, 0x7C, 0x38, 0x00 } },
    { 'w', 0,        { 0x00, 0x00, 0xC6, 0xD6, 0xFE, 0xFE, 0x6C, 0x00 } },
    { 'y', 0,        { 0x00, 0xC6, 0xC6, 0xFE, 0x7E, 0x0E, 0x7C, 0x00 } },
    { 'z', 0,        { 0x00, 0x00, 0xFE, 0x1C, 0x38, 0x70, 0xFE, 0x00 } },

    // --- Bold Symbols ---
    { '*', GF_DENSE | GF_PUNCT, { 0x00, 0xD6, 0x7C, 0xFE, 0x7C, 0xD6, 0x00, 0x00 } },
    { '?', GF_PUNCT,            { 0x7C, 0xFE, 0xC6, 0x1C, 0x38, 0x00, 0x38, 0x00 } },
    { '$', GF_DENSE | GF_PUNCT, { 0x38, 0x7E, 0xF0, 0x7C, 0x1E, 0xFC, 0x38, 0x00 } },
    { '#', GF_DENSE | GF_PUNCT, { 0x6C, 0xFE, 0xFE, 0x6C, 0xFE, 0xFE, 0x6C, 0x00 } },
    { '@', GF_DENSE | GF_PUNCT, { 0x7C, 0xFE, 0xDE, 0xDE, 0xFC, 0xE0, 0x7E, 0x00 } },
    { '+', GF_PUNCT,            { 0x00, 0x38, 0x38, 0xFE, 0x38, 0x38, 0x00, 0x00 } },
    { '(', GF_PUNCT,            { 0x38, 0x70, 0xE0, 0xE0, 0xE0, 0x70, 0x38, 0x00 } },
    { ')', GF_PUNCT,            { 0x38, 0x1C, 0x0E, 0x0E, 0x0E, 0x1C, 0x38, 0x00 } },
    { '[', GF_PUNCT,            { 0xF8, 0xF8, 0xE0, 0xE0, 0xE0, 0xF8, 0xF8, 0x00 } },
    { ']', GF_PUNCT,            { 0x3E, 0x3E, 0x0E, 0x0E, 0x0E, 0x3E, 0x3E, 0x00 } },
    { '>', GF_PUNCT,            { 0xE0, 0x78, 0x1C, 0x0E, 0x1C, 0x78, 0xE0, 0x00 } },
    { '<', GF_PUNCT,            { 0x0E, 0x3C, 0x70, 0xE0, 0x70, 0x3C, 0x0E, 0x00 } },
    { '~', GF_PUNCT,            { 0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { '/', GF_PUNCT,            { 0x06, 0x0E, 0x1C, 0x38, 0x70, 0xE0, 0xC0, 0x00 } },
    { '\\',GF_PUNCT,            { 0xC0, 0xE0, 0x70, 0x38, 0x1C, 0x0E, 0x06, 0x00 } },

    // --- Micro Punctuation ---
    { '-', GF_PUNCT | GF_MICRO, { 0x00, 0x00, 0x00, 0xFE, 0xFE, 0x00, 0x00, 0x00 } },
    { '_', GF_PUNCT | GF_MICRO, { 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xFE, 0x00 } },
    { '.', GF_PUNCT | GF_MICRO, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x38, 0x00 } },
    { ',', GF_PUNCT | GF_MICRO, { 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x60, 0x00 } },
    { '\'',GF_PUNCT | GF_MICRO, { 0x38, 0x38, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { ':', GF_PUNCT | GF_MICRO, { 0x00, 0x38, 0x38, 0x00, 0x38, 0x38, 0x00, 0x00 } }
};

#define NUM_GLYPHS ((int)(sizeof(s_glyphs) / sizeof(s_glyphs[0])))

static uint8_t s_glyph_q[NUM_GLYPHS][4];
static uint8_t s_dense_alnum[NUM_GLYPHS];
static int s_num_dense_alnum = 0;
static uint8_t s_dense_punct[16];
static int s_num_dense_punct = 0;

static void init_glyph_tables(void) {
    s_num_dense_alnum = 0;
    s_num_dense_punct = 0;

    for (int i = 0; i < NUM_GLYPHS; i++) {
        uint8_t tl = 0, tr = 0, bl = 0, br = 0;
        for (int r = 0; r < 7; r++) {
            uint8_t row = s_glyphs[i].rows[r];
            for (int c = 0; c < 7; c++) {
                if (row & (1 << (7 - c))) {
                    if (r <= 3 && c <= 3) tl++;
                    if (r <= 3 && c >= 3) tr++;
                    if (r >= 3 && c <= 3) bl++;
                    if (r >= 3 && c >= 3) br++;
                }
            }
        }
        s_glyph_q[i][0] = tl;
        s_glyph_q[i][1] = tr;
        s_glyph_q[i][2] = bl;
        s_glyph_q[i][3] = br;

        if (s_glyphs[i].flags & GF_DENSE) {
            if (s_glyphs[i].flags & GF_PUNCT) {
                if (s_num_dense_punct < 16) {
                    s_dense_punct[s_num_dense_punct++] = (uint8_t)i;
                }
            } else {
                s_dense_alnum[s_num_dense_alnum++] = (uint8_t)i;
            }
        }
    }
}

static int find_glyph_index(char ch) {
    for (int i = 0; i < NUM_GLYPHS; i++) {
        if (s_glyphs[i].ch == ch) return i;
    }
    if (ch >= 'a' && ch <= 'z') {
        char up = (char)(ch - 'a' + 'A');
        for (int i = 0; i < NUM_GLYPHS; i++) {
            if (s_glyphs[i].ch == up) return i;
        }
    }
    return 0;
}

// ============================================================================
// SETTINGS & PERSISTENCE
// ============================================================================

static void set_default_settings(void) {
    s_settings.min_char_size      = 2;
    s_settings.max_char_size      = 5;
    s_settings.big_char_bias      = 20; // Default 20%
    s_settings.punct_ratio        = 12;
    s_settings.imperfection       = 30;
    s_settings.show_date          = 1;  // Day/Word ON
    s_settings.show_month         = 1;  // Month ON
    s_settings.show_battery       = 1;  // Battery ON
    s_settings.show_hand_hour     = 0;  // Hour OFF
    s_settings.show_hand_minute   = 0;  // Min OFF
    s_settings.invert_colors      = 0;  // Invert OFF
#if defined(PBL_ROUND)
    s_settings.fill_mode          = 0;  // Circular Dial on round watches
#else
    s_settings.fill_mode          = 1;  // Full Screen Rectangle on rectangle watches
#endif
    s_settings.layout_style       = 0;  // 0 = Semi-Grid Multi-Scale, 1 = Organic Collage
    s_settings.contrast_mode      = 0;  // 0 = White Comp / Grey Noise, 1 = Grey Comp / White Noise, 2 = Uniform
    s_settings.random_colors      = 0;  // Pastel colors OFF
}

static void clamp_settings(void) {
    if (s_settings.min_char_size < 1) s_settings.min_char_size = 1;
    if (s_settings.min_char_size > 4) s_settings.min_char_size = 4;
    if (s_settings.max_char_size < 3) s_settings.max_char_size = 3;
    if (s_settings.max_char_size > 6) s_settings.max_char_size = 6;
    if (s_settings.min_char_size > s_settings.max_char_size) {
        s_settings.min_char_size = s_settings.max_char_size;
    }
    if (s_settings.big_char_bias > 100) s_settings.big_char_bias = 100;
    if (s_settings.punct_ratio > 50) s_settings.punct_ratio = 50;
    if (s_settings.imperfection > 100) s_settings.imperfection = 100;
    if (s_settings.show_date > 1) s_settings.show_date = 1;
    if (s_settings.show_month > 1) s_settings.show_month = 1;
    if (s_settings.show_battery > 1) s_settings.show_battery = 1;
    if (s_settings.show_hand_hour > 1) s_settings.show_hand_hour = 1;
    if (s_settings.show_hand_minute > 1) s_settings.show_hand_minute = 1;
    if (s_settings.fill_mode > 1) s_settings.fill_mode = 1;
    if (s_settings.layout_style > 1) s_settings.layout_style = 1;
    if (s_settings.contrast_mode > 2) s_settings.contrast_mode = 2;
    if (s_settings.random_colors > 1) s_settings.random_colors = 1;
#if defined(PBL_ROUND)
    s_settings.fill_mode = 0;
#endif
}

static void load_settings(void) {
    set_default_settings();
    if (persist_exists(SETTINGS_PERSIST_KEY)) {
        persist_read_data(SETTINGS_PERSIST_KEY, &s_settings, sizeof(WatchSettings));
        clamp_settings();
    }
}

static void save_settings(void) {
    clamp_settings();
    persist_write_data(SETTINGS_PERSIST_KEY, &s_settings, sizeof(WatchSettings));
}

static int tuple_to_int(const Tuple *t) {
    if (t->type == TUPLE_CSTRING) {
        const char *s = t->value->cstring;
        int val = 0;
        while (*s >= '0' && *s <= '9') {
            val = val * 10 + (*s - '0');
            s++;
        }
        return val;
    }
    if (t->length == 1) return t->value->uint8;
    if (t->length == 2) return t->value->int16;
    return (int)t->value->int32;
}

static void inbox_received_callback(DictionaryIterator *iter, void *context) {
    Tuple *t;
    if ((t = dict_find(iter, MESSAGE_KEY_MIN_CHAR_SIZE)))      s_settings.min_char_size      = (uint8_t)tuple_to_int(t);
    if ((t = dict_find(iter, MESSAGE_KEY_MAX_CHAR_SIZE)))      s_settings.max_char_size      = (uint8_t)tuple_to_int(t);
    if ((t = dict_find(iter, MESSAGE_KEY_BIG_CHAR_BIAS)))      s_settings.big_char_bias      = (uint8_t)tuple_to_int(t);
    if ((t = dict_find(iter, MESSAGE_KEY_PUNCT_RATIO)))        s_settings.punct_ratio        = (uint8_t)tuple_to_int(t);
    if ((t = dict_find(iter, MESSAGE_KEY_IMPERFECTION)))       s_settings.imperfection       = (uint8_t)tuple_to_int(t);
    if ((t = dict_find(iter, MESSAGE_KEY_SHOW_DATE)))          s_settings.show_date          = (uint8_t)(tuple_to_int(t) ? 1 : 0);
    if ((t = dict_find(iter, MESSAGE_KEY_SHOW_MONTH)))         s_settings.show_month         = (uint8_t)(tuple_to_int(t) ? 1 : 0);
    if ((t = dict_find(iter, MESSAGE_KEY_SHOW_BATTERY)))       s_settings.show_battery       = (uint8_t)(tuple_to_int(t) ? 1 : 0);
    if ((t = dict_find(iter, MESSAGE_KEY_SHOW_HAND_HOUR)))     s_settings.show_hand_hour     = (uint8_t)(tuple_to_int(t) ? 1 : 0);
    if ((t = dict_find(iter, MESSAGE_KEY_SHOW_HAND_MINUTE)))   s_settings.show_hand_minute   = (uint8_t)(tuple_to_int(t) ? 1 : 0);
    if ((t = dict_find(iter, MESSAGE_KEY_INVERT_COLORS)))      s_settings.invert_colors      = (uint8_t)(tuple_to_int(t) ? 1 : 0);
    if ((t = dict_find(iter, MESSAGE_KEY_FILL_MODE)))          s_settings.fill_mode          = (uint8_t)(tuple_to_int(t) ? 1 : 0);
    if ((t = dict_find(iter, MESSAGE_KEY_LAYOUT_STYLE)))       s_settings.layout_style       = (uint8_t)(tuple_to_int(t) ? 1 : 0);
    if ((t = dict_find(iter, MESSAGE_KEY_CONTRAST_MODE)))      s_settings.contrast_mode      = (uint8_t)tuple_to_int(t);
    if ((t = dict_find(iter, MESSAGE_KEY_RANDOM_COLORS)))      s_settings.random_colors      = (uint8_t)(tuple_to_int(t) ? 1 : 0);

    save_settings();
    if (s_canvas_layer) {
        layer_mark_dirty(s_canvas_layer);
    }
}

// ============================================================================
// GEOMETRY & CLOCK HAND CORRIDORS
// ============================================================================

typedef struct {
    int16_t screen_w, screen_h;
    int16_t cx, cy;
    bool is_full_rect;
    int16_t r_dial;
    int32_t r_dial_sq;
    int32_t r_rim_max_sq;
    int32_t r_arbor_sq;

    // Hour hand capsule
    int16_t h_x0, h_y0, h_dx, h_dy;
    int16_t h_len;
    int32_t h_len_sq;
    int32_t h_core_w_sq;
    int32_t h_near_w_sq;

    // Minute hand capsule
    int16_t m_x0, m_y0, m_dx, m_dy;
    int16_t m_len;
    int32_t m_len_sq;
    int32_t m_core_w_sq;
    int32_t m_near_w_sq;
} DialGeometry;

static bool point_in_capsule(int16_t px, int16_t py,
                             int16_t x0, int16_t y0,
                             int16_t dx, int16_t dy,
                             int32_t len_sq, int32_t half_w_sq) {
    int32_t ux = px - x0;
    int32_t uy = py - y0;
    int32_t dot = ux * dx + uy * dy;

    if (dot <= 0) {
        return (ux * ux + uy * uy) < half_w_sq;
    }
    if (dot >= len_sq) {
        int32_t ex = px - (x0 + dx);
        int32_t ey = py - (y0 + dy);
        return (ex * ex + ey * ey) < half_w_sq;
    }
    int32_t cross = dx * uy - dy * ux;
    return ((int64_t)cross * (int64_t)cross) < ((int64_t)half_w_sq * (int64_t)len_sq);
}

static bool is_in_hand_core(const DialGeometry *g, int16_t px, int16_t py) {
    int32_t dx = px - g->cx;
    int32_t dy = py - g->cy;
    if ((dx * dx + dy * dy) < g->r_arbor_sq) {
        return true;
    }
    if (point_in_capsule(px, py, g->h_x0, g->h_y0, g->h_dx, g->h_dy,
                         g->h_len_sq, g->h_core_w_sq)) {
        return true;
    }
    if (point_in_capsule(px, py, g->m_x0, g->m_y0, g->m_dx, g->m_dy,
                         g->m_len_sq, g->m_core_w_sq)) {
        return true;
    }
    return false;
}

static bool is_near_hands(const DialGeometry *g, int16_t px, int16_t py) {
    if (point_in_capsule(px, py, g->h_x0, g->h_y0, g->h_dx, g->h_dy,
                         g->h_len_sq, g->h_near_w_sq)) {
        return true;
    }
    if (point_in_capsule(px, py, g->m_x0, g->m_y0, g->m_dx, g->m_dy,
                         g->m_len_sq, g->m_near_w_sq)) {
        return true;
    }
    return false;
}

static bool is_foreground_point(const DialGeometry *g, int16_t px, int16_t py) {
    if (g->is_full_rect) {
        if (px < 2 || px >= g->screen_w - 2 || py < 2 || py >= g->screen_h - 2) {
            return false;
        }
    } else {
        int32_t dx = px - g->cx;
        int32_t dy = py - g->cy;
        if ((dx * dx + dy * dy) > g->r_dial_sq) {
            return false;
        }
    }
    return !is_in_hand_core(g, px, py);
}

static int sample_rect_quadrants(const DialGeometry *g,
                                 int16_t x0, int16_t y0, int16_t w, int16_t h,
                                 uint8_t q_out[4]) {
    q_out[0] = 0;
    q_out[1] = 0;
    q_out[2] = 0;
    q_out[3] = 0;

    for (int sy = 0; sy < 4; sy++) {
        int16_t py = y0 + ((2 * sy + 1) * h) / 8;
        for (int sx = 0; sx < 4; sx++) {
            int16_t px = x0 + ((2 * sx + 1) * w) / 8;
            if (is_foreground_point(g, px, py)) {
                int q_idx = ((sy >= 2) ? 2 : 0) + ((sx >= 2) ? 1 : 0);
                q_out[q_idx] += 3;
            }
        }
    }
    return (int)q_out[0] + (int)q_out[1] + (int)q_out[2] + (int)q_out[3];
}

// Distance from center (cx, cy) to bounding rectangle [4, 4, sw-4, sh-4] along angle
static int16_t ray_to_rect_distance(int16_t cx, int16_t cy, int16_t sw, int16_t sh, int32_t angle) {
    int32_t sin_val = sin_lookup(angle);
    int32_t cos_val = cos_lookup(angle);
    int32_t d_x = 32767, d_y = 32767;

    if (sin_val > 500) {
        d_x = ((int32_t)(sw - 4 - cx) * TRIG_MAX_RATIO) / sin_val;
    } else if (sin_val < -500) {
        d_x = ((int32_t)(cx - 4) * TRIG_MAX_RATIO) / (-sin_val);
    }

    if (cos_val > 500) {
        d_y = ((int32_t)(cy - 4) * TRIG_MAX_RATIO) / cos_val;
    } else if (cos_val < -500) {
        d_y = ((int32_t)(sh - 4 - cy) * TRIG_MAX_RATIO) / (-cos_val);
    }

    int32_t dist = (d_x < d_y) ? d_x : d_y;
    if (dist < 20) dist = 20;
    if (dist > 300) dist = 300;
    return (int16_t)dist;
}

static bool glyph_fits_cleanly(const DialGeometry *g, int glyph_idx,
                               int16_t x0, int16_t y0, int16_t bw, int16_t bh) {
    if (x0 < 1 || y0 < 1 || (x0 + bw) >= g->screen_w - 1 || (y0 + bh) >= g->screen_h - 1) {
        return false;
    }
    const uint8_t *rows = s_glyphs[glyph_idx].rows;
    int total_bits = 0;
    int outside_dial_bits = 0;

    for (int r = 0; r < 7; r++) {
        uint8_t row_bits = rows[r];
        if (!row_bits) continue;
        int16_t py0 = y0 + (r * bh) / 8;
        int16_t py1 = y0 + ((r + 1) * bh) / 8;
        int16_t py_mid = (py0 + py1) / 2;

        for (int c = 0; c < 7; c++) {
            if (row_bits & (1 << (7 - c))) {
                total_bits++;
                int16_t px0 = x0 + (c * bw) / 8;
                int16_t px1 = x0 + ((c + 1) * bw) / 8;
                int16_t px_mid = (px0 + px1) / 2;

                if (is_in_hand_core(g, px_mid, py_mid)) {
                    return false;
                }
                if (bw >= 18 && (is_in_hand_core(g, px0, py0) || is_in_hand_core(g, px1 - 1, py1 - 1))) {
                    return false;
                }

                if (!g->is_full_rect) {
                    int32_t dx = px_mid - g->cx;
                    int32_t dy = py_mid - g->cy;
                    int32_t d2 = dx * dx + dy * dy;
                    if (d2 > g->r_rim_max_sq) {
                        return false;
                    }
                    if (d2 > g->r_dial_sq) {
                        outside_dial_bits++;
                    }
                }
            }
        }
    }
    if (total_bits == 0) return false;
    if (g->is_full_rect) return true;
    return (outside_dial_bits * 9) <= (total_bits * 2);
}

static bool is_rect_completely_free_from_hands(const DialGeometry *g,
                                               int16_t rx, int16_t ry,
                                               int16_t rw, int16_t rh,
                                               int16_t margin) {
    int16_t x0 = rx - margin;
    int16_t y0 = ry - margin;
    int16_t x1 = rx + rw + margin;
    int16_t y1 = ry + rh + margin;

    if (x0 < 2 || y0 < 2 || x1 >= g->screen_w - 2 || y1 >= g->screen_h - 2) {
        return false;
    }

    if (!g->is_full_rect) {
        int16_t corners[4][2] = { {x0, y0}, {x1, y0}, {x0, y1}, {x1, y1} };
        for (int i = 0; i < 4; i++) {
            int32_t dx = corners[i][0] - g->cx;
            int32_t dy = corners[i][1] - g->cy;
            if ((dx * dx + dy * dy) > g->r_dial_sq) {
                return false;
            }
        }
    }

    // Step across date rectangle + margin to ensure hand core AND near corridor never obscure text
    for (int16_t y = y0; y <= y1; y += 4) {
        for (int16_t x = x0; x <= x1; x += 4) {
            if (is_in_hand_core(g, x, y) || is_near_hands(g, x, y)) {
                return false;
            }
        }
    }
    if (is_in_hand_core(g, x0, y0) || is_near_hands(g, x0, y0) ||
        is_in_hand_core(g, x1, y0) || is_near_hands(g, x1, y0) ||
        is_in_hand_core(g, x0, y1) || is_near_hands(g, x0, y1) ||
        is_in_hand_core(g, x1, y1) || is_near_hands(g, x1, y1)) {
        return false;
    }
    return true;
}

// ============================================================================
// HASHING, AALIB MATCHING & DRAWING
// ============================================================================

static uint32_t fast_hash(int a, int b, int salt) {
    uint32_t h = (uint32_t)(a * 374761393U + b * 668265263U + salt * 1442695041U);
    h = (h ^ (h >> 13)) * 1274126177U;
    return h ^ (h >> 16);
}

static bool subgrid_is_free(const uint64_t occ[], int max_n, int gx, int gy, int gw, int gh) {
    if (gx < 0 || gy < 0 || (gx + gw) > max_n || (gy + gh) > max_n) {
        return false;
    }
    uint64_t mask = (((uint64_t)1 << gw) - 1ULL) << gx;
    for (int y = gy; y < gy + gh; y++) {
        if (occ[y] & mask) return false;
    }
    return true;
}

static void subgrid_occupy(uint64_t occ[], int gx, int gy, int gw, int gh) {
    uint64_t mask = (((uint64_t)1 << gw) - 1ULL) << gx;
    for (int y = gy; y < gy + gh; y++) {
        occ[y] |= mask;
    }
}

static void occupy_organic_glyph_cells(uint64_t occ[], int glyph_idx,
                                       int gx0, int gy0, int gw, int gh) {
    if (glyph_idx < 0 || glyph_idx >= NUM_GLYPHS) return;
    const uint8_t *rows_data = s_glyphs[glyph_idx].rows;
    for (int dy = 0; dy < gh; dy++) {
        int r0 = (dy * 7) / gh;
        int r1 = ((dy + 1) * 7 + gh - 1) / gh;
        if (r1 > 7) r1 = 7;

        for (int dx = 0; dx < gw; dx++) {
            int c0 = (dx * 7) / gw;
            int c1 = ((dx + 1) * 7 + gw - 1) / gw;
            if (c1 > 7) c1 = 7;

            bool has_pixels = false;
            for (int r = r0; r < r1 && !has_pixels; r++) {
                uint8_t row_bits = rows_data[r];
                for (int c = c0; c < c1; c++) {
                    if (row_bits & (1 << (7 - c))) {
                        has_pixels = true;
                        break;
                    }
                }
            }
            if (has_pixels) {
                int gx = gx0 + dx;
                int gy = gy0 + dy;
                if (gx >= 0 && gx < SUB_N && gy >= 0 && gy < SUB_N) {
                    occ[gy] |= ((uint64_t)1 << gx);
                }
            }
        }
    }
}

static int match_contour_glyph(const DialGeometry *geom,
                               const uint8_t q[4], int total_cov,
                               int16_t bx, int16_t by, int16_t bw, int16_t bh,
                               bool allow_punct, bool allow_micro, uint32_t seed) {
    int best_idx = -1;
    int32_t best_err = 0x7FFFFFFF;
    uint32_t jitter_amp = 42U + ((uint32_t)s_settings.imperfection * 2U) / 3U;

    for (int i = 0; i < NUM_GLYPHS; i++) {
        uint8_t flags = s_glyphs[i].flags;
        if ((flags & GF_PUNCT) && !allow_punct) continue;
        if ((flags & GF_MICRO) && !allow_micro) continue;

        int32_t err = 0;
        for (int k = 0; k < 4; k++) {
            int32_t diff = (int32_t)s_glyph_q[i][k] - (int32_t)q[k];
            if (q[k] == 0 && s_glyph_q[i][k] > 1) {
                err += diff * diff * 4;
            } else if (diff > 0) {
                err += diff * diff * 2;
            } else {
                err += (diff * diff * 3) / 2;
            }
        }

        if (total_cov >= 21 && (flags & GF_MICRO)) {
            err += 240;
        }

        err += (int32_t)(fast_hash(i, (int)seed, 31) % jitter_amp);

        if (err < best_err) {
            if (glyph_fits_cleanly(geom, i, bx, by, bw, bh)) {
                best_err = err;
                best_idx = i;
            }
        }
    }
    return best_idx;
}

#if defined(PBL_COLOR)
static const GColor s_light_garbage_colors[] = {
    GColorPastelYellow,
    GColorIcterine,
    GColorMintGreen,
    GColorCeleste,
    GColorBabyBlueEyes,
    GColorRichBrilliantLavender,
    GColorMelon,
    GColorRajah,
    GColorInchworm,
    GColorSpringBud,
    GColorShockingPink,
    GColorBrilliantRose,
    GColorElectricBlue,
    GColorMediumAquamarine,
    GColorTiffanyBlue,
    GColorVeryLightBlue
};
#define NUM_LIGHT_GARBAGE_COLORS ((int)(sizeof(s_light_garbage_colors) / sizeof(s_light_garbage_colors[0])))

static const GColor s_dark_garbage_colors[] = {
    GColorBlack,
    GColorOxfordBlue,
    GColorDukeBlue,
    GColorDarkGreen,
    GColorMidnightGreen,
    GColorCobaltBlue,
    GColorImperialPurple,
    GColorBulgarianRose,
    GColorDarkCandyAppleRed,
    GColorWindsorTan,
    GColorArmyGreen,
    GColorIndigo
};
#define NUM_DARK_GARBAGE_COLORS ((int)(sizeof(s_dark_garbage_colors) / sizeof(s_dark_garbage_colors[0])))

static GColor get_garbage_color(uint32_t seed, GColor default_fg) {
    if (s_settings.random_colors) {
        if (s_settings.invert_colors) {
            return s_dark_garbage_colors[(seed >> 3) % NUM_DARK_GARBAGE_COLORS];
        } else {
            return s_light_garbage_colors[(seed >> 3) % NUM_LIGHT_GARBAGE_COLORS];
        }
    }
    if (s_settings.contrast_mode == 0) {
        return s_settings.invert_colors ? GColorDarkGray : GColorLightGray;
    }
    return default_fg;
}
#else
static inline GColor get_garbage_color(uint32_t seed, GColor default_fg) {
    (void)seed;
    return default_fg;
}
#endif

static void draw_glyph_rows(GContext *ctx, const uint8_t rows[8],
                            int16_t x0, int16_t y0, int16_t box_w, int16_t box_h,
                            bool is_complication, GColor fg_color) {
    if (!rows) return;
    bool dither = false;
#if defined(PBL_COLOR)
    if (is_complication) {
        GColor c;
        if (s_settings.contrast_mode == 1) {
            c = s_settings.invert_colors ? GColorDarkGray : GColorLightGray;
        } else {
            c = s_settings.invert_colors ? GColorBlack : GColorWhite;
        }
        graphics_context_set_fill_color(ctx, c);
        graphics_context_set_stroke_color(ctx, c);
    } else {
        graphics_context_set_fill_color(ctx, fg_color);
        graphics_context_set_stroke_color(ctx, fg_color);
    }
#else
    if (is_complication) {
        if (s_settings.contrast_mode == 1) {
            dither = true;
        }
    } else {
        if (s_settings.contrast_mode == 0) {
            dither = true;
        }
    }
    graphics_context_set_fill_color(ctx, fg_color);
    graphics_context_set_stroke_color(ctx, fg_color);
#endif

    for (int r = 0; r < 7; r++) {
        uint8_t row_bits = rows[r];
        if (!row_bits) continue;

        int16_t py0 = y0 + (r * box_h) / 8;
        int16_t py1 = y0 + ((r + 1) * box_h) / 8;
        int16_t rh = py1 - py0;
        if (rh <= 0) rh = 1;

        if (dither) {
            for (int c = 0; c < 7; c++) {
                if (row_bits & (1 << (7 - c))) {
                    int16_t px0 = x0 + (c * box_w) / 8;
                    int16_t px1 = x0 + ((c + 1) * box_w) / 8;
                    int16_t rw = px1 - px0;
                    if (rw <= 0) rw = 1;
                    for (int dy = 0; dy < rh; dy++) {
                        for (int dx = 0; dx < rw; dx++) {
                            if (((px0 + dx + py0 + dy) & 1) == 0) {
                                graphics_draw_pixel(ctx, GPoint(px0 + dx, py0 + dy));
                            }
                        }
                    }
                }
            }
        } else {
            int c = 0;
            while (c < 7) {
                if (row_bits & (1 << (7 - c))) {
                    int c_start = c;
                    while (c < 7 && (row_bits & (1 << (7 - c)))) {
                        c++;
                    }
                    int16_t px0 = x0 + (c_start * box_w) / 8;
                    int16_t px1 = x0 + (c * box_w) / 8;
                    int16_t rw = px1 - px0;
                    if (rw <= 0) rw = 1;
                    graphics_fill_rect(ctx, GRect(px0, py0, rw, rh), 0, GCornerNone);
                } else {
                    c++;
                }
            }
        }
    }
}

static void draw_glyph(GContext *ctx, int glyph_idx,
                       int16_t x0, int16_t y0, int16_t box_w, int16_t box_h,
                       bool is_complication, GColor fg_color) {
    if (glyph_idx < 0 || glyph_idx >= NUM_GLYPHS) return;
    draw_glyph_rows(ctx, s_glyphs[glyph_idx].rows, x0, y0, box_w, box_h, is_complication, fg_color);
}

static void get_battery_glyph_rows(uint8_t rows[8]) {
    BatteryChargeState charge = battery_state_service_peek();
    int pct = charge.charge_percent;
    bool charging = charge.is_charging;

    // Vertical filled rectangle battery:
    // Row 0: terminal cap: 0x38 (..###...)
    // Row 1: top border:   0xFE (.#######)
    // Row 6: bottom border:0xFE (.#######)
    // Rows 2..5: body interior (width 5, bits 1..5)
    rows[0] = 0x38;
    rows[1] = 0xFE;
    rows[6] = 0xFE;
    rows[7] = 0x00;

    int filled_rows = (pct + 12) / 25; // 0..4
    if (filled_rows > 4) filled_rows = 4;
    if (filled_rows < 1 && pct > 0) filled_rows = 1;

    for (int r = 2; r <= 5; r++) {
        int dist_from_bottom = 5 - r;
        if (dist_from_bottom < filled_rows) {
            rows[r] = 0xFE; // Solid filled rectangle row
        } else {
            rows[r] = 0x82; // Hollow casing (.#.....#)
        }
    }
    if (charging) {
        rows[3] = 0xBA;
        rows[4] = 0x92;
    }
}

static const char * const s_day_names[7] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

static const char * const s_month_names[12] = {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};

// ============================================================================
// LAYOUT 0: MULTI-SCALE QUADTREE SEMI-GRID
// Clean grid alignment like our original semi-grid, PLUS large 4x4 (32px),
// 3x3 (24px), and 2x2 (16px) hero blocks that automatically subdivide down to
// 1x1 (8px) around the clock hands so hands ALWAYS stay razor-sharp & visible!
// ============================================================================

static bool grid_rect_free(const uint32_t occ[], int cols, int rows,
                           int gx, int gy, int gw, int gh) {
    if (gx < 0 || gy < 0 || (gx + gw) > cols || (gy + gh) > rows) {
        return false;
    }
    uint32_t mask = ((1U << gw) - 1U) << gx;
    for (int y = gy; y < gy + gh; y++) {
        if (occ[y] & mask) return false;
    }
    return true;
}

static void grid_rect_occupy(uint32_t occ[], int gx, int gy, int gw, int gh) {
    uint32_t mask = ((1U << gw) - 1U) << gx;
    for (int y = gy; y < gy + gh; y++) {
        occ[y] |= mask;
    }
}

// Occupy only subcells where the glyph actually contains stroke pixels.
// Unused pockets (e.g. lower-right of 'P', upper-right of 'b', lower-left of 'q',
// empty areas of 'S', '4', '7', 'L', '1') remain free in occ to be nested with smaller glyphs!
static void occupy_glyph_cells(uint32_t occ[], int cols, int rows,
                               int glyph_idx, int gx0, int gy0, int bs) {
    if (glyph_idx < 0 || glyph_idx >= NUM_GLYPHS) return;
    const uint8_t *rows_data = s_glyphs[glyph_idx].rows;
    for (int dy = 0; dy < bs; dy++) {
        int r0 = (dy * 7) / bs;
        int r1 = ((dy + 1) * 7 + bs - 1) / bs;
        if (r1 > 7) r1 = 7;

        for (int dx = 0; dx < bs; dx++) {
            int c0 = (dx * 7) / bs;
            int c1 = ((dx + 1) * 7 + bs - 1) / bs;
            if (c1 > 7) c1 = 7;

            bool has_pixels = false;
            for (int r = r0; r < r1 && !has_pixels; r++) {
                uint8_t row_bits = rows_data[r];
                for (int c = c0; c < c1; c++) {
                    if (row_bits & (1 << (7 - c))) {
                        has_pixels = true;
                        break;
                    }
                }
            }
            if (has_pixels) {
                int gx = gx0 + dx;
                int gy = gy0 + dy;
                if (gx >= 0 && gx < cols && gy >= 0 && gy < rows) {
                    occ[gy] |= (1U << gx);
                }
            }
        }
    }
}

static bool try_place_grid_word(GContext *ctx, const DialGeometry *geom,
                                uint32_t occ[], int cols, int rows,
                                int16_t origin_x, int16_t origin_y,
                                int16_t cell_w, int16_t cell_h,
                                const char *word, int word_len,
                                int gx0, int gy0, int dw, int dh,
                                GColor fg_color) {
    if (gx0 < 0 || gy0 < 0 || (gx0 + word_len * dw) > cols || (gy0 + dh) > rows) {
        return false;
    }
    int16_t full_w = word_len * dw * cell_w;
    int16_t full_h = dh * cell_h;
    int16_t rx0 = origin_x + gx0 * cell_w;
    int16_t ry0 = origin_y + gy0 * cell_h;

    // Full clearance check: text banner must never be obscured or grazed by hands!
    if (!is_rect_completely_free_from_hands(geom, rx0, ry0, full_w, full_h, 3)) {
        return false;
    }

    for (int i = 0; i < word_len; i++) {
        int gx = gx0 + i * dw;
        if (!grid_rect_free(occ, cols, rows, gx, gy0, dw, dh)) {
            return false;
        }
    }

    int16_t bw = dw * cell_w;
    int16_t bh = dh * cell_h;
    for (int i = 0; i < word_len; i++) {
        int gx = gx0 + i * dw;
        int16_t bx = origin_x + gx * cell_w;
        int16_t by = origin_y + gy0 * cell_h;
        int g_idx = find_glyph_index(word[i]);
        draw_glyph(ctx, g_idx, bx, by, bw, bh, true, fg_color);
        grid_rect_occupy(occ, gx, gy0, dw, dh);
    }
    return true;
}

static bool try_place_grid_stacked_date(GContext *ctx, const DialGeometry *geom,
                                        uint32_t occ[], int cols, int rows,
                                        int16_t origin_x, int16_t origin_y,
                                        int16_t cell_w, int16_t cell_h,
                                        const char *day_str, const char *num_str,
                                        int gx0, int gy0, int dw, int dh,
                                        GColor fg_color) {
    if (gx0 < 0 || gy0 < 0 || (gx0 + 3 * dw) > cols || (gy0 + 2 * dh) > rows) {
        return false;
    }
    int16_t full_w = 3 * dw * cell_w;
    int16_t full_h = 2 * dh * cell_h;
    int16_t rx0 = origin_x + gx0 * cell_w;
    int16_t ry0 = origin_y + gy0 * cell_h;

    if (!is_rect_completely_free_from_hands(geom, rx0, ry0, full_w, full_h, 3)) {
        return false;
    }

    int num_gx0 = gx0 + (dw / 2);
    for (int i = 0; i < 3; i++) {
        int gx = gx0 + i * dw;
        if (!grid_rect_free(occ, cols, rows, gx, gy0, dw, dh)) return false;
    }
    for (int i = 0; i < 2; i++) {
        int gx = num_gx0 + i * dw;
        int gy = gy0 + dh;
        if (!grid_rect_free(occ, cols, rows, gx, gy, dw, dh)) return false;
    }

    int16_t bw = dw * cell_w;
    int16_t bh = dh * cell_h;
    for (int i = 0; i < 3; i++) {
        int gx = gx0 + i * dw;
        draw_glyph(ctx, find_glyph_index(day_str[i]),
                   origin_x + gx * cell_w, origin_y + gy0 * cell_h,
                   bw, bh, true, fg_color);
        grid_rect_occupy(occ, gx, gy0, dw, dh);
    }
    for (int i = 0; i < 2; i++) {
        int gx = num_gx0 + i * dw;
        int gy = gy0 + dh;
        draw_glyph(ctx, find_glyph_index(num_str[i]),
                   origin_x + gx * cell_w, origin_y + gy * cell_h,
                   bw, bh, true, fg_color);
        grid_rect_occupy(occ, gx, gy, dw, dh);
    }
    return true;
}

static void render_semi_grid(Layer *layer, GContext *ctx, const DialGeometry *geom, GColor fg_color) {
    GRect bounds = layer_get_bounds(layer);
    int16_t min_dim = (bounds.size.w < bounds.size.h) ? bounds.size.w : bounds.size.h;
    int16_t cell_w = min_dim / 25; // 8px on Emery, 6px on 144x168, 10px on Gabbro
    if (cell_w < 6) cell_w = 6;
    int16_t cell_h = cell_w + 1;   // Slight portrait aspect ratio (8x9 on Emery, 6x7 on 144x168)

    int cols = geom->is_full_rect ? ((bounds.size.w - 4) / cell_w) : ((min_dim - 6) / cell_w);
    int rows = geom->is_full_rect ? ((bounds.size.h - 4) / cell_h) : ((min_dim - 6) / cell_h);
    if (cols > 30) cols = 30;
    if (rows > MAX_GRID_ROWS) rows = MAX_GRID_ROWS;

    int16_t origin_x = (bounds.size.w - cols * cell_w) / 2;
    int16_t origin_y = (bounds.size.h - rows * cell_h) / 2;

    uint32_t occ[MAX_GRID_ROWS];
    for (int r = 0; r < rows; r++) occ[r] = 0U;

    int minute_salt = s_current_time.tm_hour * 60 + s_current_time.tm_min;

    // 1. Atomic Complication Word (e.g. "SAT-31")
    // Placed ONLY as an unobstructed whole so digits are NEVER dropped mid-word.
    if (s_settings.show_date) {
        char date_word[10];
        char num_word[4];
        int wday = (s_current_time.tm_wday >= 0 && s_current_time.tm_wday < 7)
                   ? s_current_time.tm_wday : 0;
        snprintf(date_word, sizeof(date_word), "%s-%02d", s_day_names[wday], s_current_time.tm_mday);
        snprintf(num_word, sizeof(num_word), "%02d", s_current_time.tm_mday);
        bool placed = false;

        // Pass A: Large 3x3 hero banner ("SAT-31" = 18 cols wide)
        if (s_settings.max_char_size >= 5 && cols >= 20) {
            static const int pct_y[6] = { 62, 24, 55, 31, 68, 17 };
            static const int off_x[5] = { 0, -1, 1, -2, 2 };
            int cx_gx = (cols - 18) / 2;
            for (int py = 0; py < 6 && !placed; py++) {
                int gy = (rows * pct_y[py]) / 100;
                for (int ox = 0; ox < 5 && !placed; ox++) {
                    placed = try_place_grid_word(ctx, geom, occ, cols, rows,
                                                 origin_x, origin_y, cell_w, cell_h,
                                                 date_word, 6, cx_gx + off_x[ox], gy, 3, 3, fg_color);
                }
            }
        }

        // Pass B: Medium 2x3 banner ("SAT-31" = 12 cols wide, shifts left/right to dodge vertical hands)
        if (!placed) {
            static const int pct_y[8] = { 62, 24, 55, 31, 70, 16, 46, 38 };
            static const int off_x[7] = { 0, -2, 2, -4, 4, -6, 6 };
            int cx_gx = (cols - 12) / 2;
            for (int py = 0; py < 8 && !placed; py++) {
                int gy = (rows * pct_y[py]) / 100;
                for (int ox = 0; ox < 7 && !placed; ox++) {
                    placed = try_place_grid_word(ctx, geom, occ, cols, rows,
                                                 origin_x, origin_y, cell_w, cell_h,
                                                 date_word, 6, cx_gx + off_x[ox], gy, 2, 3, fg_color);
                }
            }
        }

        // Pass C: Guaranteed 2-line quadrant badge ("SAT" over "31", 6x4 cells)
        if (!placed) {
            int q_gx[6] = { cols - 8, 2, cols - 8, 2, 2, cols - 8 };
            int q_gy[6] = { (rows * 58) / 100, (rows * 58) / 100,
                            (rows * 24) / 100, (rows * 24) / 100,
                            (rows * 41) / 100, (rows * 41) / 100 };
            for (int q = 0; q < 6 && !placed; q++) {
                placed = try_place_grid_stacked_date(ctx, geom, occ, cols, rows,
                                                     origin_x, origin_y, cell_w, cell_h,
                                                     s_day_names[wday], num_word,
                                                     q_gx[q], q_gy[q], 2, 2, fg_color);
            }
        }
    }

    // 1b. Month Complication (e.g. "NOV")
    if (s_settings.show_month) {
        int mon = (s_current_time.tm_mon >= 0 && s_current_time.tm_mon < 12)
                  ? s_current_time.tm_mon : 0;
        const char *month_str = s_month_names[mon];
        bool placed_mon = false;
        static const int m_py[8] = { 24, 70, 32, 60, 42, 50, 18, 76 };
        static const int m_px[6] = { 4, 68, 18, 52, 32, 44 };
        for (int py = 0; py < 8 && !placed_mon; py++) {
            int gy = (rows * m_py[py]) / 100;
            for (int px = 0; px < 6 && !placed_mon; px++) {
                int gx = (cols * m_px[px]) / 100;
                placed_mon = try_place_grid_word(ctx, geom, occ, cols, rows,
                                                 origin_x, origin_y, cell_w, cell_h,
                                                 month_str, 3, gx, gy, 2, 2, fg_color);
            }
        }
    }

    // 1c. Battery Complication (Single character filled rectangle)
    if (s_settings.show_battery) {
        uint8_t batt_rows[8];
        get_battery_glyph_rows(batt_rows);
        bool placed_batt = false;
        int bw_cells = 2, bh_cells = 2;
        static const int b_py[8] = { 18, 74, 28, 65, 38, 54, 14, 80 };
        static const int b_px[8] = { 78, 14, 68, 24, 82, 10, 48, 38 };
        for (int p = 0; p < 8 && !placed_batt; p++) {
            int gy = (rows * b_py[p]) / 100;
            int gx = (cols * b_px[p]) / 100;
            if (gx >= 0 && gy >= 0 && (gx + bw_cells) <= cols && (gy + bh_cells) <= rows) {
                if (grid_rect_free(occ, cols, rows, gx, gy, bw_cells, bh_cells)) {
                    int16_t bx = origin_x + gx * cell_w;
                    int16_t by = origin_y + gy * cell_h;
                    int16_t bw = bw_cells * cell_w;
                    int16_t bh = bh_cells * cell_h;
                    if (is_rect_completely_free_from_hands(geom, bx, by, bw, bh, 3)) {
                        draw_glyph_rows(ctx, batt_rows, bx, by, bw, bh, true, fg_color);
                        grid_rect_occupy(occ, gx, gy, bw_cells, bh_cells);
                        placed_batt = true;
                    }
                }
            }
        }
    }

    // 1d. Hour Digits at Analog Hand Tip
    if (s_settings.show_hand_hour) {
        int h_val = clock_is_24h_style() ? s_current_time.tm_hour : (s_current_time.tm_hour % 12);
        if (!clock_is_24h_style() && h_val == 0) h_val = 12;
        char h_str[4];
        snprintf(h_str, sizeof(h_str), "%d", h_val);
        int h_len_chars = strlen(h_str);

        int16_t tx = geom->cx + (geom->h_dx * 120) / 100;
        int16_t ty = geom->cy + (geom->h_dy * 120) / 100;
        int dw = 2, dh = 2;
        int gx = (tx - origin_x) / cell_w - (h_len_chars * dw) / 2;
        int gy = (ty - origin_y) / cell_h - dh / 2;
        if (gx < 0) gx = 0;
        if (gy < 0) gy = 0;
        if (gx + h_len_chars * dw > cols) gx = cols - h_len_chars * dw;
        if (gy + dh > rows) gy = rows - dh;

        for (int i = 0; i < h_len_chars; i++) {
            int cgx = gx + i * dw;
            int16_t bx = origin_x + cgx * cell_w;
            int16_t by = origin_y + gy * cell_h;
            int16_t bw = dw * cell_w;
            int16_t bh = dh * cell_h;
            draw_glyph(ctx, find_glyph_index(h_str[i]), bx, by, bw, bh, true, fg_color);
            grid_rect_occupy(occ, cgx, gy, dw, dh);
        }
    }

    // 1e. Minute Digits at Analog Hand Tip
    if (s_settings.show_hand_minute) {
        char m_str[4];
        snprintf(m_str, sizeof(m_str), "%02d", s_current_time.tm_min);
        int m_len_chars = 2;

        int16_t tx = geom->cx + (geom->m_dx * 84) / 100;
        int16_t ty = geom->cy + (geom->m_dy * 84) / 100;
        int dw = 2, dh = 2;
        int gx = (tx - origin_x) / cell_w - (m_len_chars * dw) / 2;
        int gy = (ty - origin_y) / cell_h - dh / 2;
        if (gx < 0) gx = 0;
        if (gy < 0) gy = 0;
        if (gx + m_len_chars * dw > cols) gx = cols - m_len_chars * dw;
        if (gy + dh > rows) gy = rows - dh;

        for (int i = 0; i < m_len_chars; i++) {
            int cgx = gx + i * dw;
            int16_t bx = origin_x + cgx * cell_w;
            int16_t by = origin_y + gy * cell_h;
            int16_t bw = dw * cell_w;
            int16_t bh = dh * cell_h;
            draw_glyph(ctx, find_glyph_index(m_str[i]), bx, by, bw, bh, true, fg_color);
            grid_rect_occupy(occ, cgx, gy, dw, dh);
        }
    }

    // 2. Multi-Scale Hero & Medium Grid Blocks (5x5, 4x4, 3x3, 2x2 cells)
    // Larger min_char_size strictly enforces larger character sizes across the face!
    int min_bs = (int)s_settings.min_char_size; // 1..4
    if (min_bs < 1) min_bs = 1;
    if (min_bs > 4) min_bs = 4;
    int max_bs = (int)s_settings.max_char_size - 1; // 3..6 -> 2..5
    if (max_bs < min_bs) max_bs = min_bs;
    if (max_bs > 5) max_bs = 5;

    int loop_min_bs = (min_bs > 1) ? min_bs : 2;
    for (int bs = max_bs; bs >= loop_min_bs; bs--) {
        int accept_pct;
        if (bs >= 4) {
            accept_pct = 35 + (s_settings.big_char_bias * 55) / 100;
        } else if (bs == 3) {
            accept_pct = 25 + (s_settings.big_char_bias * 50) / 100;
        } else {
            accept_pct = (min_bs == 2)
                         ? (25 + (s_settings.big_char_bias * 45) / 100)
                         : (15 + (s_settings.big_char_bias * 30) / 100);
        }

        for (int gy = 0; gy <= rows - bs; gy++) {
            for (int gx = 0; gx <= cols - bs; gx++) {
                if (!grid_rect_free(occ, cols, rows, gx, gy, bs, bs)) {
                    continue;
                }
                uint32_t h = fast_hash(gx, gy, bs * 37 + (minute_salt / 15));
                if ((int)(h % 100) >= accept_pct) {
                    continue;
                }

                int16_t bx = origin_x + gx * cell_w;
                int16_t by = origin_y + gy * cell_h;
                int16_t bw = bs * cell_w;
                int16_t bh = bs * cell_h;

                uint8_t q[4];
                int total_cov = sample_rect_quadrants(geom, bx, by, bw, bh, q);
                if (total_cov < 48) {
                    continue;
                }

                bool allow_punct = ((int)((h >> 8) % 100) < (int)s_settings.punct_ratio);
                int g_idx = (allow_punct && s_num_dense_punct > 0)
                            ? s_dense_punct[(h >> 12) % s_num_dense_punct]
                            : s_dense_alnum[(h >> 12) % s_num_dense_alnum];

                if (glyph_fits_cleanly(geom, g_idx, bx, by, bw, bh)) {
                    draw_glyph(ctx, g_idx, bx, by, bw, bh, false, get_garbage_color(h, fg_color));
                    occupy_glyph_cells(occ, cols, rows, g_idx, gx, gy, bs);
                }
            }
        }
    }

    // 3. 1x1 Base Cells: Seal Hand Contours & Outer Rim
    for (int gy = 0; gy < rows; gy++) {
        for (int gx = 0; gx < cols; gx++) {
            if (!grid_rect_free(occ, cols, rows, gx, gy, 1, 1)) {
                continue;
            }

            int16_t bx = origin_x + gx * cell_w;
            int16_t by = origin_y + gy * cell_h;
            uint8_t q[4];
            int total_cov = sample_rect_quadrants(geom, bx, by, cell_w, cell_h, q);
            if (total_cov == 0) {
                continue;
            }

            uint32_t h = fast_hash(gx, gy, minute_salt + 97);
            int16_t mid_x = bx + cell_w / 2;
            int16_t mid_y = by + cell_h / 2;
            bool near_hand = is_near_hands(geom, mid_x, mid_y);

            // If user requested min character size > 1, do NOT fill open interior spaces with 1x1!
            if (min_bs > 1 && total_cov >= 45 && !near_hand) {
                continue;
            }

            // On outer circle rim (away from hands!), skip minor slivers per imperfection
            if (!geom->is_full_rect && !near_hand && total_cov < 27) {
                int skip_pct = (total_cov <= 15) ? ((int)s_settings.imperfection + 25)
                                                 : ((int)s_settings.imperfection / 2);
                if ((int)((h >> 16) % 100) < skip_pct) {
                    continue;
                }
            }

            bool allow_punct = ((int)((h >> 8) % 100) < (int)s_settings.punct_ratio);
            bool allow_micro = allow_punct && (total_cov <= 18) && ((h & 0x3) == 0);

            int g_idx = -1;
            if (total_cov >= 45) {
                int cand = (allow_punct && s_num_dense_punct > 0)
                           ? s_dense_punct[(h >> 12) % s_num_dense_punct]
                           : s_dense_alnum[(h >> 12) % s_num_dense_alnum];
                if (glyph_fits_cleanly(geom, cand, bx, by, cell_w, cell_h)) {
                    g_idx = cand;
                }
            } else {
                g_idx = match_contour_glyph(geom, q, total_cov,
                                            bx, by, cell_w, cell_h,
                                            allow_punct, allow_micro, h);
                if (g_idx < 0 && near_hand && !allow_punct && s_settings.punct_ratio > 0) {
                    g_idx = match_contour_glyph(geom, q, total_cov,
                                                bx, by, cell_w, cell_h,
                                                true, total_cov <= 15, h);
                }
            }

            if (g_idx >= 0) {
                draw_glyph(ctx, g_idx, bx, by, cell_w, cell_h, false, get_garbage_color(h, fg_color));
                grid_rect_occupy(occ, gx, gy, 1, 1);
            }
        }
    }
}

// ============================================================================
// LAYOUT 1: ORGANIC COLLAGE (Scattered 48x48 Sub-grid Multi-Scale Packing)
// ============================================================================

static const uint8_t LVL_W[7] = { 0,  2,  3,  4,  6,  8, 10 };
static const uint8_t LVL_H[7] = { 0,  3,  4,  5,  7, 10, 12 };
static const uint16_t LVL_STRIDE[7] = { 1, 733, 1229, 977, 1619, 1397, 1511 };

static bool try_place_organic_word(GContext *ctx, const DialGeometry *geom,
                                   uint64_t occ[], int16_t origin_x, int16_t origin_y,
                                   int16_t span_x, int16_t span_y,
                                   const char *word, int word_len,
                                   int gx0, int gy0, int dw, int dh,
                                   int minute_salt, GColor fg_color) {
    if (gx0 < 0 || gy0 < 0 || (gx0 + word_len * dw) > SUB_N || (gy0 + dh) > SUB_N) {
        return false;
    }
    int16_t rx0 = origin_x + (gx0 * span_x) / SUB_N;
    int16_t ry0 = origin_y + (gy0 * span_y) / SUB_N;
    int16_t full_w = origin_x + ((gx0 + word_len * dw) * span_x) / SUB_N - rx0;
    int16_t full_h = origin_y + ((gy0 + dh) * span_y) / SUB_N - ry0;
    if (!is_rect_completely_free_from_hands(geom, rx0, ry0, full_w, full_h, 3)) {
        return false;
    }

    int gy_arr[8];
    for (int i = 0; i < word_len; i++) {
        int gx = gx0 + i * dw;
        int jitter = (s_settings.imperfection >= 25)
                     ? ((int)(fast_hash(i, minute_salt, 7) % 3) - 1) : 0;
        int gy = gy0 + jitter;
        if (gy < 0 || (gy + dh) > SUB_N) gy = gy0;

        int g_idx = find_glyph_index(word[i]);
        int16_t bx = origin_x + (gx * span_x) / SUB_N;
        int16_t bw = origin_x + ((gx + dw) * span_x) / SUB_N - bx;
        int16_t by = origin_y + (gy * span_y) / SUB_N;
        int16_t bh = origin_y + ((gy + dh) * span_y) / SUB_N - by;
        uint8_t q[4];

        if (!subgrid_is_free(occ, SUB_N, gx, gy, dw, dh) ||
            sample_rect_quadrants(geom, bx, by, bw, bh, q) < 45 ||
            !glyph_fits_cleanly(geom, g_idx, bx, by, bw, bh)) {
            if (gy == gy0) return false;
            gy = gy0;
            by = origin_y + (gy * span_y) / SUB_N;
            bh = origin_y + ((gy + dh) * span_y) / SUB_N - by;
            if (!subgrid_is_free(occ, SUB_N, gx, gy, dw, dh) ||
                sample_rect_quadrants(geom, bx, by, bw, bh, q) < 45 ||
                !glyph_fits_cleanly(geom, g_idx, bx, by, bw, bh)) {
                return false;
            }
        }
        gy_arr[i] = gy;
    }

    for (int i = 0; i < word_len; i++) {
        int gx = gx0 + i * dw;
        int gy = gy_arr[i];
        int g_idx = find_glyph_index(word[i]);
        int16_t bx = origin_x + (gx * span_x) / SUB_N;
        int16_t bw = origin_x + ((gx + dw) * span_x) / SUB_N - bx;
        int16_t by = origin_y + (gy * span_y) / SUB_N;
        int16_t bh = origin_y + ((gy + dh) * span_y) / SUB_N - by;
        draw_glyph(ctx, g_idx, bx, by, bw, bh, true, fg_color);
        subgrid_occupy(occ, gx, gy, dw, dh);
    }
    return true;
}

static void render_organic_collage(Layer *layer, GContext *ctx, const DialGeometry *geom, GColor fg_color) {
    GRect bounds = layer_get_bounds(layer);
    int16_t min_dim = (bounds.size.w < bounds.size.h) ? bounds.size.w : bounds.size.h;
    int16_t span_x = geom->is_full_rect ? (bounds.size.w - 4) : (min_dim - 6);
    int16_t span_y = geom->is_full_rect ? (bounds.size.h - 4) : (min_dim - 6);
    int16_t origin_x = (bounds.size.w - span_x) / 2;
    int16_t origin_y = (bounds.size.h - span_y) / 2;

    int minute_salt = s_current_time.tm_hour * 60 + s_current_time.tm_min;

    uint64_t occ[SUB_N];
    for (int y = 0; y < SUB_N; y++) occ[y] = 0ULL;

    // 1. Atomic Day / Date Word ("SAT-31" or stacked "SAT" / "31")
    if (s_settings.show_date) {
        char date_word[10];
        char num_word[4];
        int wday = (s_current_time.tm_wday >= 0 && s_current_time.tm_wday < 7)
                   ? s_current_time.tm_wday : 0;
        snprintf(date_word, sizeof(date_word), "%s-%02d", s_day_names[wday], s_current_time.tm_mday);
        snprintf(num_word, sizeof(num_word), "%02d", s_current_time.tm_mday);
        bool placed = false;

        if (s_settings.max_char_size >= 4) {
            static const int cand_gy[6] = { 30, 11, 27, 14, 33, 8 };
            static const int cand_gx[5] = { 6, 4, 8, 2, 10 };
            for (int r = 0; r < 6 && !placed; r++) {
                for (int c = 0; c < 5 && !placed; c++) {
                    placed = try_place_organic_word(ctx, geom, occ, origin_x, origin_y,
                                                    span_x, span_y, date_word, 6,
                                                    cand_gx[c], cand_gy[r], 6, 8,
                                                    minute_salt, fg_color);
                }
            }
        }

        if (!placed) {
            static const int cand_gy[7] = { 30, 12, 26, 16, 34, 8, 21 };
            static const int cand_gx[5] = { 12, 8, 16, 2, 22 };
            for (int r = 0; r < 7 && !placed; r++) {
                for (int c = 0; c < 5 && !placed; c++) {
                    placed = try_place_organic_word(ctx, geom, occ, origin_x, origin_y,
                                                    span_x, span_y, date_word, 6,
                                                    cand_gx[c], cand_gy[r], 4, 6,
                                                    minute_salt, fg_color);
                }
            }
        }

        // Stacked 2-line quadrant fallback ("SAT" over "31")
        if (!placed) {
            static const int q_gx[6] = { 30, 6, 30, 6, 4, 32 };
            static const int q_gy[6] = { 26, 26, 11, 11, 18, 18 };
            for (int q = 0; q < 6 && !placed; q++) {
                int dw = 4, dh = 6;
                int gx0 = q_gx[q], gy0 = q_gy[q];
                bool ok = true;
                for (int i = 0; i < 3 && ok; i++) {
                    int gx = gx0 + i * dw;
                    int16_t bx = origin_x + (gx * span_x) / SUB_N;
                    int16_t bw = origin_x + ((gx + dw) * span_x) / SUB_N - bx;
                    int16_t by = origin_y + (gy0 * span_y) / SUB_N;
                    int16_t bh = origin_y + ((gy0 + dh) * span_y) / SUB_N - by;
                    uint8_t qv[4];
                    if (!subgrid_is_free(occ, SUB_N, gx, gy0, dw, dh) ||
                        sample_rect_quadrants(geom, bx, by, bw, bh, qv) < 45 ||
                        !glyph_fits_cleanly(geom, find_glyph_index(s_day_names[wday][i]), bx, by, bw, bh)) {
                        ok = false;
                    }
                }
                for (int i = 0; i < 2 && ok; i++) {
                    int gx = gx0 + 2 + i * dw;
                    int gy = gy0 + dh;
                    int16_t bx = origin_x + (gx * span_x) / SUB_N;
                    int16_t bw = origin_x + ((gx + dw) * span_x) / SUB_N - bx;
                    int16_t by = origin_y + (gy * span_y) / SUB_N;
                    int16_t bh = origin_y + ((gy + dh) * span_y) / SUB_N - by;
                    uint8_t qv[4];
                    if (!subgrid_is_free(occ, SUB_N, gx, gy, dw, dh) ||
                        sample_rect_quadrants(geom, bx, by, bw, bh, qv) < 45 ||
                        !glyph_fits_cleanly(geom, find_glyph_index(num_word[i]), bx, by, bw, bh)) {
                        ok = false;
                    }
                }
                if (ok) {
                    try_place_organic_word(ctx, geom, occ, origin_x, origin_y,
                                           span_x, span_y, s_day_names[wday], 3,
                                           gx0, gy0, dw, dh, 0, fg_color);
                    try_place_organic_word(ctx, geom, occ, origin_x, origin_y,
                                           span_x, span_y, num_word, 2,
                                           gx0 + 2, gy0 + dh, dw, dh, 0, fg_color);
                    placed = true;
                }
            }
        }
    }

    // 1b. Month Complication (e.g. "NOV")
    if (s_settings.show_month) {
        int mon = (s_current_time.tm_mon >= 0 && s_current_time.tm_mon < 12) ? s_current_time.tm_mon : 0;
        const char *month_str = s_month_names[mon];
        bool placed_mon = false;
        static const int m_gy[6] = { 20, 28, 12, 33, 16, 25 };
        static const int m_gx[6] = { 6, 26, 12, 22, 30, 4 };
        for (int i = 0; i < 6 && !placed_mon; i++) {
            placed_mon = try_place_organic_word(ctx, geom, occ, origin_x, origin_y,
                                                span_x, span_y, month_str, 3,
                                                m_gx[i], m_gy[i], 4, 6,
                                                minute_salt, fg_color);
        }
    }

    // 1c. Battery Complication (Single character filled rectangle)
    if (s_settings.show_battery) {
        uint8_t batt_rows[8];
        get_battery_glyph_rows(batt_rows);
        bool placed_batt = false;
        static const int b_gy[6] = { 14, 32, 20, 26, 10, 35 };
        static const int b_gx[6] = { 32, 8, 28, 14, 35, 10 };
        int dw = 4, dh = 6;
        for (int i = 0; i < 6 && !placed_batt; i++) {
            int gx = b_gx[i], gy = b_gy[i];
            if (subgrid_is_free(occ, SUB_N, gx, gy, dw, dh)) {
                int16_t bx = origin_x + (gx * span_x) / SUB_N;
                int16_t by = origin_y + (gy * span_y) / SUB_N;
                int16_t bw = origin_x + ((gx + dw) * span_x) / SUB_N - bx;
                int16_t bh = origin_y + ((gy + dh) * span_y) / SUB_N - by;
                if (is_rect_completely_free_from_hands(geom, bx, by, bw, bh, 3)) {
                    draw_glyph_rows(ctx, batt_rows, bx, by, bw, bh, true, fg_color);
                    subgrid_occupy(occ, gx, gy, dw, dh);
                    placed_batt = true;
                }
            }
        }
    }

    // 1d. Hour Digits at Analog Hand Tip
    if (s_settings.show_hand_hour) {
        int h_val = clock_is_24h_style() ? s_current_time.tm_hour : (s_current_time.tm_hour % 12);
        if (!clock_is_24h_style() && h_val == 0) h_val = 12;
        char h_str[4];
        snprintf(h_str, sizeof(h_str), "%d", h_val);
        int h_len_chars = strlen(h_str);

        int16_t tx = geom->cx + (geom->h_dx * 120) / 100;
        int16_t ty = geom->cy + (geom->h_dy * 120) / 100;
        int dw = 3, dh = 5;
        int gx = ((tx - origin_x) * SUB_N) / span_x - (h_len_chars * dw) / 2;
        int gy = ((ty - origin_y) * SUB_N) / span_y - dh / 2;
        if (gx < 0) gx = 0;
        if (gy < 0) gy = 0;
        if (gx + h_len_chars * dw > SUB_N) gx = SUB_N - h_len_chars * dw;
        if (gy + dh > SUB_N) gy = SUB_N - dh;

        for (int i = 0; i < h_len_chars; i++) {
            int cgx = gx + i * dw;
            int16_t bx = origin_x + (cgx * span_x) / SUB_N;
            int16_t by = origin_y + (gy * span_y) / SUB_N;
            int16_t bw = origin_x + ((cgx + dw) * span_x) / SUB_N - bx;
            int16_t bh = origin_y + ((gy + dh) * span_y) / SUB_N - by;
            draw_glyph(ctx, find_glyph_index(h_str[i]), bx, by, bw, bh, true, fg_color);
            subgrid_occupy(occ, cgx, gy, dw, dh);
        }
    }

    // 1e. Minute Digits at Analog Hand Tip
    if (s_settings.show_hand_minute) {
        char m_str[4];
        snprintf(m_str, sizeof(m_str), "%02d", s_current_time.tm_min);
        int m_len_chars = 2;

        int16_t tx = geom->cx + (geom->m_dx * 84) / 100;
        int16_t ty = geom->cy + (geom->m_dy * 84) / 100;
        int dw = 3, dh = 5;
        int gx = ((tx - origin_x) * SUB_N) / span_x - (m_len_chars * dw) / 2;
        int gy = ((ty - origin_y) * SUB_N) / span_y - dh / 2;
        if (gx < 0) gx = 0;
        if (gy < 0) gy = 0;
        if (gx + m_len_chars * dw > SUB_N) gx = SUB_N - m_len_chars * dw;
        if (gy + dh > SUB_N) gy = SUB_N - dh;

        for (int i = 0; i < m_len_chars; i++) {
            int cgx = gx + i * dw;
            int16_t bx = origin_x + (cgx * span_x) / SUB_N;
            int16_t by = origin_y + (gy * span_y) / SUB_N;
            int16_t bw = origin_x + ((cgx + dw) * span_x) / SUB_N - bx;
            int16_t bh = origin_y + ((gy + dh) * span_y) / SUB_N - by;
            draw_glyph(ctx, find_glyph_index(m_str[i]), bx, by, bw, bh, true, fg_color);
            subgrid_occupy(occ, cgx, gy, dw, dh);
        }
    }

    // 2. Scattered Multi-Scale Pass (max_lvl down to min_lvl)
    int min_lvl = s_settings.min_char_size;
    if (min_lvl < 1) min_lvl = 1;
    if (min_lvl > 4) min_lvl = 4;
    int max_lvl = s_settings.max_char_size;
    if (max_lvl < min_lvl) max_lvl = min_lvl;
    if (max_lvl > 6) max_lvl = 6;
    int max_px_jitter = (s_settings.imperfection >= 40) ? 2 :
                        ((s_settings.imperfection >= 15) ? 1 : 0);

    for (int lvl = max_lvl; lvl >= min_lvl; lvl--) {
        int base_gw = LVL_W[lvl];
        int base_gh = LVL_H[lvl];
        uint16_t stride = LVL_STRIDE[lvl];

        int accept_pct;
        if (lvl >= 4) {
            accept_pct = 35 + (s_settings.big_char_bias * 55) / 100;
        } else if (lvl == 3) {
            accept_pct = 25 + (s_settings.big_char_bias * 50) / 100;
        } else if (lvl == 2) {
            accept_pct = (min_lvl == 2) ? 75 : 45;
        } else {
            accept_pct = 85;
        }

        int idx = (int)(fast_hash(lvl, minute_salt, 53) % SUB_CELLS);

        for (int step = 0; step < SUB_CELLS; step++) {
            int gx = idx % SUB_N;
            int gy = idx / SUB_N;
            idx = (idx + stride) % SUB_CELLS;

            uint32_t h = fast_hash(gx, gy, minute_salt + lvl * 19);
            int gw = base_gw;
            int gh = base_gh;
            if (lvl >= 3 && s_settings.imperfection >= 20) {
                if ((h & 0x7) == 0) gw++;
                else if ((h & 0x7) == 1) gh++;
            }

            if (!subgrid_is_free(occ, SUB_N, gx, gy, gw, gh)) continue;
            if (lvl > min_lvl && (int)((h >> 4) % 100) >= accept_pct) continue;

            int16_t bx = origin_x + (gx * span_x) / SUB_N;
            int16_t by = origin_y + (gy * span_y) / SUB_N;
            int16_t bw = origin_x + ((gx + gw) * span_x) / SUB_N - bx;
            int16_t bh = origin_y + ((gy + gh) * span_y) / SUB_N - by;

            if (max_px_jitter > 0) {
                bx += (int16_t)((int)((h >> 11) % (2 * max_px_jitter + 1)) - max_px_jitter);
                by += (int16_t)((int)((h >> 15) % (2 * max_px_jitter + 1)) - max_px_jitter);
            }

            uint8_t q[4];
            int total_cov = sample_rect_quadrants(geom, bx, by, bw, bh, q);
            if (total_cov == 0) continue;

            int min_cov = (lvl >= 4) ? 45 : ((lvl == 3) ? 21 : 12);
            if (total_cov < min_cov) continue;

            int16_t mid_x = bx + bw / 2;
            int16_t mid_y = by + bh / 2;
            bool near_hand = is_near_hands(geom, mid_x, mid_y);
            if (min_lvl > 1 && lvl == 1 && total_cov >= 45 && !near_hand) {
                continue;
            }
            if (!geom->is_full_rect && lvl == min_lvl && total_cov < 27 && !near_hand) {
                int skip_chance = (total_cov <= 18) ? (int)s_settings.imperfection
                                                    : ((int)s_settings.imperfection / 2);
                if ((int)((h >> 19) % 100) < skip_chance) continue;
            }

            bool allow_punct = ((int)((h >> 8) % 100) < (int)s_settings.punct_ratio);
            bool allow_micro = allow_punct && (lvl <= 2) && (total_cov <= 18) && ((h & 0x3) == 0);

            int g_idx = -1;
            if (total_cov >= 45) {
                int cand = (allow_punct && s_num_dense_punct > 0)
                           ? s_dense_punct[(h >> 12) % s_num_dense_punct]
                           : s_dense_alnum[(h >> 12) % s_num_dense_alnum];
                if (glyph_fits_cleanly(geom, cand, bx, by, bw, bh)) {
                    g_idx = cand;
                }
            } else {
                g_idx = match_contour_glyph(geom, q, total_cov, bx, by, bw, bh, allow_punct, allow_micro, h);
                if (g_idx < 0 && near_hand && !allow_punct && s_settings.punct_ratio > 0) {
                    g_idx = match_contour_glyph(geom, q, total_cov, bx, by, bw, bh, true, total_cov <= 15, h);
                }
            }

            if (g_idx >= 0) {
                draw_glyph(ctx, g_idx, bx, by, bw, bh, false, get_garbage_color(h, fg_color));
                if (lvl >= 4) {
                    occupy_organic_glyph_cells(occ, g_idx, gx, gy, gw, gh);
                } else {
                    subgrid_occupy(occ, gx, gy, gw, gh);
                }
            }
        }
    }

    // 3. Interior & Hand-Flank Alphanumeric Gap Sealer (keeps hands crisp when min_lvl >= 2)
    if (min_lvl >= 2) {
        int gw = 2, gh = 3;
        int idx = (int)(fast_hash(99, minute_salt, 71) % SUB_CELLS);
        for (int step = 0; step < SUB_CELLS; step++) {
            int gx = idx % SUB_N;
            int gy = idx / SUB_N;
            idx = (idx + 733) % SUB_CELLS;

            if (!subgrid_is_free(occ, SUB_N, gx, gy, gw, gh)) continue;

            int16_t bx = origin_x + (gx * span_x) / SUB_N;
            int16_t by = origin_y + (gy * span_y) / SUB_N;
            int16_t bw = origin_x + ((gx + gw) * span_x) / SUB_N - bx;
            int16_t bh = origin_y + ((gy + gh) * span_y) / SUB_N - by;

            uint8_t q[4];
            int total_cov = sample_rect_quadrants(geom, bx, by, bw, bh, q);
            bool near_hand = is_near_hands(geom, bx + bw / 2, by + bh / 2);

            if (total_cov >= 45 || (near_hand && total_cov >= 15)) {
                uint32_t h = fast_hash(gx, gy, minute_salt + 313);
                int g_idx;
                if (total_cov >= 45) {
                    int cand = s_dense_alnum[(h >> 5) % s_num_dense_alnum];
                    g_idx = glyph_fits_cleanly(geom, cand, bx, by, bw, bh) ? cand : -1;
                } else {
                    g_idx = match_contour_glyph(geom, q, total_cov, bx, by, bw, bh, false, false, h);
                    if (g_idx < 0 && near_hand && s_settings.punct_ratio > 0) {
                        g_idx = match_contour_glyph(geom, q, total_cov, bx, by, bw, bh, true, total_cov <= 15, h);
                    }
                }
                if (g_idx >= 0) {
                    draw_glyph(ctx, g_idx, bx, by, bw, bh, false, get_garbage_color(h, fg_color));
                    subgrid_occupy(occ, gx, gy, gw, gh);
                }
            }
        }
    }
}

// ============================================================================
// MAIN CANVAS UPDATE PROCEDURE
// ============================================================================

static void canvas_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);

    // Fetch fresh local time so ticks and settings reloads are always synchronized
    time_t now = time(NULL);
    struct tm *local_now = localtime(&now);
    if (local_now) {
        s_current_time = *local_now;
    }

    GColor bg_color = s_settings.invert_colors ? GColorWhite : GColorBlack;
    GColor fg_color = s_settings.invert_colors ? GColorBlack : GColorWhite;

    graphics_context_set_fill_color(ctx, bg_color);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    int16_t min_dim = (bounds.size.w < bounds.size.h) ? bounds.size.w : bounds.size.h;
    int16_t dial_span = min_dim - 6;

    DialGeometry geom;
    geom.screen_w = bounds.size.w;
    geom.screen_h = bounds.size.h;
    geom.cx = bounds.size.w / 2;
    geom.cy = bounds.size.h / 2;
    geom.is_full_rect = (s_settings.fill_mode == 1);
    geom.r_dial = (dial_span / 2) - 2;
    geom.r_dial_sq = (int32_t)geom.r_dial * (int32_t)geom.r_dial;

    int16_t r_rim_max = geom.r_dial + (dial_span / 22);
    geom.r_rim_max_sq = (int32_t)r_rim_max * (int32_t)r_rim_max;

    int16_t r_arbor = (dial_span * 48) / 1000;
    geom.r_arbor_sq = (int32_t)r_arbor * (int32_t)r_arbor;

    int hour = s_current_time.tm_hour % 12;
    int min = s_current_time.tm_min;

    int32_t m_angle = (min * TRIG_MAX_ANGLE) / 60;
    int32_t h_angle = ((hour * 60 + min) * TRIG_MAX_ANGLE) / 720;

    int16_t h_len, m_len;
    if (geom.is_full_rect) {
        int16_t h_edge = ray_to_rect_distance(geom.cx, geom.cy, bounds.size.w, bounds.size.h, h_angle);
        int16_t m_edge = ray_to_rect_distance(geom.cx, geom.cy, bounds.size.w, bounds.size.h, m_angle);
        h_len = (h_edge * 48) / 100;
        m_len = (m_edge * 95) / 100;
    } else {
        h_len = (geom.r_dial * 48) / 100; // Distinctively short and stubby (48% radius)
        m_len = geom.r_dial + (dial_span / 24); // Full reach (104% radius)
    }
    geom.h_len = h_len;
    geom.m_len = m_len;

    int16_t h_core = (dial_span * 46) / 1000; // ~9.0px half-width (18px channel on Emery)
    int16_t h_near = h_core + (dial_span / 16);
    geom.h_core_w_sq = (int32_t)h_core * (int32_t)h_core;
    geom.h_near_w_sq = (int32_t)h_near * (int32_t)h_near;

    int32_t h_sin = sin_lookup(h_angle);
    int32_t h_cos = cos_lookup(h_angle);
    geom.h_x0 = geom.cx;
    geom.h_y0 = geom.cy;
    int16_t h_x1 = geom.cx + (int16_t)((h_sin * h_len) / TRIG_MAX_RATIO);
    int16_t h_y1 = geom.cy - (int16_t)((h_cos * h_len) / TRIG_MAX_RATIO);
    geom.h_dx = h_x1 - geom.h_x0;
    geom.h_dy = h_y1 - geom.h_y0;
    geom.h_len_sq = (int32_t)geom.h_dx * (int32_t)geom.h_dx +
                    (int32_t)geom.h_dy * (int32_t)geom.h_dy;

    int16_t m_core = (dial_span * 36) / 1000; // ~7.0px half-width (14px channel on Emery)
    int16_t m_near = m_core + (dial_span / 16);
    geom.m_core_w_sq = (int32_t)m_core * (int32_t)m_core;
    geom.m_near_w_sq = (int32_t)m_near * (int32_t)m_near;

    int32_t m_sin = sin_lookup(m_angle);
    int32_t m_cos = cos_lookup(m_angle);
    geom.m_x0 = geom.cx;
    geom.m_y0 = geom.cy;
    int16_t m_x1 = geom.cx + (int16_t)((m_sin * m_len) / TRIG_MAX_RATIO);
    int16_t m_y1 = geom.cy - (int16_t)((m_cos * m_len) / TRIG_MAX_RATIO);
    geom.m_dx = m_x1 - geom.m_x0;
    geom.m_dy = m_y1 - geom.m_y0;
    geom.m_len_sq = (int32_t)geom.m_dx * (int32_t)geom.m_dx +
                    (int32_t)geom.m_dy * (int32_t)geom.m_dy;

    if (s_settings.layout_style == 0) {
        render_semi_grid(layer, ctx, &geom, fg_color);
    } else {
        render_organic_collage(layer, ctx, &geom, fg_color);
    }
}

// ============================================================================
// WINDOW & LIFECYCLE HANDLERS
// ============================================================================

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    s_current_time = *tick_time;
    if (s_canvas_layer) {
        layer_mark_dirty(s_canvas_layer);
    }
}

static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    s_canvas_layer = layer_create(bounds);
    layer_set_update_proc(s_canvas_layer, canvas_update_proc);
    layer_add_child(window_layer, s_canvas_layer);

    time_t now = time(NULL);
    s_current_time = *localtime(&now);
}

static void main_window_unload(Window *window) {
    if (s_canvas_layer) {
        layer_destroy(s_canvas_layer);
        s_canvas_layer = NULL;
    }
}

static void init(void) {
    init_glyph_tables();
    load_settings();

    app_message_register_inbox_received(inbox_received_callback);
    app_message_open(256, 64);

    s_main_window = window_create();
    window_set_background_color(s_main_window, GColorBlack);
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });

    window_stack_push(s_main_window, true);
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
    tick_timer_service_unsubscribe();
    app_message_deregister_callbacks();
    if (s_main_window) {
        window_destroy(s_main_window);
        s_main_window = NULL;
    }
}

int main(void) {
    init();
    app_event_loop();
    deinit();
    return 0;
}
