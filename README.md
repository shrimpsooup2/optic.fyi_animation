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
| 0.15s | the eye opens, centred in the frame — a shut line widening into the full circle |
| 0.60s | the lid slides in from the upper right and cuts the crescent |
| 1.00s | the pupil dilates into the gap |
| 1.75s | a saccade — dart, hold, dart back, settle |
| 2.72s | one blink — lids curve shut from two fixed corners, down to a white line |
| 3.15s | `ptic.fyi` comes out of the mark and shoulders it aside into the lockup |

The whole sequence replays every 8s.

Three masks do the work. The crescent is cut by a mask rather than drawn as a
path, which is what lets the lid animate: slide the mask's circle away and the
eyeball is whole again. A second mask, shaped like the eyeball, keeps the
wordmark hidden until it clears the mark's edge instead of appearing out of empty
space. The third is the eye aperture, built the way an eye actually is: two corner
points fixed out at the sides, an upper lid curve between them and a shallower
lower one. Scaling that lens on Y about the corner line bows the curves apart
without the corners moving, so opening and blinking are one mechanism and the
shape is never distorted — the lids pass over it. The corner line sits `0.28R`
below centre, chosen because that is where the crescent is at full width and the
pupil is clear of it, so a shut eye reads as one clean line. Shut keeps a sliver
of scale rather than zero, since a zero-height lens has no area to draw.

Wide open the aperture parks fully clear of the mark, so outside a blink the
lids touch nothing: the resting frame matches the shipped SVG to 0.06% of
pixels. The mark starts centred and is carried into place by a `translateX` that
the wordmark's own reveal rides on top of, so the name reads as pushing it
aside; the eyeball-shaped mask travels with it, which is what keeps the letters
hidden until the mark has moved off them.

Everything is CSS keyframes on nested groups — one transform per element, so
nothing fights over a property, and opening and blinking compose by
multiplying. The animation runs on its own with no class to apply, so it cannot
fail to start if a script does; JavaScript only restarts it. Under
`prefers-reduced-motion` the loop is off, but pressing Replay is a direct
request for it and plays anyway.

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
