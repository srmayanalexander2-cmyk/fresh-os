/* A one-shot local RTC alarm and relative PIT countdown; session only. */
static int alarm_min = -1, counting;
static u32 timer_end;
static int clock_command(const char *s) {
    if (eq(s, "alarm off")) {
        alarm_min = -1;
        say("Alarm cleared.");
        return 1;
    }
    if (prefix(s, "alarm ")) {
        const char *t = s + 6;
        int v = -1;
        if (len(t) == 5 && t[2] == ':' && t[0] >= '0' && t[0] <= '2' && t[1] >= '0' &&
            t[1] <= '9' && t[3] >= '0' && t[3] <= '5' && t[4] >= '0' && t[4] <= '9')
            v = ((t[0] - '0') * 10 + t[1] - '0') * 60 + (t[3] - '0') * 10 + t[4] - '0';
        if (v < 0 || v >= 1440)
            say("Use local 24-hour time: alarm HH:MM");
        else {
            alarm_min = v;
            notify("One-shot alarm set.");
        }
        return 1;
    }
    if (eq(s, "timer off")) {
        counting = 0;
        say("Timer cleared.");
        return 1;
    }
    if (prefix(s, "timer ")) {
        const char *t = s + 6;
        int n = 0;
        while (*t >= '0' && *t <= '9' && n <= 86400)
            n = n * 10 + *t++ - '0';
        if (*t || n < 1 || n > 86400)
            say("Use timer SECONDS (1-86400) or timer off.");
        else {
            timer_end = ticks + n * 60;
            counting = 1;
            notify("Countdown started.");
        }
        return 1;
    }
    return 0;
}
static void alarm_tick(u32 now) {
    int a = alarm_min == hour * 60 + minute, t = counting && (int)(now - timer_end) >= 0;
    if (a)
        alarm_min = -1;
    if (t)
        counting = 0;
    if (a || t) {
        sound();
        notify(a ? "Clock alarm: the set time has arrived." : "Clock timer finished.");
        serial("Clock alert\r\n");
    }
}
static void clock_details(int x, int y) {
    char hhmm[6], seconds[12];
    if (alarm_min >= 0) {
        two(hhmm, alarm_min / 60);
        hhmm[2] = ':';
        two(hhmm + 3, alarm_min % 60);
    }
    text(x, y, "Alarm", 1, INK);
    text(x + 76, y, alarm_min >= 0 ? hhmm : "Off", 1, INK);
    num(seconds, counting ? (int)(timer_end - ticks + 59) / 60 : 0);
    text(x + 193, y, "Timer (seconds)", 1, INK);
    text(x + 344, y, seconds, 1, INK);
    text(x, y + 42,
         "In Terminal: alarm HH:MM   /   timer SECONDS   /   alarm off   /   timer off", 0,
         INK);
}
