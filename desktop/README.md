# Optic Eye

A small always-on-top eye that sits in the corner of the screen, follows the
cursor, blinks every few seconds, and slowly inverts itself to suit whatever is
behind it. Windows only.

![the iris and pupil sliding around the eyeball, ending concentric when it looks straight at you](preview.png)

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

**Right-click it to send it to any of the four corners** — it shrinks away where
it is, jumps, and pops back in at the far end.

![shrinking away to nothing, then popping back in past full size](teleport.png)

Left-click makes it blink. Only the circle takes the mouse; the corners of its
box stay see-through, so clicking near it still reaches whatever is underneath.
It starts top-left. Running it twice does nothing; the second copy exits.

**To quit: `Ctrl+Alt+Q`**, or use its tray icon. The hotkey exists because the
window has no taskbar button, so if the tray icon were ever missing there would
be no way out of it at all.

## How it works

`eye_render.c` is a standalone software rasteriser with no platform headers, so
the geometry can be tested off-Windows. It carries the same constants as
`tools/optic.py` — eyeball, a lid circle of radius `¾R` subtracted from it, and
the pupil out on the 45° axis — and draws them by classifying 4×4 supersamples
per pixel into pupil, ink or nothing.

Two things fall out of that:

- **Gaze** slides the iris and pupil together as one assembly, anywhere inside
  the eyeball. The cut circle is not a fixed notch — it is a circle *around* the
  pupil, and its offset scales with the pupil's own, so the resting mark is
  exactly the logo and the two converge as the gaze comes inward. Look straight
  at it and you get a concentric eye: a ring of white, a dark iris, the pupil in
  the middle.
- The position itself is what eases, not an angle and a distance separately.
  That distinction matters: in polar terms the angle goes wild near the centre
  for no real movement, which made the whole mark spin rather than the pupil
  slide. Easing `(x, y)` has no such singularity.
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

It stays in front. `WS_EX_TOPMOST` only settles the order at the moment it is
set, so anything that raises its own topmost window afterwards lands above it
with no message to say so. It re-asserts the front every 1.5s, and forces
`hwndInsertAfter` back to `HWND_TOPMOST` on every `WM_WINDOWPOSCHANGING`.

`optic_eye_win32.c` is the shell around it: a layered, topmost tool window fed
by `UpdateLayeredWindow` with a premultiplied BGRA DIB, a 16ms timer, and a tray
icon rendered by the same rasteriser. `WM_NCHITTEST` reports everything outside
the circle as transparent, so the square window only catches clicks where
something is actually drawn.

The teleport scales the mark rather than sliding the window: it shrinks on a
squared curve, the window jumps the moment nothing is being drawn, and it
returns on an ease-out-back that overshoots to about 1.10. The eyeball is drawn
at `0.40` of the box rather than filling it, which is the headroom that
overshoot needs — the pupil already reaches `1.127R`, so at full size there was
none to spare.
