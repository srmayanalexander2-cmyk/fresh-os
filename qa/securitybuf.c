/* A freestanding Windows program linked against the SDK's import libraries.
 * Check the bytes Windows apps actually consume, not just BOOL returns. */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
typedef struct {
    ACE_HEADER h;
    DWORD mask;
    DWORD sid[3];
} Record;
int mainCRTStartup(void) {
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    Record records[2];
    for (unsigned i = 0; i < 2; i++) {
        Record *r = &records[i];
        r->h.AceType = i ? ACCESS_DENIED_ACE_TYPE : ACCESS_ALLOWED_ACE_TYPE;
        r->h.AceFlags = 0;
        r->h.AceSize = sizeof *r;
        r->mask = 1u << i;
        if (!InitializeSid(r->sid, &nt, 1) || *GetSidSubAuthorityCount(r->sid) != 1)
            return 1;
        *GetSidSubAuthority(r->sid, 0) = 32 + i;
        if (!IsValidSid(r->sid) || GetLengthSid(r->sid) != 12 ||
            GetSidLengthRequired(1) != 12)
            return 2;
    }
    DWORD copy[3];
    if (CopySid(8, copy, records[0].sid) || GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        return 3;
    if (!CopySid(sizeof copy, copy, records[0].sid) || !EqualSid(copy, records[0].sid) ||
        EqualSid(copy, records[1].sid))
        return 4;
    DWORD buffer[(8 + 3 * sizeof(Record)) / 4];
    PACL acl = (PACL)buffer;
    if (!InitializeAcl(acl, sizeof buffer, ACL_REVISION))
        return 5;
    if (!AddAce(acl, ACL_REVISION, MAXDWORD, records, sizeof records))
        return 6;
    Record *at;
    if (!GetAce(acl, 1, (void **)&at) || at->h.AceType != ACCESS_DENIED_ACE_TYPE)
        return 7;
    /* Duplicate an ACE already in the destination ACL, inserting at the front. */
    if (!AddAce(acl, ACL_REVISION, 0, at, sizeof *at))
        return 8;
    ACL_SIZE_INFORMATION sz;
    if (!GetAclInformation(acl, &sz, sizeof sz, AclSizeInformation) || sz.AceCount != 3 ||
        sz.AclBytesFree || sz.AclBytesInUse != sizeof buffer)
        return 9;
    if (AddAce(acl, ACL_REVISION, 0, records, sizeof *at) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        return 10;
    if (!GetAce(acl, 0, (void **)&at) || at->mask != 2 || !GetAce(acl, 1, (void **)&at) ||
        at->mask != 1)
        return 11;
    if (!DeleteAce(acl, 0) || !GetAce(acl, 0, (void **)&at) || at->mask != 1)
        return 12;
    records[0].h.AceSize = 0;
    if (AddAce(acl, ACL_REVISION, 0, records, sizeof *at) ||
        GetLastError() != ERROR_INVALID_PARAMETER || acl->AceCount != 2)
        return 13;
    if (GetAce(acl, 2, (void **)&at))
        return 14;
    HANDLE heap = HeapCreate(0, 0, 8192);
    if (!heap)
        return 15;
    BYTE *b = HeapAlloc(heap, HEAP_ZERO_MEMORY, 127);
    if (!b || b[126])
        return 16;
    b[0] = 0x39;
    if (!HeapAlloc(heap, 0, 32) || HeapReAlloc(heap, HEAP_REALLOC_IN_PLACE_ONLY, b, 2048))
        return 17;
    BYTE *next = HeapReAlloc(heap, HEAP_ZERO_MEMORY, b, 2048);
    if (!next || next == b || next[0] != 0x39 || next[127] || HeapSize(heap, 0, next) != 2048)
        return 18;
    if (HeapAlloc(heap, 0, 8193) || HeapFree(GetProcessHeap(), 0, next) ||
        HeapDestroy(GetProcessHeap()))
        return 19;
    if (!HeapFree(heap, 0, next) || HeapFree(heap, 0, next) || !HeapAlloc(heap, 0, 64))
        return 20;
    if (!HeapDestroy(heap) || HeapDestroy(heap) || HeapAlloc(heap, 0, 1))
        return 21;
    CRITICAL_SECTION cs;
    InitializeCriticalSection(&cs);
    EnterCriticalSection(&cs);
    if (!TryEnterCriticalSection(&cs) || cs.RecursionCount != 2 || !cs.OwningThread)
        return 22;
    LeaveCriticalSection(&cs);
    if (cs.RecursionCount != 1)
        return 23;
    LeaveCriticalSection(&cs);
    if (cs.RecursionCount || cs.OwningThread)
        return 24;
    DeleteCriticalSection(&cs);
    if (!InitializeCriticalSectionAndSpinCount(&cs, 500) || cs.SpinCount)
        return 25;
    DeleteCriticalSection(&cs);
    if (!InitializeCriticalSectionEx(&cs, 0, CRITICAL_SECTION_NO_DEBUG_INFO))
        return 26;
    DeleteCriticalSection(&cs);
    return 0;
}
