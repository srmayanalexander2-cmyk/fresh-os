# Optional test console

The browser runs a local x86 emulator; no OS logic is implemented in JavaScript. Build the Normal release ISO first (or extract it into `out/`). With the `v86@0.5.473` npm package and the `bios/` directory from `copy/v86` commit `315b2b0b758703ef0e6cc7b4088d6068f83f3c20`, run:

```
python3 tools/preview/prepare.py /path/to/node_modules/v86/build /path/to/v86/bios
python3 -m http.server --bind 0.0.0.0 --directory build/preview 8080
```

This harness serves no disks other than the release downloads and the read-only optical portion of Normal. Installation testing requires an actual prepared IDE target; use `tools/smoke.py --install` instead. Emulator and firmware licenses are copied alongside their binaries.
