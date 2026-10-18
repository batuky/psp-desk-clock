#include "bitmap_font.h"

#include <stddef.h>

#define UNKNOWN_CODE 0xFFFDu

/* Each glyph is drawn as 8 rows of 5 characters: '#' is a lit pixel, '.' is
 * dark. Letters sit on rows 0-6; row 7 is used by the cedilla of Ç and ş.
 * Characters are listed by Unicode code point: Ç is U+00C7, ı is U+0131 and
 * ş is U+015F. */
typedef struct {
    unsigned code;
    const char *rows[FONT_GLYPH_H];
} Glyph;

static const Glyph GLYPHS[] = {
    {' ',    {".....", ".....", ".....", ".....", ".....", ".....", ".....", "....."}},
    {'.',    {".....", ".....", ".....", ".....", ".....", ".##..", ".##..", "....."}},
    {'-',    {".....", ".....", ".....", ".###.", ".....", ".....", ".....", "....."}},
    {'0',    {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###.", "....."}},
    {'1',    {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###.", "....."}},
    {'2',    {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####", "....."}},
    {'3',    {".###.", "#...#", "....#", "..##.", "....#", "#...#", ".###.", "....."}},
    {'4',    {"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#.", "....."}},
    {'5',    {"#####", "#....", "####.", "....#", "....#", "#...#", ".###.", "....."}},
    {'6',    {"..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###.", "....."}},
    {'7',    {"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#...", "....."}},
    {'8',    {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###.", "....."}},
    {'9',    {".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##..", "....."}},
    {'P',    {"####.", "#...#", "#...#", "####.", "#....", "#....", "#....", "....."}},
    {'S',    {".####", "#....", "#....", ".###.", "....#", "....#", "####.", "....."}},
    {'C',    {".###.", "#...#", "#....", "#....", "#....", "#...#", ".###.", "....."}},
    {0x00C7, {".###.", "#...#", "#....", "#....", "#....", "#...#", ".###.", "..#.."}}, /* Ç */
    {'a',    {".....", ".....", ".###.", "....#", ".####", "#...#", ".####", "....."}},
    {'b',    {"#....", "#....", "####.", "#...#", "#...#", "#...#", "####.", "....."}},
    {'e',    {".....", ".....", ".###.", "#...#", "#####", "#....", ".###.", "....."}},
    {'i',    {"..#..", ".....", ".##..", "..#..", "..#..", "..#..", ".###.", "....."}},
    {0x0131, {".....", ".....", ".##..", "..#..", "..#..", "..#..", ".###.", "....."}}, /* ı */
    {'l',    {".##..", "..#..", "..#..", "..#..", "..#..", "..#..", ".###.", "....."}},
    {'m',    {".....", ".....", "##.#.", "#.#.#", "#.#.#", "#...#", "#...#", "....."}},
    {'r',    {".....", ".....", "#.##.", "##..#", "#....", "#....", "#....", "....."}},
    {'s',    {".....", ".....", ".####", "#....", ".###.", "....#", "####.", "....."}},
    {0x015F, {".....", ".....", ".####", "#....", ".###.", "....#", "####.", "..#.."}}, /* ş */
    {'t',    {".#...", ".#...", "####.", ".#...", ".#...", ".#..#", "..##.", "....."}},
    {'u',    {".....", ".....", "#...#", "#...#", "#...#", "#..##", ".##.#", "....."}},
    {'z',    {".....", ".....", "#####", "...#.", "..#..", ".#...", "#####", "....."}},
};
#define GLYPH_COUNT ((int)(sizeof GLYPHS / sizeof GLYPHS[0]))

/* Drawn for any character the font does not have. */
static const char *const MISSING_BOX[FONT_GLYPH_H] = {
    "#####", "#...#", "#...#", "#...#", "#...#", "#...#", "#####", ".....",
};

static const Glyph *find_glyph(unsigned code) {
    for (int i = 0; i < GLYPH_COUNT; i++)
        if (GLYPHS[i].code == code) return &GLYPHS[i];
    return NULL;
}

/* Reads one UTF-8 character and moves `*cursor` past it. Anything that is
 * not valid UTF-8 comes back as UNKNOWN_CODE, one byte at a time, and the
 * cursor never moves past the end of the string. */
static unsigned next_code(const unsigned char **cursor) {
    const unsigned char *s = *cursor;
    unsigned first = s[0];
    int length;
    unsigned code;

    if (first < 0x80) {
        *cursor = s + 1;
        return first;
    }
    length = (first >= 0xC2 && first <= 0xDF) ? 2 : (first >= 0xE0 && first <= 0xEF) ? 3
           : (first >= 0xF0 && first <= 0xF4) ? 4 : 0;
    *cursor = s + 1;
    if (length == 0) return UNKNOWN_CODE;
    code = first & (0xFFu >> (length + 1));
    for (int i = 1; i < length; i++) {
        if ((s[i] & 0xC0) != 0x80) return UNKNOWN_CODE;
        code = (code << 6) | (s[i] & 0x3Fu);
    }
    /* Reject what UTF-8 forbids: a value written with more bytes than it
     * needs (overlong), the surrogate range, and anything past U+10FFFF. */
    if ((length == 3 && code < 0x800u) || (length == 4 && code < 0x10000u) ||
        (code >= 0xD800u && code <= 0xDFFFu) || code > 0x10FFFFu)
        return UNKNOWN_CODE;
    *cursor = s + length;
    return code;
}

int bitmap_font_draw_text(Framebuffer *fb, int x, int y, int scale, const char *utf8,
                          Pixel color) {
    const unsigned char *cursor = (const unsigned char *)utf8;
    if (scale < 1) scale = 1;

    while (*cursor) {
        const Glyph *glyph = find_glyph(next_code(&cursor));
        const char *const *rows = glyph ? glyph->rows : MISSING_BOX;
        for (int r = 0; r < FONT_GLYPH_H; r++)
            for (int c = 0; c < FONT_GLYPH_W; c++)
                if (rows[r][c] == '#')
                    fb_fill_rect(fb, x + c * scale, y + r * scale, scale, scale, color);
        x += FONT_ADVANCE * scale;
    }
    return x;
}

int bitmap_font_text_width(const char *utf8, int scale) {
    const unsigned char *cursor = (const unsigned char *)utf8;
    int chars = 0;
    if (scale < 1) scale = 1;

    while (*cursor) {
        next_code(&cursor);
        chars++;
    }
    return chars == 0 ? 0 : chars * FONT_ADVANCE * scale - scale;
}

int bitmap_font_can_draw(const char *utf8) {
    const unsigned char *cursor = (const unsigned char *)utf8;
    while (*cursor)
        if (!find_glyph(next_code(&cursor))) return 0;
    return 1;
}
