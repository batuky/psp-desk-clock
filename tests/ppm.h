/* Golden-image helpers: binary PPM (P6) writer and exact comparer. */
#ifndef PPM_H
#define PPM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "framebuffer.h"

#define PPM_HEADER_FMT "P6\n%d %d\n255\n"

/* Packs the Framebuffer into PPM bytes. Caller frees. *len gets the size. */
static unsigned char *ppm_encode(const Framebuffer *fb, size_t *len) {
    char header[32];
    int hl = snprintf(header, sizeof header, PPM_HEADER_FMT, FB_WIDTH, FB_HEIGHT);
    size_t n = (size_t)hl + (size_t)FB_WIDTH * FB_HEIGHT * 3;
    unsigned char *out = malloc(n), *p;
    if (!out) return NULL;
    memcpy(out, header, (size_t)hl);
    p = out + hl;
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) {
        Pixel px = fb->pixels[i];
        *p++ = (unsigned char)(px & 0xFF);         /* R */
        *p++ = (unsigned char)((px >> 8) & 0xFF);  /* G */
        *p++ = (unsigned char)((px >> 16) & 0xFF); /* B */
    }
    *len = n;
    return out;
}

static int ppm_write(const char *path, const Framebuffer *fb) {
    size_t n;
    unsigned char *bytes = ppm_encode(fb, &n);
    FILE *f = bytes ? fopen(path, "wb") : NULL;
    int ok = f && fwrite(bytes, 1, n, f) == n;
    if (f) fclose(f);
    free(bytes);
    return ok;
}

/* 1 when the file at path is byte-for-byte the PPM of fb, else 0. */
static int ppm_matches(const char *path, const Framebuffer *fb) {
    size_t n, got;
    unsigned char *want = ppm_encode(fb, &n), *have;
    FILE *f = fopen(path, "rb");
    int same;
    if (!want || !f) {
        if (f) fclose(f);
        free(want);
        return 0;
    }
    have = malloc(n + 1);
    got = have ? fread(have, 1, n + 1, f) : 0;
    fclose(f);
    same = have && got == n && memcmp(have, want, n) == 0;
    free(have);
    free(want);
    return same;
}

#endif
