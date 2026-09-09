# Optic Eye

A small always-on-top eye that sits in the corner of the screen, follows the
cursor, blinks every few seconds, and slowly inverts itself to suit whatever is
behind it. Windows only.

![the eye rotating, drawing its pupil in, inverted on a light backdrop, and mid-blink](preview.png)

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

It starts top-left and is **click-through**, so it never gets in the way of
anything underneath. Right-click its tray icon to move it round the corners or
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
  eases toward the target rather than snapping.
- **Up close** the pupil stops behaving like a compass needle and draws inward
  instead, the way an eye focuses on something right in front of it. It only
  travels as far as `0.33R`, because the cut circle reaches `0.043R` past the
  centre and the pupil has to stay inside that notch — any further and it would
  break out onto the white and stop reading as the logo.
- **Light and dark** is sampled from a ring of twelve points just *outside* the
  widget, every 400ms, so it never reads back its own pixels. The ink fades
  between off-white and near-black across the middle of the brightness range,
  and the pupil deepens with it so it keeps its contrast on either ground. The
  fade has a time constant of about a second, so moving a white window under it
  is a slow turn rather than a flicker.
- **Blinking** is the same eyelid aperture: corners fixed out at the sides, an
  upper lid curve and a shallower lower one, scaled on Y about the corner line.
  Wide open it clears the pupil at *any* rotation, so the lids are invisible
  except during a blink.

`optic_eye_win32.c` is the shell around it: a layered, topmost, click-through
tool window fed by `UpdateLayeredWindow` with a premultiplied BGRA DIB, a 16ms
timer, and a tray icon rendered by the same rasteriser.
