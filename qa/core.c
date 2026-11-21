#include <assert.h>
#include <stdio.h>
#include "math.h"
#include "calendar.h"
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
    puts("PASS calendar and checked integer parser");
}
