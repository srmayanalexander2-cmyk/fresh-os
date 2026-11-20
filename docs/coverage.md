# Requested Windows apps and features

Status of this build, not the unavailable system described in `take.txt`. **Partial** means a specific FreshOS counterpart exists, not Windows API or binary compatibility. Unimplemented entries are not registered as fake apps.


## Windows 10 (Version 1507) Features

| Request | Status |
|---|---|
| Hybrid Start Menu (combined traditional list with resizable Live Tiles) | Partial: GUI launcher and command-line Start; no Live Tiles. |
| Continuum (automatic/manual tablet mode switch) | Not implemented. |
| Task View and Virtual Desktops (Win + Tab) | Partial: keyboard app tray; no virtual desktops. |
| Snap Assist & Quadrant Snapping (2x2 window snapping) | Not implemented. |
| Action Center & Quick Actions sidebar | Not implemented. |
| Universal Windows Platform (UWP) app architecture | Not implemented. |
| Quick Access File Explorer view | Partial: workspace shortcuts and named Home records, not Windows Explorer. |
| Unified Settings App | Partial: native shell, wallpaper, motion, text, brightness and feedback settings. |
| Windows Hello (biometric fingerprint and IR facial recognition) | Not implemented. |
| Microsoft Passport / Windows Hello for Business (cryptographic key authentication) | Not implemented. |
| Device Guard & Credential Guard | Not implemented. |
| Windows Defender Antivirus (built-in real-time anti-malware) | Not implemented. |
| DirectX 12 low-level graphics API | Not implemented. |
| Game DVR & Game Bar (Win + G) | Not implemented. |
| Windows Update for Business & Peer-to-Peer update sharing | Not implemented. |
| Command Prompt enhancements (Ctrl + C/Ctrl + V, word wrap) | Partial: editable command line, previous command, app suggestions and completion; no clipboard. |
| Native MKV, FLAC, and HEVC codec support | Not implemented. |

## Windows 11 (Version 24H2) Features

| Request | Status |
|---|---|
| Scrollable Quick Settings flyout with reorderable toggles | Not implemented. |
| File Explorer context menu text labels beneath top-row icons | Partial: Files launches the session note and canvas; Home commands manage named records. |
| Native 7-Zip and TAR archive creation (.7z, .tar, .gz, .bz2, .xz, .zst) | Not implemented. |
| Energy Saver mode (for both laptops and desktops) | Not implemented. |
| Content Adaptive Brightness Control (CABC) on AC power | Not implemented. |
| Voice Clarity (echo cancellation and noise suppression) | Not implemented. |
| Phone Link Start Menu companion panel | Not implemented. |
| HDR .jxr background wallpaper support | Not implemented. |
| Wi-Fi 7 (802.11be) native support | Not implemented. |
| Bluetooth LE Audio & hearing aid controls | Not implemented. |
| Sudo for Windows (CLI privilege elevation) | Not implemented. |
| Rust memory-safety rewrites in the Windows kernel | Not implemented. |
| Prism Emulator for Arm64 application translation | Not implemented. |
| Enterprise Hotpatching (zero-reboot monthly security updates) | Not implemented. |
| Remote Desktop high-DPI scaling and zoom up to 500% | Not implemented. |
| Power Grid Forecast API | Not implemented. |
| Windows Protected Print Mode (driverless Mopria printing) | Not implemented. |
| Personal Data Encryption (PDE) for folder-level security | Not implemented. |
| Local Security Authority (LSA) protection enabled by default | Not implemented. |
| SHA-3 cryptographic support | Not implemented. |
| App Control for Business (WDAC enhancements) | Not implemented. |
| Windows LAPS automated local admin account management | Not implemented. |
| Wi-Fi network list privacy controls | Not implemented. |
| SMB over QUIC client controls & NTLM hardening | Not implemented. |

## Windows 11 (Version 25H2) Features

