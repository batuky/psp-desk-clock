#include <stdlib.h>
#include <string.h>

#include "bitmap_font.h"
#include "minitest.h"
#include "ppm.h"

static Framebuffer fb;

static Pixel at(int x, int y) { return fb.pixels[y * FB_WIDTH + x]; }

static int lit_pixels(void) {
    int n = 0;
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) n += fb.pixels[i] == PIXEL_WHITE;
    return n;
}

#define CCEDILLA "\xC3\x87"
#define SCEDILLA "\xC5\x9F"
#define DOTLESS_I "\xC4\xB1"

/* Everything the clock needs to show: the characters of the Date line and,
 * later, the calendar header. One entry per character. */
static const char *const SUPPORTED[] = {
    " ", ".", "-", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "P", "S", "C", CCEDILLA,
    "a", "b", "e", "i", DOTLESS_I, "l", "m", "r", "s", SCEDILLA, "t", "u", "z",
};
#define SUPPORTED_COUNT ((int)(sizeof SUPPORTED / sizeof SUPPORTED[0]))

/* One character drawn at scale 1 into a 5x8 picture: 40 characters, '#' lit
 * and '.' dark, row by row. */
static void picture(const char *ch, char out[FONT_GLYPH_W * FONT_GLYPH_H + 1]) {
    fb_clear(&fb, PIXEL_BLACK);
    bitmap_font_draw_text(&fb, 10, 20, 1, ch, PIXEL_WHITE);
    for (int r = 0; r < FONT_GLYPH_H; r++)
        for (int c = 0; c < FONT_GLYPH_W; c++)
            out[r * FONT_GLYPH_W + c] = at(10 + c, 20 + r) == PIXEL_WHITE ? '#' : '.';
    out[FONT_GLYPH_W * FONT_GLYPH_H] = '\0';
}

static int rows_equal(const char *a, const char *b, int first_row, int last_row) {
    return memcmp(a + first_row * FONT_GLYPH_W, b + first_row * FONT_GLYPH_W,
                  (size_t)(last_row - first_row + 1) * FONT_GLYPH_W) == 0;
}

static int row_is_blank(const char *pic, int row) {
    for (int c = 0; c < FONT_GLYPH_W; c++)
        if (pic[row * FONT_GLYPH_W + c] == '#') return 0;
    return 1;
}

static void drawing_text_lights_pixels_and_returns_the_next_position(void) {
    fb_clear(&fb, PIXEL_BLACK);
    int next = bitmap_font_draw_text(&fb, 10, 20, 1, "8", PIXEL_WHITE);
    CHECK(lit_pixels() > 0);
    CHECK(next == 10 + FONT_ADVANCE);
}

static void every_character_advances_one_cell_whatever_its_bytes(void) {
    fb_clear(&fb, PIXEL_BLACK);
    CHECK(bitmap_font_draw_text(&fb, 0, 0, 1, "Cumartesi", PIXEL_WHITE) == 9 * FONT_ADVANCE);
    CHECK(bitmap_font_draw_text(&fb, 0, 0, 2, "Cumartesi", PIXEL_WHITE) == 9 * FONT_ADVANCE * 2);
    CHECK(bitmap_font_draw_text(&fb, 5, 0, 3, CCEDILLA "ar" SCEDILLA "amba", PIXEL_WHITE) ==
          5 + 8 * FONT_ADVANCE * 3); /* 8 characters, 10 bytes */
    CHECK(bitmap_font_draw_text(&fb, 7, 0, 1, "", PIXEL_WHITE) == 7);
}

static void text_width_is_the_ink_the_drawing_covers(void) {
    /* One glyph is FONT_GLYPH_W wide; each further character adds FONT_ADVANCE. */
    CHECK(bitmap_font_text_width("", 3) == 0);
    CHECK(bitmap_font_text_width("8", 1) == FONT_GLYPH_W);
    CHECK(bitmap_font_text_width("Cumartesi", 3) == 9 * FONT_ADVANCE * 3 - 3);
    CHECK(bitmap_font_text_width(CCEDILLA "ar" SCEDILLA "amba", 1) == 8 * FONT_ADVANCE - 1);
    /* It counts characters the way drawing does, so a stray byte is one box. */
    CHECK(bitmap_font_text_width("1\xFF" "2", 1) == 3 * FONT_ADVANCE - 1);
}

static void scaling_turns_every_font_pixel_into_a_solid_block(void) {
    static Framebuffer small;
    fb_clear(&fb, PIXEL_BLACK);
    bitmap_font_draw_text(&fb, 4, 4, 1, "8" CCEDILLA, PIXEL_WHITE);
    memcpy(&small, &fb, sizeof small);
    int small_lit = lit_pixels();

    fb_clear(&fb, PIXEL_BLACK);
    bitmap_font_draw_text(&fb, 4, 4, 3, "8" CCEDILLA, PIXEL_WHITE);
    CHECK(lit_pixels() == small_lit * 9);
    /* Font pixel (c, r) at scale 1 is the 3x3 block at (4 + 3c, 4 + 3r). */
    int mismatches = 0;
    for (int r = 0; r < FONT_GLYPH_H; r++)
        for (int c = 0; c < 2 * FONT_ADVANCE; c++) {
            Pixel want = small.pixels[(4 + r) * FB_WIDTH + 4 + c];
            for (int dy = 0; dy < 3; dy++)
                for (int dx = 0; dx < 3; dx++)
                    mismatches += at(4 + 3 * c + dx, 4 + 3 * r + dy) != want;
        }
    CHECK(mismatches == 0);
}

