#include "settings.h"

#include <string.h>

#define LAYOUT_KEY "layout="

/* The letter each Layout goes by in the file (see GLOSSARY.md). */
static const char LAYOUT_LETTERS[CLOCK_LAYOUT_COUNT] = {
    [CLOCK_LAYOUT_B] = 'B',
    [CLOCK_LAYOUT_D] = 'D',
};

size_t settings_format_layout(ClockLayout layout, char out[SETTINGS_MAX_BYTES]) {
    size_t len = strlen(LAYOUT_KEY);
    if ((unsigned)layout >= CLOCK_LAYOUT_COUNT) layout = CLOCK_LAYOUT_DEFAULT;
    memcpy(out, LAYOUT_KEY, len);
    out[len++] = LAYOUT_LETTERS[layout];
    out[len++] = '\n';
    return len;
}

ClockLayout settings_parse_layout(const char *text, size_t len) {
    size_t key_len = strlen(LAYOUT_KEY);
    /* The letter must be the whole value: only a line end may follow it. */
    if (len < key_len + 1 || memcmp(text, LAYOUT_KEY, key_len) != 0) return CLOCK_LAYOUT_DEFAULT;
    if (len > key_len + 1 && text[key_len + 1] != '\n' && text[key_len + 1] != '\r')
        return CLOCK_LAYOUT_DEFAULT;
    for (int l = 0; l < CLOCK_LAYOUT_COUNT; l++)
        if (text[key_len] == LAYOUT_LETTERS[l]) return (ClockLayout)l;
    return CLOCK_LAYOUT_DEFAULT;
}
