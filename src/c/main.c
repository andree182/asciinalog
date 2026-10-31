#include <pebble.h>

// ============================================================================
// ASCIInalog Watchface
// Multi-scale irregular ASCII collage with negative-space analog hands,
// AAlib glyph-pixel coverage contouring, organic imperfections, and PKJS config.
// ============================================================================

#define SUB_N 48
#define SUB_CELLS (SUB_N * SUB_N)
#define SETTINGS_PERSIST_KEY 101

typedef struct {
    uint8_t min_char_size; // 1..4 (default 2)
    uint8_t max_char_size; // 3..6 (default 5)
    uint8_t big_char_bias; // 0..100 (default 72)
    uint8_t punct_ratio;   // 0..50 (default 12)
    uint8_t imperfection;  // 0..100 (default 35)
    uint8_t show_date;     // 0 or 1 (default 1)
    uint8_t invert_colors; // 0 or 1 (default 0)
} WatchSettings;

static WatchSettings s_settings;
static Window *s_main_window;
static Layer *s_canvas_layer;
static struct tm s_current_time;

// ============================================================================
// 8x8 BOLD RETRO PIXEL FONT (Active 7x7 in rows 0..6, bits 7..1; 1px spacing)
// GF_DENSE glyphs fill all 4 quadrants strongly so interior has no false gaps.
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
    // --- Bold Full-Height Digits & Uppercase (Core Interior Collage Pool) ---
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

    // --- Bold Full-Height & Asymmetric Lowercase Letters ---
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

    // --- Expressive Bold Symbols (Capped by punct_ratio) ---
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

    // --- Micro Punctuation (Strictly Limited) ---
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
    s_settings.min_char_size = 2;
    s_settings.max_char_size = 5;
    s_settings.big_char_bias = 72;
    s_settings.punct_ratio   = 12;
    s_settings.imperfection  = 35;
    s_settings.show_date     = 1;
    s_settings.invert_colors = 0;
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
    if ((t = dict_find(iter, MESSAGE_KEY_MIN_CHAR_SIZE))) s_settings.min_char_size = (uint8_t)tuple_to_int(t);
    if ((t = dict_find(iter, MESSAGE_KEY_MAX_CHAR_SIZE))) s_settings.max_char_size = (uint8_t)tuple_to_int(t);
    if ((t = dict_find(iter, MESSAGE_KEY_BIG_CHAR_BIAS))) s_settings.big_char_bias = (uint8_t)tuple_to_int(t);
    if ((t = dict_find(iter, MESSAGE_KEY_PUNCT_RATIO)))   s_settings.punct_ratio   = (uint8_t)tuple_to_int(t);
    if ((t = dict_find(iter, MESSAGE_KEY_IMPERFECTION)))  s_settings.imperfection  = (uint8_t)tuple_to_int(t);
    if ((t = dict_find(iter, MESSAGE_KEY_SHOW_DATE)))     s_settings.show_date     = (uint8_t)(tuple_to_int(t) ? 1 : 0);
    if ((t = dict_find(iter, MESSAGE_KEY_INVERT_COLORS))) s_settings.invert_colors = (uint8_t)(tuple_to_int(t) ? 1 : 0);

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
    int16_t r_dial;
    int32_t r_dial_sq;
    int32_t r_rim_max_sq;
    int32_t r_arbor_sq;

    // Hour hand capsule
    int16_t h_x0, h_y0, h_dx, h_dy;
    int32_t h_len_sq;
    int32_t h_core_w_sq;

    // Minute hand capsule
    int16_t m_x0, m_y0, m_dx, m_dy;
    int32_t m_len_sq;
    int32_t m_core_w_sq;
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