static void every_supported_glyph_is_drawn_and_all_are_different(void) {
    static char pics[SUPPORTED_COUNT][FONT_GLYPH_W * FONT_GLYPH_H + 1];
    for (int i = 0; i < SUPPORTED_COUNT; i++) {
        picture(SUPPORTED[i], pics[i]);
        CHECK(bitmap_font_can_draw(SUPPORTED[i]));
        if (strcmp(SUPPORTED[i], " ") == 0)
            CHECK(strchr(pics[i], '#') == NULL); /* the space is blank */
        else
            CHECK(strchr(pics[i], '#') != NULL);
    }
    for (int i = 0; i < SUPPORTED_COUNT; i++)
        for (int j = i + 1; j < SUPPORTED_COUNT; j++) CHECK(strcmp(pics[i], pics[j]) != 0);
}

/* The Turkish letters are the plain letter plus a mark: same shape, only the
 * mark differs. */
static void turkish_letters_are_their_plain_letter_plus_a_mark(void) {
    char c[FONT_GLYPH_W * FONT_GLYPH_H + 1], cc[sizeof c], s[sizeof c], sc[sizeof c];
    char i[sizeof c], dotless[sizeof c];
    picture("C", c);
    picture(CCEDILLA, cc);
    picture("s", s);
    picture(SCEDILLA, sc);
    picture("i", i);
    picture(DOTLESS_I, dotless);

    /* Cedilla: the letter sits on rows 0-6 unchanged, row 7 gains the hook. */
    CHECK(rows_equal(c, cc, 0, 6));
    CHECK(row_is_blank(c, 7) && !row_is_blank(cc, 7));
    CHECK(rows_equal(s, sc, 0, 6));
    CHECK(row_is_blank(s, 7) && !row_is_blank(sc, 7));
    /* Dotless i is i without its dot: the stem is unchanged, the dot row is empty. */
    CHECK(rows_equal(i, dotless, 2, 7));
    CHECK(!row_is_blank(i, 0) && row_is_blank(dotless, 0));
}

static void text_is_clipped_at_the_screen_edges(void) {
    /* Canary pixels on both sides of the buffer must stay untouched. */
    static struct {
        Pixel before[FB_WIDTH * 20];
        Framebuffer buf;
        Pixel after[FB_WIDTH * 20];
    } guarded;
    const Pixel canary = 0xDEADBEEFu;
    for (int i = 0; i < FB_WIDTH * 20; i++) guarded.before[i] = guarded.after[i] = canary;
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) guarded.buf.pixels[i] = PIXEL_BLACK;

    bitmap_font_draw_text(&guarded.buf, -3, 5, 1, "8", PIXEL_WHITE);                /* left */
    bitmap_font_draw_text(&guarded.buf, FB_WIDTH - 2, 5, 3, "8", PIXEL_WHITE);      /* right */
    bitmap_font_draw_text(&guarded.buf, 40, -5, 2, "Cumartesi", PIXEL_WHITE);       /* top */
    bitmap_font_draw_text(&guarded.buf, 40, FB_HEIGHT - 3, 2, "Cumartesi", PIXEL_WHITE); /* bottom */
    bitmap_font_draw_text(&guarded.buf, -1000, -1000, 2, "Cumartesi", PIXEL_WHITE); /* far away */
    bitmap_font_draw_text(&guarded.buf, 5000, 5000, 2, "Cumartesi", PIXEL_WHITE);

    int canaries_intact = 1;
    for (int i = 0; i < FB_WIDTH * 20; i++)
        if (guarded.before[i] != canary || guarded.after[i] != canary) canaries_intact = 0;
    CHECK(canaries_intact);
    /* The part that is inside the screen is still drawn: the right edge of '8'. */
    int lit_at_left_edge = 0;
    for (int y = 0; y < FB_HEIGHT; y++) lit_at_left_edge += guarded.buf.pixels[y * FB_WIDTH] == PIXEL_WHITE;
    CHECK(lit_at_left_edge > 0);
}

