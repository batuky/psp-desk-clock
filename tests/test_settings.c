#include <string.h>

#include "minitest.h"
#include "settings.h"

static ClockLayout parse(const char *text) { return settings_parse_layout(text, strlen(text)); }

static void every_layout_survives_a_save_and_a_load(void) {
    for (int l = 0; l < CLOCK_LAYOUT_COUNT; l++) {
        char text[SETTINGS_MAX_BYTES];
        size_t len = settings_format_layout((ClockLayout)l, text);
        CHECK(len > 0 && len <= SETTINGS_MAX_BYTES);
        CHECK(settings_parse_layout(text, len) == (ClockLayout)l);
    }
}

static void the_file_names_the_layout_by_its_letter(void) {
    char text[SETTINGS_MAX_BYTES];
    size_t len = settings_format_layout(CLOCK_LAYOUT_B, text);
    CHECK(len == strlen("layout=B\n") && memcmp(text, "layout=B\n", len) == 0);
    len = settings_format_layout(CLOCK_LAYOUT_D, text);
    CHECK(len == strlen("layout=D\n") && memcmp(text, "layout=D\n", len) == 0);
}

static void a_hand_edited_file_without_the_newline_still_reads(void) {
    CHECK(parse("layout=B") == CLOCK_LAYOUT_B);
    CHECK(parse("layout=B\r\n") == CLOCK_LAYOUT_B);
}

static void a_corrupt_or_unknown_value_falls_back_to_the_default(void) {
    CHECK(parse("") == CLOCK_LAYOUT_DEFAULT);
    CHECK(parse("layout=") == CLOCK_LAYOUT_DEFAULT);
    CHECK(parse("layout=X\n") == CLOCK_LAYOUT_DEFAULT);
    CHECK(parse("layout=b\n") == CLOCK_LAYOUT_DEFAULT);
    CHECK(parse("layout=BD\n") == CLOCK_LAYOUT_DEFAULT);
    CHECK(parse("theme=B\n") == CLOCK_LAYOUT_DEFAULT);
    CHECK(parse("garbage \xff\x01") == CLOCK_LAYOUT_DEFAULT);
}

/* The text is not a C string: whatever follows `len` bytes is not read. */
static void only_the_bytes_that_were_read_count(void) {
    CHECK(settings_parse_layout("layout=Bxx", strlen("layout=B")) == CLOCK_LAYOUT_B);
    CHECK(settings_parse_layout("layout=B", strlen("layout=")) == CLOCK_LAYOUT_DEFAULT);
    CHECK(settings_parse_layout("layout=B\0layout=B", 17) == CLOCK_LAYOUT_DEFAULT);
}

int main(void) {
    RUN(every_layout_survives_a_save_and_a_load);
    RUN(the_file_names_the_layout_by_its_letter);
    RUN(a_hand_edited_file_without_the_newline_still_reads);
    RUN(a_corrupt_or_unknown_value_falls_back_to_the_default);
    RUN(only_the_bytes_that_were_read_count);
    return mt_report();
}