static bool is_foreground_point(const DialGeometry *g, int16_t px, int16_t py) {
    int32_t dx = px - g->cx;
    int32_t dy = py - g->cy;
    if ((dx * dx + dy * dy) > g->r_dial_sq) {
        return false;
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

// Check every active bit of glyph_idx so ZERO pixels land inside the clock hands
// and at most ~20% protrude organically outside the dial circle.
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

                // Test center and corners on large glyphs so big blocks never clip into hands
                if (is_in_hand_core(g, px_mid, py_mid)) {
                    return false;
                }
                if (bw >= 20 && (is_in_hand_core(g, px0, py0) || is_in_hand_core(g, px1 - 1, py1 - 1))) {
                    return false;
                }

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
    if (total_bits == 0) return false;
    return (outside_dial_bits * 9) <= (total_bits * 2);
}

// ============================================================================
// HASHING, AALIB MATCHING & SUB-GRID OCCUPANCY
// ============================================================================

static uint32_t fast_hash(int a, int b, int salt) {
    uint32_t h = (uint32_t)(a * 374761393U + b * 668265263U + salt * 1442695041U);
    h = (h ^ (h >> 13)) * 1274126177U;
    return h ^ (h >> 16);
}

static bool subgrid_is_free(const uint64_t occ[SUB_N], int gx, int gy, int gw, int gh) {
    if (gx < 0 || gy < 0 || (gx + gw) > SUB_N || (gy + gh) > SUB_N) {
        return false;
    }
    uint64_t mask = (((uint64_t)1 << gw) - 1ULL) << gx;
    for (int y = gy; y < gy + gh; y++) {
        if (occ[y] & mask) return false;
    }
    return true;
}

static void subgrid_occupy(uint64_t occ[SUB_N], int gx, int gy, int gw, int gh) {
    uint64_t mask = (((uint64_t)1 << gw) - 1ULL) << gx;
    for (int y = gy; y < gy + gh; y++) {
        occ[y] |= mask;
    }
}

static int match_contour_glyph(const DialGeometry *geom,
                               const uint8_t q[4], int total_cov,
                               int16_t bx, int16_t by, int16_t bw, int16_t bh,
                               bool allow_punct, bool allow_micro, uint32_t seed) {
    int best_idx = -1;
    int32_t best_err = 0x7FFFFFFF;
    uint32_t jitter_amp = 52U + ((uint32_t)s_settings.imperfection * 2U) / 3U;

    for (int i = 0; i < NUM_GLYPHS; i++) {
        uint8_t flags = s_glyphs[i].flags;
        if ((flags & GF_PUNCT) && !allow_punct) continue;
        if ((flags & GF_MICRO) && !allow_micro) continue;

        int32_t err = 0;
        for (int k = 0; k < 4; k++) {
            int32_t diff = (int32_t)s_glyph_q[i][k] - (int32_t)q[k];
            if (q[k] == 0 && s_glyph_q[i][k] > 1) {
                err += diff * diff * 3;
            } else if (diff > 0) {
                err += diff * diff * 2;
            } else {
                // Favor fuller coverage in foreground quadrants so hand edges look crisp
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

static void draw_glyph(GContext *ctx, int glyph_idx,
                       int16_t x0, int16_t y0, int16_t box_w, int16_t box_h) {
    if (glyph_idx < 0 || glyph_idx >= NUM_GLYPHS) return;
    const uint8_t *rows = s_glyphs[glyph_idx].rows;

    for (int r = 0; r < 7; r++) {
        uint8_t row_bits = rows[r];
        if (!row_bits) continue;

        int16_t py0 = y0 + (r * box_h) / 8;
        int16_t py1 = y0 + ((r + 1) * box_h) / 8;
        int16_t rh = py1 - py0;
        if (rh <= 0) rh = 1;

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

// ============================================================================
// MULTI-SCALE IRREGULAR COLLAGE RENDERER
// ============================================================================

static const char * const s_day_names[7] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

// Base dimensions in 48x48 sub-grid units (1 unit = 4px on Emery 200x228)
// Level:                    0    1(8x12)  2(12x16) 3(16x20) 4(24x28) 5(32x40) 6(40x48)
static const uint8_t LVL_W[7] = { 0,  2,       3,       4,       6,       8,       10 };
static const uint8_t LVL_H[7] = { 0,  3,       4,       5,       7,      10,       12 };

static const uint16_t LVL_STRIDE[7] = { 1, 733, 1229, 977, 1619, 1397, 1511 };

static void canvas_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);

    GColor bg_color = s_settings.invert_colors ? GColorWhite : GColorBlack;
    GColor fg_color = s_settings.invert_colors ? GColorBlack : GColorWhite;

    graphics_context_set_fill_color(ctx, bg_color);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    int16_t min_dim = (bounds.size.w < bounds.size.h) ? bounds.size.w : bounds.size.h;
    int16_t dial_span = min_dim - 6; // 194px on Emery
    int16_t origin_x = (bounds.size.w - dial_span) / 2;
    int16_t origin_y = (bounds.size.h - dial_span) / 2;

    DialGeometry geom;
    geom.screen_w = bounds.size.w;
    geom.screen_h = bounds.size.h;
    geom.cx = bounds.size.w / 2;
    geom.cy = bounds.size.h / 2;
    geom.r_dial = (dial_span / 2) - 2;
    geom.r_dial_sq = (int32_t)geom.r_dial * (int32_t)geom.r_dial;

    int16_t r_rim_max = geom.r_dial + (dial_span / 22);
    geom.r_rim_max_sq = (int32_t)r_rim_max * (int32_t)r_rim_max;

    int16_t r_arbor = (dial_span * 48) / 1000; // ~9px on Emery
    geom.r_arbor_sq = (int32_t)r_arbor * (int32_t)r_arbor;

    int hour = s_current_time.tm_hour % 12;
    int min = s_current_time.tm_min;
    int minute_salt = s_current_time.tm_hour * 60 + min;

    int32_t m_angle = (min * TRIG_MAX_ANGLE) / 60;
    int32_t h_angle = ((hour * 60 + min) * TRIG_MAX_ANGLE) / 720;

    // Hour hand: distinctly shorter (52% radius) & wider (22px corridor on Emery)
    int16_t h_len  = (geom.r_dial * 52) / 100;
    int16_t h_tail = (geom.r_dial * 9) / 100;
    int16_t h_core = (dial_span * 56) / 1000; // ~11px half-width
    geom.h_core_w_sq = (int32_t)h_core * (int32_t)h_core;

    int32_t h_sin = sin_lookup(h_angle);
    int32_t h_cos = cos_lookup(h_angle);
    geom.h_x0 = geom.cx - (int16_t)((h_sin * h_tail) / TRIG_MAX_RATIO);
    geom.h_y0 = geom.cy + (int16_t)((h_cos * h_tail) / TRIG_MAX_RATIO);
    int16_t h_x1 = geom.cx + (int16_t)((h_sin * h_len) / TRIG_MAX_RATIO);
    int16_t h_y1 = geom.cy - (int16_t)((h_cos * h_len) / TRIG_MAX_RATIO);
    geom.h_dx = h_x1 - geom.h_x0;
    geom.h_dy = h_y1 - geom.h_y0;
    geom.h_len_sq = (int32_t)geom.h_dx * (int32_t)geom.h_dx +
                    (int32_t)geom.h_dy * (int32_t)geom.h_dy;

    // Minute hand: slices all the way through the outer rim (~104% radius, 15px corridor)
    int16_t m_len  = geom.r_dial + (dial_span / 24);
    int16_t m_tail = (geom.r_dial * 10) / 100;
    int16_t m_core = (dial_span * 39) / 1000; // ~7.5px half-width
    geom.m_core_w_sq = (int32_t)m_core * (int32_t)m_core;

    int32_t m_sin = sin_lookup(m_angle);
    int32_t m_cos = cos_lookup(m_angle);
    geom.m_x0 = geom.cx - (int16_t)((m_sin * m_tail) / TRIG_MAX_RATIO);
    geom.m_y0 = geom.cy + (int16_t)((m_cos * m_tail) / TRIG_MAX_RATIO);
    int16_t m_x1 = geom.cx + (int16_t)((m_sin * m_len) / TRIG_MAX_RATIO);
    int16_t m_y1 = geom.cy - (int16_t)((m_cos * m_len) / TRIG_MAX_RATIO);
    geom.m_dx = m_x1 - geom.m_x0;
    geom.m_dy = m_y1 - geom.m_y0;
    geom.m_len_sq = (int32_t)geom.m_dx * (int32_t)geom.m_dx +
                    (int32_t)geom.m_dy * (int32_t)geom.m_dy;

    uint64_t occ[SUB_N];
    for (int y = 0; y < SUB_N; y++) {
        occ[y] = 0ULL;
    }

    graphics_context_set_fill_color(ctx, fg_color);

    // ------------------------------------------------------------------------
    // OPTIONAL DAY / DATE WORD (e.g. "SAT-31") PLACED IN BEST UNOBSTRUCTED BAND
    // ------------------------------------------------------------------------
    if (s_settings.show_date) {
        char date_word[10];
        int wday = (s_current_time.tm_wday >= 0 && s_current_time.tm_wday < 7)
                   ? s_current_time.tm_wday : 0;
        snprintf(date_word, sizeof(date_word), "%s-%02d", s_day_names[wday], s_current_time.tm_mday);

        int dw = (s_settings.max_char_size >= 4) ? 6 : 5;
        int dh = (s_settings.max_char_size >= 4) ? 8 : 6;
        int word_len = 6;
        int start_gx = (SUB_N - word_len * dw) / 2;

        static const int cand_rows[4] = { 30, 11, 34, 7 };
        int best_gy = 30;
        int best_fit = -1;
        for (int c = 0; c < 4; c++) {
            int gy_try = cand_rows[c];
            int fits = 0;
            for (int i = 0; i < word_len; i++) {
                int g_idx = find_glyph_index(date_word[i]);
                int gx = start_gx + i * dw;
                int16_t bx = origin_x + (gx * dial_span) / SUB_N;
                int16_t bw = origin_x + ((gx + dw) * dial_span) / SUB_N - bx;
                int16_t by = origin_y + (gy_try * dial_span) / SUB_N;
                int16_t bh = origin_y + ((gy_try + dh) * dial_span) / SUB_N - by;
                if (glyph_fits_cleanly(&geom, g_idx, bx, by, bw, bh)) {
                    fits++;
                }
            }
            if (fits > best_fit) {
                best_fit = fits;
                best_gy = gy_try;
                if (fits == word_len) break;
            }
        }

        for (int i = 0; i < word_len && date_word[i] != '\0'; i++) {
            int g_idx = find_glyph_index(date_word[i]);
            int gx = start_gx + i * dw;
            int gy_jitter = (s_settings.imperfection >= 25)
                            ? ((int)(fast_hash(i, minute_salt, 7) % 3) - 1) : 0;
            int gy = best_gy + gy_jitter;

            int16_t bx = origin_x + (gx * dial_span) / SUB_N;
            int16_t by = origin_y + (gy * dial_span) / SUB_N;
            int16_t bw = origin_x + ((gx + dw) * dial_span) / SUB_N - bx;
            int16_t bh = origin_y + ((gy + dh) * dial_span) / SUB_N - by;

            if (subgrid_is_free(occ, gx, gy, dw, dh) &&
                glyph_fits_cleanly(&geom, g_idx, bx, by, bw, bh)) {
                draw_glyph(ctx, g_idx, bx, by, bw, bh);
                subgrid_occupy(occ, gx, gy, dw, dh);
            }
        }
    }

    // ------------------------------------------------------------------------
    // PASS 1: IRREGULAR MULTI-SCALE PACKING (MAX_CHAR_SIZE DOWN TO MIN_CHAR_SIZE)
    // Large tiers (lvl >= 4) use dense full-quadrant characters so hands aren't
    // distorted by giant half-empty letters; lvl <= 3 contours hands & edges.
    // ------------------------------------------------------------------------
    int min_lvl = s_settings.min_char_size;
    int max_lvl = s_settings.max_char_size;
    int max_px_jitter = (s_settings.imperfection >= 40) ? 2 :
                        ((s_settings.imperfection >= 15) ? 1 : 0);

    for (int lvl = max_lvl; lvl >= min_lvl; lvl--) {
        int base_gw = LVL_W[lvl];
        int base_gh = LVL_H[lvl];
        uint16_t stride = LVL_STRIDE[lvl];

        int accept_pct = 100;
        if (lvl > min_lvl) {
            int tier_rank = lvl - min_lvl;
            int span = (max_lvl > min_lvl) ? (max_lvl - min_lvl) : 1;
            int top_cap = 15 + (s_settings.big_char_bias * 75) / 100;
            accept_pct = top_cap + ((span - tier_rank) * (95 - top_cap)) / span;
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

            if (!subgrid_is_free(occ, gx, gy, gw, gh)) {
                continue;
            }
            if (lvl > min_lvl && (int)((h >> 4) % 100) >= accept_pct) {
                continue;
            }

            int16_t bx = origin_x + (gx * dial_span) / SUB_N;
            int16_t by = origin_y + (gy * dial_span) / SUB_N;
            int16_t bw = origin_x + ((gx + gw) * dial_span) / SUB_N - bx;
            int16_t bh = origin_y + ((gy + gh) * dial_span) / SUB_N - by;

            if (max_px_jitter > 0) {
                bx += (int16_t)((int)((h >> 11) % (2 * max_px_jitter + 1)) - max_px_jitter);
                by += (int16_t)((int)((h >> 15) % (2 * max_px_jitter + 1)) - max_px_jitter);
            }

            uint8_t q[4];
            int total_cov = sample_rect_quadrants(&geom, bx, by, bw, bh, q);
            if (total_cov == 0) {
                continue;
            }

            // Giant/Large tiers (lvl >= 4) require solid interior foreground (total_cov >= 45)
            // so huge half-empty glyphs don't create false black blobs next to the hands.
            int min_cov = (lvl >= 4) ? 45 : ((lvl == 3) ? 21 : 12);
            if (total_cov < min_cov) {
                continue;
            }

            // Imperfection on outer rim crevices (keep hand borders fuller for readability)
            if (lvl == min_lvl && total_cov < 30) {
                int16_t mid_x = bx + bw / 2 - geom.cx;
                int16_t mid_y = by + bh / 2 - geom.cy;
                bool near_outer_rim = (mid_x * mid_x + mid_y * mid_y) > ((geom.r_dial_sq * 7) / 10);
                if (near_outer_rim) {
                    int skip_chance = (total_cov <= 18) ? (int)s_settings.imperfection
                                                        : ((int)s_settings.imperfection / 2);
                    if ((int)((h >> 19) % 100) < skip_chance) {
                        continue;
                    }
                }
            }

            bool allow_punct = ((int)((h >> 8) % 100) < (int)s_settings.punct_ratio);
            bool allow_micro = allow_punct && (lvl <= 2) && (total_cov <= 18) && ((h & 0x3) == 0);

            int g_idx = -1;
            if (total_cov >= 45) {
                int cand;
                if (allow_punct && s_num_dense_punct > 0) {
                    cand = s_dense_punct[(h >> 12) % s_num_dense_punct];
                } else {
                    cand = s_dense_alnum[(h >> 12) % s_num_dense_alnum];
                }
                if (glyph_fits_cleanly(&geom, cand, bx, by, bw, bh)) {
                    g_idx = cand;
                }
            } else {
                g_idx = match_contour_glyph(&geom, q, total_cov,
                                            bx, by, bw, bh,
                                            allow_punct, allow_micro, h);
            }

            if (g_idx >= 0) {
                draw_glyph(ctx, g_idx, bx, by, bw, bh);
                subgrid_occupy(occ, gx, gy, gw, gh);
            }
        }
    }

    // ------------------------------------------------------------------------
    // PASS 2: INTERIOR-ONLY ALPHANUMERIC GAP SEALER (When min_lvl >= 2)
    // Fills narrow 8x12 gaps between giant letters INSIDE the white disc with
    // small letters/digits (NO punctuation!) so random interior holes never
    // compete visually with the negative-space clock hands.
    // ------------------------------------------------------------------------
    if (min_lvl >= 2) {
        int gw = 2, gh = 3;
        int idx = (int)(fast_hash(99, minute_salt, 71) % SUB_CELLS);
        for (int step = 0; step < SUB_CELLS; step++) {
            int gx = idx % SUB_N;
            int gy = idx / SUB_N;
            idx = (idx + 733) % SUB_CELLS;

            if (!subgrid_is_free(occ, gx, gy, gw, gh)) {
                continue;
            }
            int16_t bx = origin_x + (gx * dial_span) / SUB_N;
            int16_t by = origin_y + (gy * dial_span) / SUB_N;
            int16_t bw = origin_x + ((gx + gw) * dial_span) / SUB_N - bx;
            int16_t bh = origin_y + ((gy + gh) * dial_span) / SUB_N - by;

            uint8_t q[4];
            int total_cov = sample_rect_quadrants(&geom, bx, by, bw, bh, q);
            // Strictly interior or direct hand-flank gaps (not outer rim noise)
            int16_t mid_x = bx + bw / 2 - geom.cx;
            int16_t mid_y = by + bh / 2 - geom.cy;
            bool inside_inner_disc = (mid_x * mid_x + mid_y * mid_y) < ((geom.r_dial_sq * 72) / 100);

            if (total_cov >= 45 || (inside_inner_disc && total_cov >= 24)) {
                uint32_t h = fast_hash(gx, gy, minute_salt + 313);
                int g_idx;
                if (total_cov >= 45) {
                    int cand = s_dense_alnum[(h >> 5) % s_num_dense_alnum];
                    g_idx = glyph_fits_cleanly(&geom, cand, bx, by, bw, bh) ? cand : -1;
                } else {
                    // Pure alphanumeric contour right beside the hand
                    g_idx = match_contour_glyph(&geom, q, total_cov, bx, by, bw, bh, false, false, h);
                }
                if (g_idx >= 0) {
                    draw_glyph(ctx, g_idx, bx, by, bw, bh);
                    subgrid_occupy(occ, gx, gy, gw, gh);
                }
            }
        }
    }
}

// ============================================================================
// WINDOW, TIME TICK & APP LIFECYCLE
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
