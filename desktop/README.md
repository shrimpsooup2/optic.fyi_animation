# Optic Eye

A small always-on-top eye that sits in the corner of the screen, follows the
cursor, and blinks every few seconds. Windows only.

![the eye at several gaze angles and mid-blink](preview.png)

## Getting the .exe

Push to this repo, or hit **Run workflow** on
[Build Optic Eye (Windows)](../../actions/workflows/desktop.yml), then download
`OpticEye.exe` from that run's **Artifacts**. No toolchain needed on your side.

Building it yourself, from a Developer Command Prompt:

```bat
cl /O2 /Fe:OpticEye.exe desktop\optic_eye_win32.c desktop\eye_render.c ^
   /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib shell32.lib
```

or with mingw:

```sh
x86_64-w64-mingw32-gcc -O2 -mwindows -o OpticEye.exe \
    desktop/optic_eye_win32.c desktop/eye_render.c -lgdi32 -lshell32 -lm
```

Single file, no installer, no runtime — about 100 KB.

## Using it

It starts bottom-right and is **click-through**, so it never gets in the way of
anything underneath. Right-click its tray icon to move it to another corner or
quit. Running it twice does nothing; the second copy exits.

## How it works

`eye_render.c` is a standalone software rasteriser with no platform headers, so
the geometry can be tested off-Windows. It carries the same constants as
`tools/optic.py` — eyeball, a lid circle of radius `¾R` subtracted from it, and
the pupil out on the 45° axis — and draws them by classifying 4×4 supersamples
per pixel into pupil, ink or nothing.

Two things fall out of that:

- **Gaze** is the whole mark rotated about its centre, exactly as the web
  animation's saccade works. The pupil rests on the 45° axis, so pointing it at
  the cursor is just a rotation, and the crescent stays a crescent. The angle
  eases toward the target rather than snapping, and the cursor is ignored within
  24px so the gaze doesn't spin when it passes over.
- **Blinking** is the same eyelid aperture: corners fixed out at the sides, an
  upper lid curve and a shallower lower one, scaled on Y about the corner line.
  Wide open it clears the pupil at *any* rotation, so the lids are invisible
  except during a blink.

`optic_eye_win32.c` is the shell around it: a layered, topmost, click-through
tool window fed by `UpdateLayeredWindow` with a premultiplied BGRA DIB, a 16ms
timer, and a tray icon rendered by the same rasteriser.
