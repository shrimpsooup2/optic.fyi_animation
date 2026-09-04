# optic.fyi — logo

The Optic mark as clean vector geometry, plus an HTML animation that builds the
logo out of the eye.

| file | what it is |
| --- | --- |
| [`index.html`](index.html) | the animation — open it in a browser, no build step, no dependencies |
| [`assets/optic-logo.svg`](assets/optic-logo.svg) | the full lockup (mark + `ptic.fyi`) |
| [`assets/optic-icon.svg`](assets/optic-icon.svg) | the mark on its own, `0 0 200 200` |
| [`reference/optic.fyi.png`](reference/optic.fyi.png) | the original raster everything was fitted to |

## The mark

Three circles on one 45° axis:

```
        eyeball   radius R, the "O"
        lid       centred R/√2 out, radius ¾R   -- subtracted, leaves the crescent
        pupil     centred 0.7574R out, radius 0.3696R
```

The crescent is a single path of two arcs — the long way round the eyeball, then
back along the lid — so the SVG has no clipping, no masks and no even-odd fill
tricks. The icon file is 270 bytes.

The wordmark is outlined from Instrument Sans (Bold for `ptic`, Regular for
`.fyi`), so nothing depends on a font being installed.

```
ink      #F2F4F5      pupil    #3DDBCB
muted    #9AA3B0      stage    #14181F
```

## The animation

The eye does the work; the name only arrives once it has settled.

| | |
| --- | --- |
| 0.15s | the eye opens — a closed slit widening into the full circle |
| 0.78s | the lid slides in from the upper right and cuts the crescent |
| 1.00s | the pupil dilates into the gap |
| 1.75s | a saccade — dart, hold, dart back, settle |
| 2.72s | a blink — lids close from top and bottom, leaving a white line |
| 2.92s | `ptic.fyi` slides out from behind the eyeball, letter by letter |
| 4.60s | idle blinks, every 5.4s |

Three masks do the work. The crescent is cut by a mask rather than drawn as a
path, which is what lets the lid animate: slide the mask's circle away and the
eyeball is whole again. A second mask, shaped like the eyeball, keeps the
wordmark hidden until it clears the mark's edge instead of appearing out of empty
space. The third is a pair of eyelids — black rects that rest shut and are drawn
apart to open the eye, so a blink is the lids closing over the shape rather than
the shape squashing. They meet a little below centre, where the crescent is at
full width, so a shut eye reads as one clean line.

Everything is CSS keyframes on nested groups — one transform per element, so
nothing fights over a property, and opening and blinking compose by adding back
what the other took away. The animation runs on its own with no class to apply,
so it cannot fail to start if a script does; JavaScript only handles Replay, and
`prefers-reduced-motion` skips to the finished logo.

## Rebuilding

```sh
python3 tools/build.py          # regenerate the SVGs and index.html
python3 tools/fit_reference.py  # re-derive the constants from the reference PNG
```

Pure standard library — no pip installs. `tools/optic.py` holds every constant;
change a number there and rebuild.

### Where the numbers came from

The reference was a raster, so the geometry was measured rather than guessed.
`fit_reference.py` finds each circle from sub-pixel edge crossings (the 0.5
coverage point between background and ink) and a least-squares circle fit, then
fits the wordmark by matching Instrument Sans glyph boxes to the measured ink
boxes.

The two icons in the reference are measured independently and land within
0.0002R of each other, and the two wordmark runs independently agree on the
baseline to 0.001 units. Rendered back over the original, the reconstruction
differs on 0.94% of pixels — all of it single-pixel antialiasing along edges.

Instrument Sans is a very close match for the original wordmark but may not be
the exact face: it agrees to 0.1% on the ascender-to-x-height ratio and ~1% on
letter widths. If you have the real font, drop it into `tools/fonts/` and point
`optic.py` at it.

## Licence

Instrument Sans is vendored under the SIL Open Font License — see
[`tools/fonts/OFL.txt`](tools/fonts/OFL.txt).
