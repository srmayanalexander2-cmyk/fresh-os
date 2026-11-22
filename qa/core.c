#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "math.h"
#include "calendar.h"
#include "mediafmt.h"
int main(void) {
    int x;
    const char *e;
    assert(evaluate("-(12-7)*3 + 9 / 2", &x, &e) && x == -11);
    assert(!evaluate("2147483647+1", &x, &e));
    assert(!evaluate("5/0", &x, &e));
    assert(!evaluate("2+", &x, &e));
    assert(!evaluate("(2*3", &x, &e));
    assert(!evaluate("999999999999", &x, &e));
    assert(days_in_month(2000, 2) == 29 && days_in_month(2100, 2) == 28 &&
           first_weekday(2026, 11) == 0);
    /* Media: note names, rests, sharps, ranges and malformed input. */
    assert(media_note_freq("A4", 2) == 440);
    assert(media_note_freq("C4", 2) == 262);
    assert(media_note_freq("C#4", 3) == 277);
    assert(media_note_freq("REST", 4) == 0);
    assert(media_note_freq("rest", 4) == 0);
    assert(media_note_freq("H4", 2) == -1);
    assert(media_note_freq("C9", 2) == -1);
    assert(media_note_freq("C", 1) == -1);
    int freqs[16], durs[16];
    assert(media_parse("C4:1 E4:2 REST:1 G4", freqs, durs, 16) == 4);
    assert(freqs[0] == 262 && freqs[1] == 330 && freqs[2] == 0 && freqs[3] == 392);
    assert(durs[0] == 1 && durs[1] == 2 && durs[2] == 1 && durs[3] == 1);
    assert(media_parse("", freqs, durs, 16) == -1);
    assert(media_parse("C4:0", freqs, durs, 16) == -1);
    assert(media_parse("C4:17", freqs, durs, 16) == -1);
    assert(media_parse("Q4:1", freqs, durs, 16) == -1);
    assert(media_parse("C4:1", freqs, durs, 0) == -1);
    {
        static int big_f[MEDIA_MAX_NOTES], big_d[MEDIA_MAX_NOTES];
        for (int i = 0; i < builtin_track_count; i++)
            assert(media_parse(builtin_tracks[i].text, big_f, big_d, MEDIA_MAX_NOTES) > 8);
    }
    /* Images: P1 bitmap, P2 gray scaling, P3 RGB, comments, bad sizes. */
    unsigned char rgb[64];
    int w, h;
    assert(media_parse_image("P1\n2 2\n1 0\n0 1\n", rgb, &w, &h) == 0 && w == 2 && h == 2);
    assert(rgb[0] == 255 && rgb[3] == 0 && rgb[6] == 0 && rgb[9] == 255);
    assert(media_parse_image("P2\n# comment\n2 1\n10\n0 5\n", rgb, &w, &h) == 0);
    assert(rgb[0] == 0 && rgb[3] == 127);
    assert(media_parse_image("P3\n1 1\n255\n255 0 128\n", rgb, &w, &h) == 0);
    assert(rgb[0] == 255 && rgb[1] == 0 && rgb[2] == 128);
    assert(media_parse_image("P6\n1 1\n255\n", rgb, &w, &h) == -1);
    assert(media_parse_image("P1\n200 200\n", rgb, &w, &h) == -1);
    assert(media_parse_image("P1\n2 2\n1 0\n", rgb, &w, &h) == -1);
    assert(media_parse_image("P2\n1 1\n0\n0\n", rgb, &w, &h) == -1);
    puts("PASS calendar, checked integer parser, music and Netpbm parsers");
}
