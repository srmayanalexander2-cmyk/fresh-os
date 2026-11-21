# Chrome acceptance: not passed

Target: a current official Windows installer running on the native FreshOS
kernel, followed by browser launch, an HTTPS page load and a profile check
after reboot. **None of the four acceptance checks has passed.** The following
are historical Google Omaha test fixtures, not supported versions of Chrome.
No Chrome binary is included in FreshOS.

## What was actually tried

Input source: `google/omaha`, commit `c3e428cce2af4f8619658b553292147643820219`,
`common/certificate_tag/testdata/`. SHA-256 for `ChromeSetup.exe` (819,184 bytes):
`db154536c7f4ace2189d1acd882ad33bd70e94e02e9a8c5f0a3af10f5f97f776`;
for `googlechromestandaloneenterprise.msi`:
`5ac765973e517cf29740c580892d84f43c27d5813fa0b9416e108dc4dcbdf058`.
The MSI's Omaha PE contained an LZMA/BCJ2 tar payload. On the **host** we
unwrapped that layer to obtain the 65,061,120-byte inner Chrome 85.0.4183.102
installer (`87be004cfc63d2d25b8b02e28ea24e584a49655db79423fa2f6393cdf2527456`).

Inside **booted FreshOS Lite x64 UEFI**, a Win64 libmspack client opened the
whole inner installer on `F:`, found the embedded cabinet, decompressed its
LZX stream and wrote `F:\STAGE\SETUP.EXE`. An independent FAT chain reader
verified the persisted 2,373,104-byte file's SHA-256:
`265ec32bdd3e666ed0bee316eee03977be68023ec90571f6f7dc2727ee5eda10`.
After a cold boot `runfile stage/setup.exe` reached the native PE loader,
which validated its PE32 guard metadata and stopped at its first unbound
import, `advapi32.dll!AdjustTokenPrivileges`. The executable did not enter user mode. The same extraction/reboot/launch test with an ordinary unmodified
calendar EXE succeeds and prints the correct February 2024 calendar.

| Executable | Native guest result |
|---|---|
| Archived ChromeSetup.exe | `Missing import: kernel32.dll!UnmapViewOfFile` (72 distinct static imports outside the current allowlist) |
| Chrome 85 setup.exe from the cabinet | `Missing import: advapi32.dll!AdjustTokenPrivileges`; also has delay imports and 295 unsupported static imports |
| Chrome 85 chrome.exe | `Missing import: chrome_elf.dll!GetInstallDetailsPayload`; 172 unsupported static imports |

The matching Chrome 85 `chrome_elf.dll` actually exports `GetInstallDetailsPayload`;
that is not a kernel function to stub. The raw loader does not resolve versioned
DLL dependencies; even that DLL has 119 unbound distinct static imports.

`chrome.dll` alone is 126,978,544 bytes and the uncompressed browser payload
exceeds 207 MB. The new data volume can store that much data; it does **not**
provide that much mapped executable memory. Concurrent browser processes,
threads, modern DLL loading and initialization, machine-wide registry, graphics, networking
and sandbox services are not supplied by the current ABI. No current stable
installer was executed. Guard metadata is not discarded to force a launch.

## Reproduction and other app checks

* `python qa/raw_launch.py Lite /path/to/ChromeSetup.exe` stages the supplied file unmodified on a disposable data volume and attempts `runfile` in a booted guest. It reports the exact input digest and fails on a loader refusal or nonzero exit. It does not verify vendor signatures, installation, or HTTPS.
* `python qa/securitybuf.py Lite` uses a Windows SDK client in both PE32 and PE64 to test ACL and SID buffer contents, private heap ownership and reallocation, and critical-section recursion. These checks do not grant token privileges or simulate concurrent threads.
* `qa/guard32.py Lite` checks an instrumented indirect call, rejection of an unlisted target, and initialization of the load-config cookie. `qa/regpersist.py Lite` checks a per-user Win32 registry value across a cold boot. Neither test runs Chrome.
* See `extras/cabstage/README.md` for `qa/cabstage.py` commands.
* `python qa/children.py Lite` builds a Win64 parent, exercises actual child
  execution, signaled handles, exit status and parent resumption, then runs
  PE32 and PE64 programs with static TLS template/zero-fill and attach/detach
  callbacks on the initial thread.
* `qa/win_tools.py` tests getfiletype and Nikon shuttercount; `qa/processes.py`
  checks cal, private memory, faults, and optionally upstream BusyBox.
* `python qa/pe_preflight.py /path/to/ChromeSetup.exe` reports the input hash,
  unbound imports and **host IMP packer** result. The raw guest PE loader is a
  separate path (`runfile`); static pack/preflight success is not execution.
