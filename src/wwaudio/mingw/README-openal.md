# WWAudio + OpenAL Soft (soft_oal.dll)

Сборка линкуется с **OpenAL32.dll** (import lib в `third_party/openal-prebuilt/lib/`).
В рантайме используется ваша **soft_oal.dll** (и при необходимости router **OpenAL32.dll**) рядом с `renegade.exe` — Meson их не ищет и не копирует.

```bash
cd Code
meson setup build-mingw \
  --cross-file wwlib/cross_mingw32.txt \
  --cross-file wwlib/cross_mingw32.paths.txt
ninja -C build-mingw Commando/lib/renegade.exe
```

## MP3 (mpg123)

По умолчанию (`-Dwwaudio_mpg123=true`) **libmpg123** собирается статически вместе с игрой (исходники в `third_party/mpg123/`). Отдельная DLL не нужна.

Отключить MP3: `meson setup build-mingw --reconfigure -Dwwaudio_mpg123=false` (останутся WAV/ADPCM).

## Отключить OpenAL

```bash
meson setup build-mingw --reconfigure -Dwwaudio_backend=openal
```
