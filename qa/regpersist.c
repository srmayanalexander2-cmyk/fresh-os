/* A normal Win64 test executable. No FreshOS-specific ABI calls. */
#include <windows.h>
static const WCHAR path[] = L"Software\\FreshOS\\AppTest";
static const WCHAR label[] = L"SavedValue";
int mainCRTStartup(void) {
    HKEY k;
    DWORD value = 0xa03f7812, got = 0, n = sizeof got, t = 0, disp = 0;
#ifdef WRITER
    if (RegCreateKeyExW(HKEY_CURRENT_USER, path, 0, 0, 0, KEY_READ | KEY_WRITE, 0, &k,
                        &disp) ||
        disp != REG_CREATED_NEW_KEY)
        return 10;
    if (RegSetValueExW(k, label, 0, REG_DWORD, (const BYTE *)&value, sizeof value))
        return 11;
#else
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\FRESHOS\\APPTEST", 0, KEY_READ, &k))
        return 12;
    if (RegSetValueExW(k, label, 0, REG_DWORD, (const BYTE *)&value, sizeof value) !=
        ERROR_ACCESS_DENIED)
        return 13;
#endif
    n = 0;
    if (RegQueryValueExW(k, L"savedvalue", 0, &t, 0, &n) || t != REG_DWORD || n != 4)
        return 14;
    n = 2;
    if (RegQueryValueExW(k, label, 0, 0, (BYTE *)&got, &n) != ERROR_MORE_DATA || n != 4)
        return 15;
    if (RegQueryValueExW(k, label, 0, &t, (BYTE *)&got, &n) || got != value || n != 4 ||
        t != REG_DWORD)
        return 16;
    if (RegCloseKey(k) || RegCloseKey(k) != ERROR_INVALID_HANDLE)
        return 17;
#ifndef WRITER
    if (RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, KEY_WRITE, &k) ||
        RegDeleteValueW(k, label) || RegDeleteValueW(k, label) != ERROR_FILE_NOT_FOUND ||
        RegCloseKey(k))
        return 18;
#endif
    return 0;
}