static void a_character_without_a_glyph_is_drawn_as_a_box(void) {
    char unknown[FONT_GLYPH_W * FONT_GLYPH_H + 1];
    char known[sizeof unknown];
    picture("@", unknown);
    CHECK(!bitmap_font_can_draw("@"));
    CHECK(strchr(unknown, '#') != NULL); /* visible, not blank */
    for (int i = 0; i < SUPPORTED_COUNT; i++) {
        picture(SUPPORTED[i], known);
        CHECK(strcmp(unknown, known) != 0); /* and not mistaken for a real glyph */
    }
    /* Damaged UTF-8 is one box per bad byte and never reads past the end. */
    fb_clear(&fb, PIXEL_BLACK);
    CHECK(bitmap_font_draw_text(&fb, 0, 0, 1, "\x80", PIXEL_WHITE) == FONT_ADVANCE);
    CHECK(bitmap_font_draw_text(&fb, 0, 0, 1, "\xC3", PIXEL_WHITE) == FONT_ADVANCE);
    CHECK(!bitmap_font_can_draw("\xC3"));
}

/* Encodings UTF-8 forbids must never be taken for a real character: an
 * overlong form of a space, a surrogate, a value past U+10FFFF. Each bad
 * byte is drawn as its own box. */
static void forbidden_utf8_is_never_taken_for_a_real_character(void) {
    static const char *const FORBIDDEN[] = {
        "\xE0\x81\xA0",     /* overlong 3-byte form of U+0020 */
        "\xF0\x80\x80\xA0", /* overlong 4-byte form of U+0020 */
        "\xC0\xA0",         /* overlong 2-byte form of U+0020 */
        "\xED\xA0\x80",     /* surrogate U+D800 */
        "\xF4\x90\x80\x80", /* U+110000, past the last code point */
    };
    for (int i = 0; i < (int)(sizeof FORBIDDEN / sizeof FORBIDDEN[0]); i++) {
        fb_clear(&fb, PIXEL_BLACK);
        CHECK(!bitmap_font_can_draw(FORBIDDEN[i]));
        CHECK(bitmap_font_draw_text(&fb, 0, 0, 1, FORBIDDEN[i], PIXEL_WHITE) ==
              (int)strlen(FORBIDDEN[i]) * FONT_ADVANCE);
    }
}

static void the_date_line_alphabet_can_be_drawn(void) {
    CHECK(bitmap_font_can_draw("17.10.2026 Cumartesi"));
    CHECK(bitmap_font_can_draw("Pazartesi"));
    CHECK(bitmap_font_can_draw("Sal" DOTLESS_I));
    CHECK(bitmap_font_can_draw(CCEDILLA "ar" SCEDILLA "amba"));
    CHECK(bitmap_font_can_draw("Per" SCEDILLA "embe"));
    CHECK(bitmap_font_can_draw("Cuma"));
    CHECK(bitmap_font_can_draw("Pazar"));
    CHECK(bitmap_font_can_draw("--.--.----"));
    CHECK(bitmap_font_can_draw(""));
}

/* Golden image of the font at two sizes. Written once and approved by eye;
 * regenerate with `make update-golden` and review the image before
 * committing. */
static void the_font_matches_the_golden_image(void) {
    const char *path = "tests/golden/font_sample.ppm";
    fb_clear(&fb, PIXEL_BLACK);
    bitmap_font_draw_text(&fb, 8, 8, 3, "17.10.2026 Cumartesi", PIXEL_WHITE);
    bitmap_font_draw_text(&fb, 8, 44, 3, "Pazartesi Sal" DOTLESS_I, PIXEL_WHITE);
    bitmap_font_draw_text(&fb, 8, 80, 3, CCEDILLA "ar" SCEDILLA "amba Per" SCEDILLA "embe", PIXEL_WHITE);
    bitmap_font_draw_text(&fb, 8, 116, 3, "Cuma Pazar --.--.----", PIXEL_WHITE);
    bitmap_font_draw_text(&fb, 8, 152, 2, "0123456789 . -", PIXEL_WHITE);
    bitmap_font_draw_text(&fb, 8, 176, 2, "PSC" CCEDILLA " abeil" DOTLESS_I "mrs" SCEDILLA "tuz", PIXEL_WHITE);
    bitmap_font_draw_text(&fb, 8, 200, 1, "17.10.2026 Cumartesi @", PIXEL_WHITE);
    if (getenv("UPDATE_GOLDEN")) CHECK(ppm_write(path, &fb));
    CHECK(ppm_matches(path, &fb));
}

int main(void) {
    RUN(drawing_text_lights_pixels_and_returns_the_next_position);
    RUN(every_character_advances_one_cell_whatever_its_bytes);
    RUN(text_width_is_the_ink_the_drawing_covers);
    RUN(scaling_turns_every_font_pixel_into_a_solid_block);
    RUN(every_supported_glyph_is_drawn_and_all_are_different);
    RUN(turkish_letters_are_their_plain_letter_plus_a_mark);
    RUN(text_is_clipped_at_the_screen_edges);
    RUN(a_character_without_a_glyph_is_drawn_as_a_box);
    RUN(forbidden_utf8_is_never_taken_for_a_real_character);
    RUN(the_date_line_alphabet_can_be_drawn);
    RUN(the_font_matches_the_golden_image);
    return mt_report();
}
