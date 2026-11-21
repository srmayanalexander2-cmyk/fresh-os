/* Runs only inside the private RV32 worker. No guest/host OS calls. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "mquickjs.h"
#define JS_CLASS_COUNT JS_CLASS_USER
static JSValue js_print(JSContext *c, JSValue *t, int n, JSValue *a) {
    (void)c;
    (void)t;
    (void)n;
    (void)a;
    return JS_UNDEFINED;
}
static JSValue js_date_now(JSContext *c, JSValue *t, int n, JSValue *a) {
    return JS_NewInt32(c, 0);
}
static JSValue js_date_constructor(JSContext *c, JSValue *t, int n, JSValue *a) {
    double d = 0;
    if ((n & ~FRAME_CF_CTOR) == 1 && JS_ToNumber(c, &d, a[0]))
        return JS_EXCEPTION;
    return JS_NewDate(c, d);
}
#define js_performance_now js_date_now
#include "jslib.h"
/* Fixed message ABI: inputs at 0x60000, output <=4095 bytes at 0x70000.
   Each evaluation gets a fresh 96 KiB garbage-collected heap. */
int worker(void) {
    const char *s = (const char *)0x60000;
    char *out = (char *)0x70000;
    JSContext *c = JS_NewContext((void *)0x34000, 96 * 1024, &js_stdlib);
    JSValue v = JS_Eval(c, s, strlen(s), "page.js", JS_EVAL_RETVAL);
    int failed = JS_IsException(v);
    if (failed)
        v = JS_GetException(c);
    JSCStringBuf b;
    const char *str = JS_ToCString(c, v, &b);
    if (!str)
        return 2;
    int i = 0;
    while (i < 4095 && str[i]) {
        out[i] = str[i];
        i++;
    }
    out[i] = 0;
    return failed;
}
