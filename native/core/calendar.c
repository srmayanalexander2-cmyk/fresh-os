#include "calendar.h"
int days_in_month(int y, int m) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12)
        return 0;
    return days[m - 1] + (m == 2 && !(y % 4) && (y % 100 || !(y % 400)));
}
int first_weekday(int y, int m) {
    static const int offset[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 1 || m > 12)
        return 0;
    if (m < 3)
        y--;
    return (y + y / 4 - y / 100 + y / 400 + offset[m - 1] + 1) % 7;
}