| Request | Status |
|---|---|
| Redesigned single-page scrollable Start Menu (unified Pinned, Recommended, and grid All Apps list) | Partial: paged app launcher, not the Windows Recommended feed. |
| Dynamic taskbar icon resizing | Not implemented. |
| Color-coded battery status icon with inline percentage toggle | Not implemented. |
| Quick Machine Recovery (QMR) cloud-connected WinRE recovery | Not implemented. |
| Administrator Protection (zero-trust standard user token elevation via Windows Hello) | Not implemented. |
| Task Manager RAM speed reporting in MT/s and faster process handle release | Partial: foreground/minimized app sessions; no Windows process or performance APIs. |
| Lock Screen widget customization (add, remove, rearrange status cards) | Not implemented. |
| Windows Share drag-and-drop top edge tray | Not implemented. |
| Third-party Passkey Manager integration into Windows Hello API | Not implemented. |
| Policy-based store app removal (RemoveDefaultMicrosoftStorePackages) | Not implemented. |
| Windows Backup for Organizations | Not implemented. |
| Wi-Fi 7 Enterprise access point support (WPA3-Enterprise / 802.1X) | Not implemented. |

## Scrapped, Removed, or Deprecated Features

| Request | Status |
|---|---|
| Windows Media Center | Not implemented. |
| Native DVD playback codecs in Windows Media Player | Not implemented. |
| Desktop Gadgets & Sidebar | Partial: native clock, dock and status bubbles. |
| Classic Windows 7 games (Solitaire, Minesweeper, Hearts) | Partial: original playable Minefield. No Solitaire or Hearts. |
| OneDrive smart file placeholders (1507 version) | Not implemented. |
| Built-in USB floppy drive plug-and-play drivers | Not implemented. |
| Home Edition manual update deferral controls | Not implemented. |
| Bing Food & Drink, Health & Fitness, and Travel apps | Not implemented. |
| WordPad | Partial: plain-text Writer, without rich text. |
| Cortana app | Not implemented. |
| Legacy Mail, Calendar, and People apps | Partial: local month Calendar; no Mail or People. |
| Windows Mixed Reality portal & SteamVR integration | Not implemented. |
| NTLMv1 authentication protocol | Not implemented. |
| AllJoyn IoT framework | Not implemented. |
| Steps Recorder | Not implemented. |
| Windows Speech Recognition | Not implemented. |
| DirectAccess, Computer Browser, Remote Mailslots, and WebDAV/WebClient service | Not implemented. |
| POPCNT and SSE4.2 CPU check enforcement (blocking older legacy CPUs) | Not implemented. |
| Windows PowerShell 2.0 | Not implemented. |
| WMIC (Windows Management Instrumentation Command-line) | Not implemented. |
| Location History API | Not implemented. |
| Windows Maps & Movies & TV (removed from clean 25H2 installs) | Not implemented. |
| Tips app | Not implemented. |
| 32-bit ARM (ARM32) UWP app support | Not implemented. |
| VBScript (disabled by default) | Not implemented. |
| TLS 1.0 & 1.1 protocols (disabled by default) | Not implemented. |
| Microsoft Defender Application Guard for Office | Not implemented. |
| Microsoft Support Diagnostic Tool (MSDT) | Not implemented. |

## Built-In Apps & Utilities

