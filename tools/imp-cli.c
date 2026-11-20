/* Host front end for the same linker and VM used in FreshOS. */
#include <stdio.h>
#include <string.h>
#include "compat.h"
static u8 input[IMP_LIMIT + 1], output[IMP_LIMIT];
int main(int argc, char **argv) {
    int run = argc == 3 && !strcmp(argv[1], "run");
    if (!run && (argc != 4 || strcmp(argv[1], "convert"))) {
        fprintf(stderr, "Usage: imp convert INPUT.exe OUTPUT.imp | imp run FILE.imp\n");
        return 2;
    }
    FILE *f = fopen(argv[2], "rb");
    if (!f) {
        perror(argv[2]);
        return 1;
    }
    size_t n = fread(input, 1, sizeof input, f);
    int err = ferror(f);
    fclose(f);
    if (err || n > IMP_LIMIT) {
        fprintf(stderr, "Cannot read input or file too large\n");
        return 1;
    }
    PeResult r;
    if (run) {
        if (!imp_run(input, n, &r)) {
            fprintf(stderr, "%s\n", r.detail);
            return 1;
        }
        fputs(r.output, stdout);
        fprintf(stderr, "IMP%u: exit %u (%u instructions)\n", r.bits, r.exit_code, r.steps);
        return r.exit_code & 255;
    }
    u32 size = imp_convert(input, n, output, sizeof output, &r);
    if (!size) {
        fprintf(stderr, "%s\n", r.detail);
        return 1;
    }
    f = fopen(argv[3], "wbx");
    if (!f) {
        perror(argv[3]);
        return 1;
    }
    int ok = fwrite(output, 1, size, f) == size;
    if (fclose(f) || !ok) {
        remove(argv[3]);
        return 1;
    }
    printf("%s: IMP%u, %u bytes\n", argv[3], r.bits, size);
    return 0;
}
