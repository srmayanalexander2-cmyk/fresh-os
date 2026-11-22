#pragma once
#include "base.h"
/* Text media formats that fit the Home document store (ASCII, <=3072 bytes).
 * Music (.mus): whitespace-separated NOTE:BEATS tokens, e.g. "C4:1 E4:2 REST:1".
 * Images: Netpbm ASCII P1 (bitmap), P2 (gray) and P3 (RGB). */
#define MEDIA_MAX_NOTES 512
#define PHOTO_MAX_W 128
#define PHOTO_MAX_H 128
struct BuiltinTrack {
    const char *name, *text;
};
extern const struct BuiltinTrack builtin_tracks[];
extern const int builtin_track_count;
/* Parse one note name ("C4", "F#3", "REST") of length len. Returns Hz,
 * 0 for a rest, -1 for an invalid name. */
int media_note_freq(const char *s, int len);
/* Parse a whole .mus text. Fills freqs/durs (beats). Returns note count,
 * or -1 when empty, malformed or longer than max. */
int media_parse(const char *text, int *freqs, int *durs, int max);
/* Parse P1/P2/P3 ASCII into rgb (3 bytes per pixel, row-major).
 * Returns 0 on success, -1 on failure. */
int media_parse_image(const char *text, u8 *rgb, int *w, int *h);
