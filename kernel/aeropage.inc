/* Bounded document model. Styles/scripts are never painted as source and page
 * output is text, not a command, filename, native address, or a second script. */
#define URL_CAP 224
static char address[URL_CAP], location[URL_CAP], paths[8][URL_CAP], starred[URL_CAP];
static int hcount, hpos, editing, replace_address, scrollrow, linkfocus;
static char title[80], body[8192], linkurl[24][URL_CAP], notice[96];
/* These buffers are live only during synchronous evaluation, outside worker RAM. */
#define script ((char *)(imported + JS_RAM))
#define JS_OUT_CAP 4096
#define jsout (script + JS_SOURCE_MAX + 1)
_Static_assert(JS_RAM + JS_SOURCE_MAX + 1 + JS_OUT_CAP <= sizeof imported, "worker scratch");
static int bodylen, nlinks;
static signed char style_at[sizeof body]; /* -1 heading, 0 body, positive link index */
static const char landing[] =
    "<title>Aero Browser</title><h1>Welcome to Aero</h1><p>Open a small HTTP site or a local "
    "HTML file. Ctrl+L focuses the address field.</p><a href='aero:guide'>Browsing "
    "guide</a><a href='aero:demo'>Run the JavaScript demo</a><a href='aero:favorite'>Open "
    "your saved favorite</a>";
static const char guide[] =
    "<title>Browsing guide</title><h1>Getting around</h1><p>Enter an http:// address and "
    "choose Go. DHCP and DNS use a supported Ethernet adapter. Plain HTTP is not private: "
    "never enter passwords.</p><p>Tab selects links. Enter follows one. Up and Down scroll. "
    "Use the star to save one favorite to writable Home. Ctrl+R reloads.</p><p>Open "
    "home:name.html for a local page. Aero supports text, headings, links and inline "
    "MicroQuickJS scripts, not CSS, images, HTTPS or browser extensions.</p><a "
    "href='aero:home'>Start page</a>";
static const char demo[] =
    "<title>Script lab</title><h1>JavaScript lab</h1><p>The next line is computed in the "
    "guest:</p><script>var xs=[2,3,4,5];function square(x){return "
    "x*x;}document.write('Squares: '+xs.map(square).join(', '));document.title='Computed in "
    "Aero';</script><a href='aero:home'>Start page</a>";