| Request | Status |
|---|---|
| Alarms & Clock | Partial: RTC-backed clock and calendar; no alarms. |
| App Installer | Not implemented. |
| Calculator | Partial: native integer four-function calculator. |
| Camera | Not implemented. |
| Character Map | Not implemented. |
| Clipchamp | Not implemented. |
| Component Services | Not implemented. |
| Computer Management | Not implemented. |
| Control Panel | Partial: unified Settings for implemented controls. |
| Defragment and Optimize Drives | Not implemented. |
| Device Manager | Not implemented. |
| Disk Cleanup (cleanmgr.exe) | Not implemented. |
| Disk Management | Not implemented. |
| Event Viewer | Not implemented. |
| Feedback Hub | Not implemented. |
| File Explorer | Partial: Files launches the session note and canvas; Home commands manage named records. |
| Films & TV (Movies & TV) | Not implemented. |
| Game Bar (Xbox Game Bar) | Not implemented. |
| Get Help | Not implemented. |
| iSCSI Initiator | Not implemented. |
| Journal (Microsoft Journal) | Not implemented. |
| Local Security Policy | Not implemented. |
| Magnifier | Not implemented. |
| Malicious Software Removal Tool (MRT) | Not implemented. |
| Media Player | Not implemented. |
| Microsoft 365 Hub | Not implemented. |
| Microsoft Family | Not implemented. |
| Microsoft Management Console (mmc.exe) | Not implemented. |
| Microsoft News (MSN News) | Not implemented. |
| Microsoft Solitaire Collection | Not implemented. |
| Microsoft Store | Not implemented. |
| Microsoft Teams | Not implemented. |
| Microsoft To Do | Not implemented. |
| Narrator | Not implemented. |
| Notepad | Partial: plain-text Writer with persistent named notes on Fresh Home. |
| ODBC Data Sources | Not implemented. |
| On-Screen Keyboard | Not implemented. |
| OneDrive | Not implemented. |
| OneNote | Not implemented. |
| Outlook for Windows (New) | Not implemented. |
| Paint | Partial: mouse drawing, palette, canvas persistence and wallpaper. |
| Performance Monitor | Not implemented. |
| Phone Link | Not implemented. |
| Power Automate | Not implemented. |
| PowerShell 5.1 | Not implemented. |
| Print Management | Not implemented. |
| Quick Assist | Not implemented. |
| Recovery Drive | Not implemented. |
| Registry Editor (regedit.exe) | Not implemented. |
| Remote Desktop Connection (mstsc.exe) | Not implemented. |
| Resource Monitor | Not implemented. |
| Services | Not implemented. |
| Snipping Tool | Not implemented. |
| Sound Recorder | Not implemented. |
| Sticky Notes | Not implemented. |
| System Configuration (msconfig.exe) | Not implemented. |
| System Information (msinfo32.exe) | Partial: native edition, boot mode and storage status. |
| Task Manager | Partial: foreground/minimized app sessions; no Windows process or performance APIs. |
| Task Scheduler | Not implemented. |
| Tips | Not implemented. |
| Weather (MSN Weather) | Not implemented. |
| Windows Fax and Scan | Not implemented. |
| Windows Features (OptionalFeatures.exe) | Not implemented. |
| Windows Media Player Legacy | Not implemented. |
| Windows Memory Diagnostic | Not implemented. |
| Windows Sandbox | Not implemented. |
| Xbox | Not implemented. |

## Other requirements in the conversation

| Area | Status |
|---|---|
| From-scratch kernel, BIOS/UEFI, three editions, media builder, Actions | Implemented; boot and installer tests are in `tools/smoke.py`. |
| Default GUI, CLI desktop, keyboard restoration, customization | Implemented, with hardware and session limits in README. |
| 8 GB media and low resource use | Exact capacity; most is a writable Home partition. BIOS smoke uses 32 MiB guest RAM. No comparison benchmark against Windows. |
| Hardware installer and live mode | Prepared IDE target only, opt-in, verified copy, disk-only boot. No disk-formatting wizard for arbitrary drives. |
| Windows migration and NTFS | Offline copy of user-selected host files into a raw Home image only; no NTFS driver or automated migration. |
| EXE installers, IMP compiler, arbitrary Linux apps | Partial: EXE→IMP1 linking and execution for supported PE32/PE32+ programs and selected Win32/CRT calls. No general EXE installer or Linux ABI. |
| Android apps | Removed by request. |
| Privilege tiers, registry, antivirus, security guards | Not implemented. No claims of process isolation or malware protection. |
| Universal drivers, VM guest additions and hardware masking | Not implemented. The VMware profile uses standard emulated IDE and PS/2, not guest tools. |
| Web, media, online drivers, store and network updates | Not implemented. CD and installer are offline. |
| Visual match | Supplied welcome artwork, sky, left rail, clock and launcher geometry. Interactive apps and text are original, not screenshot facsimiles of Chrome or Windows. |
