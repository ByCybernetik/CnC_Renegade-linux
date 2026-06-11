# Game library sources (`src/`)

Meson-built Renegade libraries (lowercase dirs). Build from repository root:

```bash
./meson-setup-mingw.sh build-mingw
ninja -C build-mingw src/commando/renegade.exe
```

Legacy VC6 trees, WOL, and tools remain under `Code/`. MinGW cross files: `build/mingw/`. Dependencies: `deps/`, `third_party/`.
