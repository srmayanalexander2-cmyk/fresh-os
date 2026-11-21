/* A stopped kernel must not call the shell, filesystem or sound driver. COM1 and
 * the last firmware framebuffer are best-effort diagnostics, not recovery. */
static void fault_hex(char *p, unsigned long v) {
    *p++ = '0';
    *p++ = 'x';
    for (int i = (int)sizeof(v) * 2 - 1; i >= 0; i--)
        *p++ = "0123456789abcdef"[(v >> (4 * i)) & 15];
    *p = 0;
}
void kernel_fault(unsigned long vector, unsigned long err, unsigned long pc) {
    static const char *const names[32] = {[0] = "Divide error",
                                          [1] = "Debug exception",
                                          [2] = "Non-maskable interrupt",
                                          [3] = "Breakpoint",
                                          [4] = "Overflow",
                                          [5] = "Bound range exceeded",
                                          [6] = "Invalid instruction",
                                          [7] = "Device not available",
                                          [8] = "Double fault",
                                          [10] = "Invalid task state",
                                          [11] = "Segment not present",
                                          [12] = "Stack fault",
                                          [13] = "General protection fault",
                                          [14] = "Page fault",
                                          [16] = "Floating-point exception",
                                          [17] = "Alignment check",
                                          [18] = "Machine check",
                                          [19] = "SIMD exception",
                                          [21] = "Control protection fault"};
    const char *name =
        vector < 32 && names[vector] ? names[vector] : "Unhandled CPU exception";
    char ip[20], code[20], addr[20], v[12];
    num(v, vector);
    fault_hex(ip, pc);
    fault_hex(code, err);
    unsigned long cr2;
    __asm__ volatile("mov %%cr2,%0" : "=r"(cr2));
    fault_hex(addr, cr2);
    serial("FreshOS kernel stopped: ");
    serial(name);
    serial(" | vector ");
    serial(v);
    serial(" | IP ");
    serial(ip);
    serial(" | code ");
    serial(code);
    if (vector == 14) {
        serial(" | address ");
        serial(addr);
    }
    serial("\r\n");
    box(0, 0, W, H, 0x17354a);
    int x = W / 2 - 320, y = H / 2 - 138;
    text(x, y, "FreshOS stopped", 3, WHITE);
    text(x, y + 64, name, 2, ICE);
    text(x, y + 115, "CPU vector", 1, ICE);
    text(x + 142, y + 115, v, 1, WHITE);
    text(x, y + 144, "Instruction pointer", 1, ICE);
    text(x + 142, y + 144, ip, 1, WHITE);
    text(x, y + 173, "Error code", 1, ICE);
    text(x + 142, y + 173, code, 1, WHITE);
    if (vector == 14) {
        text(x, y + 202, "Fault address", 1, ICE);
        text(x + 142, y + 202, addr, 1, WHITE);
    }
    text(x, y + 250, "Photograph this screen or copy the COM1 log, then restart the VM.", 1,
         WHITE);
    present();
    for (;;)
        __asm__ volatile("cli; hlt");
}
