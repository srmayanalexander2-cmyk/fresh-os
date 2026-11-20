/* Per-session clipboard and notebook. Persistence is explicit, append-only Home. */
static char clip[1400], cards[3][241], todos[12][41], draft[41];
static int card, ntodo, todopage, todosel, helpindex, chidx, evpage;
static unsigned checked;
static char events[32][84];
static u32 eventtime[32];
static int ehead, ecount;
static void log_event(const char *s) {
    copy(events[ehead], s, 84);
    eventtime[ehead] = ticks / 60;
    ehead = (ehead + 1) % 32;
    if (ecount < 32)
        ecount++;
    evpage = 0;
}
static int safe_text(const char *s, int n, int multiline) {
    for (int i = 0; i < n; i++)
        if ((s[i] < 32 || s[i] > 126) && !(multiline && s[i] == '\n'))
            return 0;
    return 1;
}
static void organizer_load(void) {
    for (int i = 0; i < 3; i++) {
        char fname[] = "sticky1.txt";
        fname[6] += (char)i;
        int n = home_read(fname, cards[i], 240);
        if (n < 0 || !safe_text(cards[i], n, 1))
            n = 0;
        cards[i][n] = 0;
    }
    char data[540];
    int n = home_read("todo.list", data, sizeof data);
    for (int pos = 0; pos < n && ntodo < 12;) {
        int start = pos;
        while (pos < n && data[pos] != '\n')
            pos++;
        int size = pos - start - 2;
        if (pos == n || size < 1 || size > 40 || data[start + 1] != ' ' ||
            (data[start] != 'x' && data[start] != '-') ||
            !safe_text(data + start + 2, size, 0))
            break;
        if (data[start] == 'x')
            checked |= 1u << ntodo;
        for (int j = 0; j < size; j++)
            todos[ntodo][j] = data[start + 2 + j];
        todos[ntodo++][size] = 0;
        pos++;
    }
}
static void card_save(void) {
    char fname[] = "sticky1.txt";
    fname[6] += (char)card;
    notify(home_save(fname, cards[card], len(cards[card]))
               ? "Sticky note saved."
               : "Sticky note in RAM only: writable Home required.");
}
static void todo_save(void) {
    char data[540];
    int n = 0;
    for (int i = 0; i < ntodo; i++) {
        data[n++] = (checked >> i) & 1 ? 'x' : '-';
        data[n++] = ' ';
        for (int j = 0; todos[i][j]; j++)
            data[n++] = todos[i][j];
        data[n++] = '\n';
    }
    notify(home_save("todo.list", data, n)
               ? "Checklist saved."
               : "Checklist in RAM only: writable Home required.");
}
static void add_task(void) {
    if (!draft[0])
        return;
    if (ntodo == 12) {
        notify("Checklist full. Clear finished items first.");
        return;
    }
    copy(todos[ntodo], draft, 41);
    todosel = ntodo++;
    todopage = todosel / 4;
    draft[0] = 0;
}
static void clear_done(void) {
    int n = 0;
    for (int i = 0; i < ntodo; i++)
        if (!(checked & (1u << i)))
            copy(todos[n++], todos[i], 41);
    ntodo = n;
    checked = 0;
    todosel = todopage = 0;
}
static void paste(char *s, int cap, int *pos, int multi) {
    int n = len(s), p = clamp(*pos, 0, n);
    for (int j = 0; clip[j] && n + 1 < cap; j++) {
        char c = clip[j];
        if (c == '\n' && !multi)
            c = ' ';
        if (c != '\n' && (c < 32 || c > 126))
            continue;
        for (int i = n; i >= p; i--)
            s[i + 1] = s[i];
        s[p++] = c;
        n++;
    }
    *pos = p;
}
