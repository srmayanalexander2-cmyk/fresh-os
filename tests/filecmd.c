/* Capture the guest command output with an in-memory, read-only file fixture.
 * tests/browser_home.py separately checks real copy/write refusal on disk. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char u8;
#include "../kernel/util.h"
static u8 packed[256 * 1024], src[256];
static char output[1024];
static void say(const char *s) {
    assert(strlen(output) + strlen(s) + 2 < sizeof output);
    strcat(output, s);
    strcat(output, "\n");
}
#define notify say
static int tool_file(const char *name, const u8 **p) {
    const char *s = eq(name, "one") || eq(name, "same") ? "alpha\nbeta\nalphabet\n"
                    : eq(name, "two")                   ? "alpha\nBeta\n"
                                                        : 0;
    if (!s)
        return -2;
    strcpy((char *)src, s);
    *p = src;
    return strlen(s);
}
static int home_ready(void) { return 0; }
static int home_read(const char *n, u8 *p, int k) { return -2; }
static int home_save(const char *n, const u8 *p, int k) { return 0; }
#include "../kernel/filecmd.inc"
static void expect(const char *command, const char *answer) {
    char line[100];
    strcpy(line, command);
    output[0] = 0;
    assert(data_commands(line));
    assert(strcmp(output, answer) == 0);
}
int main(void) {
    expect("grep alpha one", "alpha\nalphabet\n");
    expect("findstr BETA one", "No matching lines.\n");
    expect("fc /b one two", "First differing byte: 6\n");
    expect("cmp one same", "Identical.\n");
    expect("cmp one missing", "Second file missing or unreadable.\n");
    expect("xxd two", "0000: 61 6c 70 68 61 0a 42 65 74 61 0a\n");
    expect("copy one new", "Use a new destination name on writable Home.\n");
    puts("PASS file search, byte comparison, hex dump and no-disk handling");
}
