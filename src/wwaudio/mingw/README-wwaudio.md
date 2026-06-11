# WWAudio backends (Miles shim)

Renegade links against a **Miles (AIL) stub** instead of the proprietary MSS SDK.

## FAudio (default, recommended for Wine)

```bash
# third_party/faudio must exist (git clone https://github.com/FNA-XNA/FAudio.git)
meson setup build-mingw --reconfigure -Dwwaudio_backend=faudio
ninja -C build-mingw
```

- Meson builds **libFAudio.a** (static) via CMake subproject and links it into `renegade.exe` — no separate `FAudio.dll`.
- Under Wine, a loose `FAudio.dll` in the prefix is often a **stub** (`unimplemented function FAudioCreate`); static linking avoids that.

MP3 decode: `-Dwwaudio_mpg123=true` (default) bundles static libmpg123.

### mpg123 (in-memory MP3 → PCM)

API reference: `third_party/mpg123/src/include/mpg123.h` (also `fmt123.h` for `MPG123_ENC_*`).

Renegade streams (e.g. `menu.mp3`) load the file into RAM, then decode once to PCM for FAudio:

| API | Use |
|-----|-----|
| `mpg123_init` / `mpg123_new` | One handle per decode |
| `mpg123_replace_reader_handle` + `mpg123_open_handle` | Read MP3 bytes from memory (not `mpg123_open_feed` for full-buffer decode) |
| `mpg123_format_none` + `mpg123_format2(mh, 0, …)` | **Rate `0` means “any rate” only in `mpg123_format2`**, not `mpg123_format` |
| `mpg123_read` until `MPG123_DONE` | Output 16-bit PCM (`MPG123_ENC_SIGNED_16`) |
| `mpg123_close` / `mpg123_delete` | Always close before delete |

Avoid `mpg123_open_feed` + `mpg123_feed` for “whole file in memory”: feed mode is for incremental input and can return endless `MPG123_NEED_MORE` after EOF.

`menu.mp3` is ~740 KiB compressed → ~8 MiB mono PCM (44100 Hz); the FAudio backend upmixes mono to stereo (~16 MiB) so both speakers get menu music. First decode on the main thread inside `AIL_open_stream_by_sample` may hitch briefly under Wine.

### FAudio buffer loops (`faudio_mss.cpp`)

Westwood uses Miles loop counts: `0` = infinite, `1` = play once. The shim maps these to `FAudioBuffer::LoopCount` / `LoopLength`.

**Critical:** `LoopLength` (and `PlayBegin` / `PlayLength` when used) are in **sample frames**, not bytes. `AudioBytes` stays in bytes.

```c
/* Wrong — FAudio returns FAUDIO_E_INVALID_CALL (0x88960001) on large buffers */
ab.LoopLength = pcm_bytes;

/* Correct */
ab.LoopLength = pcm_bytes / wfx.nBlockAlign;  /* e.g. 16293888 / 4 = 4073472 for stereo 16-bit */
```

FAudio validates that `LoopBegin + LoopLength` lies inside the play region (see `FAudioSourceVoice_SubmitSourceBuffer` in FAudio). Treating bytes as samples makes `LoopLength` ~4× too large for stereo 16-bit, so `SubmitSourceBuffer` fails and menu music never starts (`AIL_open_stream_by_sample` returns `NULL`).

Implementation: `voice_pcm_samples()` in `voice_fill_buffer_loop()`.

**Other behaviour in the same path:**

| Topic | Behaviour |
|-------|-----------|
| Mono MP3/WAV | `voice_upmix_mono_pcm()` duplicates L→R before `FAudio_CreateSourceVoice` (mono source voices pan to left only). |
| Miles pan | `miles_pan_to_balance()` treats `0–127` as Miles pan; default sample pan `63` (center). |
| Long streams (`pcm_bytes > 500000`) | Set `loop_count = 0` (infinite) on first upload; `AIL_set_sample_loop_count` does not re-submit huge buffers (re-submit also needs correct `LoopLength`). |
| `AIL_start_sample` | If `BuffersQueued > 0`, only `FAudioSourceVoice_Start` — no second full-buffer submit. |
| `AIL_set_sample_ms_position(0)` | Re-submits PCM (`voice_resubmit`) — required for weapon fire `Seek(0)` before each shot. |
| `AIL_start_3D_sample` | Same restart path as `AIL_start_sample` (`voice_restart_playback`). |
| MP3 decode hang | Use `mpg123_replace_reader_handle` + `mpg123_open_handle`, not feed mode (see table above). |

### Test player (`mp3_faudio_player`)

Build (with `-Dwwaudio_backend=faudio -Dwwaudio_mpg123=true`):

```bash
ninja -C build-mingw src/wwaudio/tools/mp3_faudio_player.exe
wine build-mingw/src/wwaudio/tools/mp3_faudio_player.exe /path/to/menu.mp3
wine WWAudio/tools/mp3_faudio_player.exe menu.mp3 --loop --seconds 30
```

If this plays but in-game menu music does not, compare loop/volume/stream paths in `faudio_mss.cpp` (not mpg123 decode). Use `--loop` to exercise the same infinite-loop buffer flags as the menu.

**Regression check:** after building, open the main menu under Wine; `menu.mp3` should loop in both channels. UI WAVs should still play on hover/click.

## OpenAL Soft (legacy)

```bash
meson setup build-mingw --reconfigure -Dwwaudio_backend=openal
```

Requires `soft_oal.dll` / OpenAL32 router next to `renegade.exe` (see `README-openal.md`).

## Silent stub

```bash
meson setup build-mingw --reconfigure -Dwwaudio_backend=stub
```

No audio output (menus run without crashing).
