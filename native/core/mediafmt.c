#include "mediafmt.h"
/* Equal-temperament frequencies for MIDI 36 (C2) .. 96 (C7), rounded Hz. */
static const int freq_table[] = {
    65, 69, 73, 78, 82, 87, 92, 98,
    104, 110, 117, 123, 131, 139, 147, 156,
    165, 175, 185, 196, 208, 220, 233, 247,
    262, 277, 294, 311, 330, 349, 370, 392,
    415, 440, 466, 494, 523, 554, 587, 622,
    659, 698, 740, 784, 831, 880, 932, 988,
    1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568,
    1661, 1760, 1865, 1976, 2093,
};
const struct BuiltinTrack builtin_tracks[] = {
    {"Twinkle",
     "C4:1 C4:1 G4:1 G4:1 A4:1 A4:1 G4:2 F4:1 F4:1 E4:1 E4:1 D4:1 D4:1 C4:2 "
     "G4:1 G4:1 F4:1 F4:1 E4:1 E4:1 D4:2 G4:1 G4:1 F4:1 F4:1 E4:1 E4:1 D4:2 "
     "C4:1 C4:1 G4:1 G4:1 A4:1 A4:1 G4:2 F4:1 F4:1 E4:1 E4:1 D4:1 D4:1 C4:2"},
    {"Ode to Joy",
     "E4:1 E4:1 F4:1 G4:1 G4:1 F4:1 E4:1 D4:1 C4:1 C4:1 D4:1 E4:1 E4:2 "
     "E4:1 D4:1 D4:2 E4:1 E4:1 F4:1 G4:1 G4:1 F4:1 E4:1 D4:1 C4:1 C4:1 "
     "D4:1 E4:1 D4:2 C4:2"},
    {"Scale Run",
     "C4:1 D4:1 E4:1 F4:1 G4:1 A4:1 B4:1 C5:2 C5:1 B4:1 A4:1 G4:1 F4:1 "
     "E4:1 D4:1 C4:2 C3:1 E3:1 G3:1 C4:1 E4:1 G4:1 C5:1 G4:2"},
    {"Bass Walk",
     "C3:1 E3:1 G3:1 E3:1 F3:1 A3:1 C4:1 A3:1 G3:1 B3:1 D4:1 B3:1 C3:2 REST:1 "
     "C3:1 E3:1 G3:1 E3:1 A3:1 C4:1 E4:1 C4:1 G3:1 E3:1 C3:2 REST:1"},
};
const int builtin_track_count = 4;
static int is_space(char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }
int media_note_freq(const char *s, int len) {
    if (len <= 0 || len > 5)
        return -1;
    if (len == 4 && (s[0] == 'R' || s[0] == 'r') && (s[1] == 'E' || s[1] == 'e') &&
        (s[2] == 'S' || s[2] == 's') && (s[3] == 'T' || s[3] == 't'))
        return 0;
    char note = s[0];
    if (note >= 'a' && note <= 'g')
        note -= 32;
    int semi = -1;
    if (note == 'C')
        semi = 0;
    else if (note == 'D')
        semi = 2;
    else if (note == 'E')
        semi = 4;
    else if (note == 'F')
        semi = 5;
    else if (note == 'G')
        semi = 7;
    else if (note == 'A')
        semi = 9;
    else if (note == 'B')
        semi = 11;
    if (semi < 0)
        return -1;
    int i = 1;
    if (i < len && s[i] == '#') {
        semi++;
        i++;
    }
    if (i != len - 1 || s[i] < '0' || s[i] > '9')
        return -1;
    int midi = (s[i] - '0' + 1) * 12 + semi;
    if (midi < 36 || midi > 96)
        return -1;
    return freq_table[midi - 36];
}
int media_parse(const char *text, int *freqs, int *durs, int max) {
    int n = 0;
    const char *p = text;
    while (*p) {
        while (*p && is_space(*p))
            p++;
        if (!*p)
            break;
        const char *start = p;
        while (*p && !is_space(*p))
            p++;
        int len = (int)(p - start);
        const char *colon = 0;
        for (int i = 0; i < len; i++)
            if (start[i] == ':')
                colon = start + i;
        int beats = 1;
        int note_len = len;
        if (colon) {
            note_len = (int)(colon - start);
            int blen = len - note_len - 1;
            if (blen <= 0 || blen > 2)
                return -1;
            beats = 0;
            for (int i = 0; i < blen; i++) {
                if (colon[1 + i] < '0' || colon[1 + i] > '9')
                    return -1;
                beats = beats * 10 + (colon[1 + i] - '0');
            }
            if (beats < 1 || beats > 16)
                return -1;
        }
        int f = media_note_freq(start, note_len);
        if (f < 0)
            return -1;
        if (n >= max)
            return -1;
        freqs[n] = f;
        durs[n] = beats;
        n++;
    }
    return n ? n : -1;
}
static void skip_ws(const char **pp) {
    const char *p = *pp;
    for (;;) {
        while (*p && is_space(*p))
            p++;
        if (*p == '#') {
            while (*p && *p != '\n')
                p++;
            continue;
        }
        break;
    }
    *pp = p;
}
static int read_int(const char **pp) {
    skip_ws(pp);
    const char *p = *pp;
    if (*p < '0' || *p > '9')
        return -1;
    int v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        if (v > 100000)
            return -1;
        p++;
    }
    *pp = p;
    return v;
}
int media_parse_image(const char *text, u8 *rgb, int *w, int *h) {
    const char *p = text;
    if (p[0] != 'P' || (p[1] != '1' && p[1] != '2' && p[1] != '3'))
        return -1;
    int kind = p[1] - '0';
    p += 2;
    int width = read_int(&p), height = read_int(&p);
    if (width < 1 || height < 1 || width > PHOTO_MAX_W || height > PHOTO_MAX_H)
        return -1;
    int maxval = 1;
    if (kind != 1) {
        maxval = read_int(&p);
        if (maxval < 1 || maxval > 255)
            return -1;
    }
    int total = width * height;
    if (kind == 1) {
        int n = 0;
        while (*p && n < total) {
            if (*p == '#') {
                while (*p && *p != '\n')
                    p++;
                continue;
            }
            if (*p == '0' || *p == '1') {
                int v = *p == '1' ? 255 : 0;
                rgb[n * 3] = rgb[n * 3 + 1] = rgb[n * 3 + 2] = (u8)v;
                n++;
            }
            p++;
        }
        if (n != total)
            return -1;
    } else if (kind == 2) {
        for (int i = 0; i < total; i++) {
            int v = read_int(&p);
            if (v < 0 || v > maxval)
                return -1;
            int g = v * 255 / maxval;
            rgb[i * 3] = rgb[i * 3 + 1] = rgb[i * 3 + 2] = (u8)g;
        }
    } else {
        for (int i = 0; i < total; i++)
            for (int c = 0; c < 3; c++) {
                int v = read_int(&p);
                if (v < 0 || v > maxval)
                    return -1;
                rgb[i * 3 + c] = (u8)(v * 255 / maxval);
            }
    }
    *w = width;
    *h = height;
    return 0;
}
