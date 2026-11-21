# In-guest CAB extraction test

This Win64 test client uses the upstream **libmspack CAB decoder** through
its public callback interface. It opens `F:\INSTALL.EXE`, searches the file
for embedded cabinets, and extracts the member `setup.exe` to the fixed path
`F:\STAGE\SETUP.EXE`. It is not Google's installer, not a generic MSI engine,
and does not itself register/install Chrome. The library source and linked
binary are not redistributed by this repository. Fetch the pinned source and
build on Linux with x86-64 MinGW-w64 GCC or `ZIG=/path/to/zig` (tested 0.13):

```
git clone https://github.com/kyz/libmspack /tmp/libmspack
git -C /tmp/libmspack checkout --detach 8f2a2a91ae2d12f985a196d481962ca4731f0f4d
python forge/media.py Lite
extras/cabstage/build.sh /tmp/libmspack /tmp/stage.exe build/Lite/pe64/KERNEL32.a
python qa/cabstage.py Lite /tmp/stage.exe
```

The default test builds a one-file cabinet containing `cal.exe`, extracts it
in FreshOS, independently verifies the output hash on disk, cold boots, and
launches the extracted EXE with arguments. To test the archived Chrome 85
inner installer, pass its local path and the reference SHA-256 of its `setup.exe`
as two additional arguments. That test checks extraction and attempts a cold-boot launch. A loader rejection
or nonzero setup exit fails the test. Even a successful setup process exit is
not a passing browser test. See `docs/chrome-acceptance.md`.