static char lower_web(char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int match_web(const char *s, const char *k) {
    while (*k)
        if (lower_web(*s++) != *k++)
            return 0;
    return 1;
}
static const char *find_web(const char *s, const char *k) {
    while (*s && !match_web(s, k))
        s++;
    return s;
}
static void bodychar(char c, int style) {
    if (bodylen + 1 >= (int)sizeof body)
        return;
    if (c == '\r')
        return;
    if (c == '\n' && (!bodylen || body[bodylen - 1] == '\n'))
        return;
    if ((u8)c < 32 && c != '\n')
        c = ' ';
    if ((u8)c >= 127)
        c = '?';
    body[bodylen] = c;
    style_at[bodylen++] = style;
    body[bodylen] = 0;
}
static void bodystr(const char *s, int style) {
    while (*s)
        bodychar(*s++, style);
}
static int add_code(int *n, const char *s, int k) {
    if (k > JS_SOURCE_MAX - *n)
        return 0;
    for (int i = 0; i < k; i++)
        script[(*n)++] = s[i];
    script[*n] = 0;
    return 1;
}
static void page_reset(void) {
    title[0] = notice[0] = body[0] = 0;
    bodylen = nlinks = scrollrow = linkfocus = 0;
    dirty = 1;
}
static void page_error(const char *s) {
    page_reset();
    copy(title, "Navigation stopped", sizeof title);
    bodystr(s, 0);
    serial("Aero: ");
    serial(s);
    serial("\r\n");
}
static int tagspace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
static void href_attr(const char *p, const char *end, char *out) {
    out[0] = 0;
    while (p < end) {
        while (p < end && tagspace(*p))
            p++;
        const char *n = p;
        while (p < end && !tagspace(*p) && *p != '=')
            p++;
        int href = p - n == 4 && match_web(n, "href");
        while (p < end && tagspace(*p))
            p++;
        if (p == end || *p != '=')
            continue;
        p++;
        while (p < end && tagspace(*p))
            p++;
        char q = p < end && (*p == '\'' || *p == '"') ? *p++ : 0;
        const char *v = p;
        while (p < end && (q ? *p != q : !tagspace(*p)))
            p++;
        int size = p - v;
        if (q && p < end)
            p++;
        if (href && size > 0 && size < URL_CAP) {
            for (int i = 0; i < size; i++)
                out[i] = v[i];
            out[size] = 0;
            return;
        }
    }
}
static void render_html(const char *s, int plain) {
    page_reset();
    copy(title, plain ? "Text document" : "Web document", sizeof title);
    if (plain) {
        bodystr(s, 0);
        return;
    }
    int style = 0, inhead = 0, code = 0, hasjs = 0, ok = 1;
    const char *pre =
        "var "
        "document={title:'',body:{textContent:null},output:'',write:function(x){this.output+="
        "String(x);},writeln:function(x){this.write(x);this.write('\\n');}};\n";
    add_code(&code, pre, len(pre));
    while (*s) {
        if (*s != '<') {
            if (!inhead) {
                char c = *s;
                int consumed = 1;
                if (c == '&') {
                    const char *const ents[] = {"&amp;",  "&lt;",   "&gt;",
                                                "&quot;", "&nbsp;", "&#39;"};
                    const char dec[] = "&<>\" '";
                    for (int j = 0; j < 6; j++)
                        if (prefix(s, ents[j])) {
                            c = dec[j];
                            consumed = len(ents[j]);
                            break;
                        }
                }
                if (tagspace(c)) {
                    if (bodylen && body[bodylen - 1] != ' ' && body[bodylen - 1] != '\n')
                        bodychar(' ', style);
                } else
                    bodychar(c, style);
                s += consumed;
            } else
                s++;
            continue;
        }
        if (prefix(s, "<!--")) {
            s = find_web(s + 4, "-->");
            if (*s)
                s += 3;
            continue;
        }
        const char *n = s + 1;
        int close = *n == '/';
        if (close)
            n++;
        const char *p = n;
        while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
               (*p >= '0' && *p <= '9'))
            p++;
        int ns = p - n;
        const char *end = p;
        char quote = 0;
        while (*end) {
            if (!quote && *end == '>')
                break;
            if (*end == '\'' || *end == '"') {
                if (!quote)
                    quote = *end;
                else if (*end == quote)
                    quote = 0;
            }
            end++;
        }
        if (!*end)
            break;
        s = end + 1;
#define TAG(k) (ns == (int)sizeof(k) - 1 && match_web(n, k))
        if (!close && (TAG("script") || TAG("style") || TAG("title"))) {
            const char *t = find_web(s, TAG("script")  ? "</script"
                                        : TAG("style") ? "</style"
                                                       : "</title");
            int count = t - s;
            if (TAG("script")) {
                hasjs = 1;
                ok = ok && add_code(&code, s, count) && add_code(&code, ";\n", 2);
            }
            if (TAG("title")) {
                count = clamp(count, 0, sizeof title - 1);
                for (int j = 0; j < count; j++)
                    title[j] = (u8)s[j] >= 32 && (u8)s[j] < 127 ? s[j] : ' ';
                title[count] = 0;
            }
            s = t;
            while (*s && *s != '>')
                s++;
            if (*s)
                s++;
            continue;
        }
        if (TAG("head"))
            inhead = !close;
        if (inhead)
            continue;
        if (TAG("a")) {
            bodychar('\n', 0);
            style = 0;
            if (!close && nlinks < 24) {
                href_attr(p, end, linkurl[nlinks]);
                if (linkurl[nlinks][0])
                    style = ++nlinks;
            }
        }
        if (ns == 2 && lower_web(n[0]) == 'h' && n[1] >= '1' && n[1] <= '6') {
            bodychar('\n', 0);
            style = close ? 0 : -1;
        }
        if (TAG("p") || TAG("div") || TAG("br") || TAG("li")) {
            bodychar('\n', 0);
            if (!close && TAG("li"))
                bodystr("- ", 0);
        }
#undef TAG
    }
    if (hasjs) {
        const char *post = "\nString(document.title).replace(/\\n/g,' "
                           "')+'\\n'+(document.body.textContent===null?'A'+document.output:'"
                           "R'+String(document.body.textContent));";
        if (!ok || !add_code(&code, post, len(post)))
            copy(notice, "Scripts exceed page limit", sizeof notice);
        else {
            const char *err =
                aero_eval(imported, AERO_ENGINE, AERO_ENGINE_SIZE, script, jsout, JS_OUT_CAP);
            if (err)
                copy(notice, err, sizeof notice);
            else {
                char *p = jsout;
                while (*p && *p != '\n')
                    p++;
                if (*p) {
                    *p++ = 0;
                    if (jsout[0])
                        copy(title, jsout, sizeof title);
                    if (*p == 'R') {
                        bodylen = nlinks = 0;
                        body[0] = 0;
                    }
                    bodychar('\n', 0);
                    if (*p)
                        bodystr(p + 1, 0);
                }
            }
        }
    }
    if (bodylen == sizeof body - 1)
        copy(notice, "Text clipped at 8 KiB", sizeof notice);
    dirty = 1;
}
static void navigate(const char *url, int remember) {
    char target[URL_CAP];
    if (!*url || len(url) >= URL_CAP) {
        page_error("Empty or oversized address");
        return;
    }
    copy(target, url, sizeof target);
    web_cancel();
    if (remember) {
        if (hcount && eq(target, paths[hpos])) {
        } else {
            hcount = hcount ? hpos + 1 : 0;
            if (hcount == 8) {
                for (int i = 1; i < 8; i++)
                    copy(paths[i - 1], paths[i], URL_CAP);
                hcount--;
            }
            hpos = hcount++;
            copy(paths[hpos], target, URL_CAP);
        }
    }
    copy(address, target, URL_CAP);
    copy(location, target, URL_CAP);
    editing = replace_address = 0;
    if (eq(target, "aero:home"))
        render_html(landing, 0);
    else if (eq(target, "aero:guide"))
        render_html(guide, 0);
    else if (eq(target, "aero:demo"))
        render_html(demo, 0);
    else if (eq(target, "aero:favorite")) {
        char saved[URL_CAP];
        int n = home_read("aero.favorite", saved, sizeof saved - 1);
        if (n > 0) {
            saved[n] = 0;
            copy(starred, saved, sizeof starred);
        }
        if (prefix(starred, "http://") || prefix(starred, "home:") ||
            eq(starred, "aero:demo") || eq(starred, "aero:guide")) {
            navigate(starred, remember);
            return;
        }
        page_error("No favorite saved. Open a page and choose Star.");
    } else if (prefix(target, "home:")) {
        const char *file = target + 5;
        if (!suffix(file, ".html") && !suffix(file, ".htm") && !suffix(file, ".txt")) {
            page_error("Local pages must be .html, .htm or .txt");
            return;
        }
        int n = home_read(file, imported, 24575);
        if (n < 0) {
            page_error("Local page missing, unreadable or over 24 KiB");
            return;
        }
        imported[n] = 0;
        render_html((char *)imported, suffix(file, ".txt"));
    } else if (prefix(target, "http://")) {
        char *h = target + 7, *p = h;
        while (*p && *p != '/' && *p != ':') {
            char c = lower_web(*p);
            if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '.' && c != '-') {
                page_error("Invalid hostname");
                return;
            }
            p++;
        }
        char hostname[128];
        int hn = p - h;
        if (!hn || hn > 127) {
            page_error("Invalid hostname length");
            return;
        }
        copy(hostname, h, hn + 1);
        int port = 80;
        if (*p == ':') {
            p++;
            port = 0;
            while (*p >= '0' && *p <= '9' && port <= 65535)
                port = port * 10 + *p++ - '0';
            if (port < 1 || port > 65535 || (*p && *p != '/')) {
                page_error("Invalid HTTP port");
                return;
            }
        }
        for (char *q = p; *q; q++) {
            if (*q == '#') {
                *q = 0;
                break;
            }
            if ((u8)*q <= 32 || (u8)*q >= 127) {
                page_error("Use percent-encoded ASCII URLs");
                return;
            }
        }
        page_reset();
        copy(title, "Loading...", sizeof title);
        bodystr("Connecting over Ethernet. Checking DHCP, DNS and TCP...", 0);
        const char *err = web_get(hostname, port, *p ? p : "/");
        if (err)
            page_error(err);
    } else
        page_error("Use http://, home:name.html or aero:home. HTTPS is not supported; it "
                   "will not be downgraded.");
    dirty = 1;
}
static void follow(int i) {
    if (i < 1 || i > nlinks)
        return;
    const char *u = linkurl[i - 1];
    char buf[URL_CAP];
    if (prefix(location, "http://") && !prefix(u, "http://")) {
        if (*u == '#')
            return;
        int cut = 7;
        while (location[cut] && location[cut] != '/')
            cut++;
        if (prefix(u, "//")) {
            cut = 5;
        } else if (*u != '/') {
            for (int j = 0; u[j] && u[j] != '/'; j++)
                if (u[j] == ':') {
                    page_error("Cannot follow this scheme from a remote page");
                    return;
                }
            int j = cut;
            while (location[j] && location[j] != '?' && location[j] != '#') {
                if (location[j] == '/')
                    cut = j + 1;
                j++;
            }
        }
        int slash = *u != '/' && location[cut - 1] != '/';
        if (cut + len(u) + slash >= URL_CAP) {
            page_error("Link too long");
            return;
        }
        copy(buf, location, cut + 1);
        if (slash)
            buf[cut++] = '/';
        copy(buf + cut, u, URL_CAP - cut);
        u = buf;
    } else if (prefix(location, "home:") && !prefix(u, "http:") && !prefix(u, "home:") &&
               !prefix(u, "aero:")) {
        copy(buf, "home:", URL_CAP);
        if (len(u) > URL_CAP - 6)
            return;
        copy(buf + 5, u, URL_CAP - 5);
        u = buf;
    }
    navigate(u, 1);
}
static void web_update(void) {
    web_tick();
    const char *t;
    int plain;
    int r = web_result(&t, &plain);
    if (r > 0) {
        render_html(t, plain);
        serial("Aero HTTP document ready: ");
        serial(title);
        serial("\r\n");
    }
    if (r < 0)
        page_error(t);
}
