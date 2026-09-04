#!/usr/bin/env python3
"""Parametric definition of the Optic mark.

Every constant below was fitted to reference/optic.fyi.png by tools/fit_reference.py,
so this module is the single source of truth for the logo's geometry. Regenerate the
committed SVGs and the animation with:

    python3 tools/build.py
"""

import math
import os

from ttf import TTF, glyph_path

HERE = os.path.dirname(os.path.abspath(__file__))
BOLD = os.path.join(HERE, 'fonts', 'InstrumentSans-Bold.ttf')
REG = os.path.join(HERE, 'fonts', 'InstrumentSans-Regular.ttf')

# ---------------------------------------------------------------- palette
INK = '#F2F4F5'      # crescent and 'ptic'
TEAL = '#3DDBCB'     # pupil
MUTED = '#9AA3B0'    # '.fyi'
BG = '#14181F'       # stage behind the mark

# ---------------------------------------------------------------- the mark
# The eyeball is a unit circle of radius R. A second circle -- the lid --
# is subtracted from its upper right to leave the crescent, and the pupil
# sits in the gap that opens up. All three share one 45 degree axis.
R = 100.0
AXIS = math.radians(45.0)
CUT_D, CUT_R = 0.70760 * R, 0.75080 * R    # ~ R/sqrt2 out, ~ 3/4 R across
DOT_D, DOT_R = 0.75735 * R, 0.36960 * R

# ------------------------------------------------------------ the wordmark
# 'ptic' is Instrument Sans Bold, '.fyi' is the Regular weight; both are
# outlined so the SVGs carry no font dependency. Units match the mark, whose
# centre sits at (R, R) before any padding offset.
XH = 90.409          # x-height of 'ptic'
BASELINE = 195.333
PEN_X = 200.774      # pen origin of the 'p' -- lands on the eyeball's rim
TRACK = -5.833       # extra advance per letter (tight logotype fit)
TAIL_XH = 37.768     # x-height of '.fyi'
TAIL_PEN = 511.453
TAIL_TRK = 0.470
FONT_XH = 520.0      # x-height in font units, both weights


def centre(ox=0.0, oy=0.0):
    return R + ox, R + oy


def _polar(d, ox, oy):
    return R + ox + d * math.cos(AXIS), R + oy - d * math.sin(AXIS)


def cut_centre(ox=0.0, oy=0.0):
    return _polar(CUT_D, ox, oy)


def dot_centre(ox=0.0, oy=0.0):
    return _polar(DOT_D, ox, oy)


def tips(ox=0.0, oy=0.0):
    """Where the lid circle crosses the eyeball -- the crescent's two horns."""
    cx, cy = centre(ox, oy)
    bx, by = cut_centre(ox, oy)
    d = math.hypot(bx - cx, by - cy)
    a = (d * d - CUT_R ** 2 + R ** 2) / (2 * d)
    h = math.sqrt(max(R * R - a * a, 0.0))
    mx, my = cx + a * (bx - cx) / d, cy + a * (by - cy) / d
    ux, uy = (bx - cx) / d, (by - cy) / d
    return (mx + h * uy, my - h * ux), (mx - h * uy, my + h * ux)


def crescent_path(ox=0.0, oy=0.0, prec=3):
    """The eyeball minus the lid, as a single two-arc path."""
    top, right = tips(ox, oy)
    f = lambda v: round(v, prec)
    return ('M%s %sA%s %s 0 1 1 %s %sA%s %s 0 0 0 %s %sZ'
            % (f(right[0]), f(right[1]), f(R), f(R), f(top[0]), f(top[1]),
               f(CUT_R), f(CUT_R), f(right[0]), f(right[1])))


def _run(font_path, text, xh, pen, track, oy, prec=2):
    font = TTF(font_path)
    s = xh / FONT_XH
    out, x = [], pen
    for ch in text:
        d, adv = glyph_path(font, ch, s, x, BASELINE + oy, prec)
        out.append((ch, d))
        x += adv + track
    return out


def wordmark(ox=0.0, oy=0.0, prec=2):
    """Outlined 'ptic' and '.fyi' as [(char, path_data)] pairs."""
    main = _run(BOLD, 'ptic', XH, PEN_X + ox, TRACK, oy, prec)
    tail = _run(REG, '.fyi', TAIL_XH, TAIL_PEN + ox, TAIL_TRK, oy, prec)
    return main, tail
