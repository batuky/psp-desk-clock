/* The settings file: what the app remembers between runs, as a few bytes of
 * text. Only the text format lives here; reading and writing the file is the
 * platform's job (ClockPlatform). Today it holds just the chosen Layout. */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stddef.h>

#include "clock_face.h"

/* The most bytes a settings file holds. */
#define SETTINGS_MAX_BYTES 16

/* Writes the settings text for `layout` into `out` and returns its length. */
size_t settings_format_layout(ClockLayout layout, char out[SETTINGS_MAX_BYTES]);

/* The Layout named in `len` bytes of settings text, or CLOCK_LAYOUT_DEFAULT
 * when the text is empty, corrupt or names no Layout. `text` need not end in
 * a zero byte. */
ClockLayout settings_parse_layout(const char *text, size_t len);

#endif
