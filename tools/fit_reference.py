#!/usr/bin/env python3
"""Re-derives the constants in optic.py by measuring reference/optic.fyi.png.

    python3 tools/fit_reference.py

The reference art is a raster, so every number in optic.py was fitted rather
than guessed. Circles are recovered from sub-pixel edge crossings (the 0.5
coverage point between background and ink) and a least-squares circle fit;
the wordmark is fitted by matching Instrument Sans glyph boxes to the measured
ink boxes. Both icons in the reference are measured independently and agree to
about a thousandth of a radius, which is what makes the fit trustworthy.
"""

import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from png import read_png
from ttf import TTF

HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(os.path.dirname(HERE), 'reference', 'optic.fyi.png')

BG, INK, TEAL = (0x14, 0x18, 0x1F), (0xF2, 0xF4, 0xF5), (0x3D, 0xDB, 0xCB)
VX = [INK[i] - BG[i] for i in range(3)]
VL = sum(v * v for v in VX)


def coverage(px):
    """Ink coverage 0..1, or None where the pixel is off the bg->ink line."""
    d = [px[i] - BG[i] for i in range(3)]
    t = sum(d[i] * VX[i] for i in range(3)) / VL
    if sum((d[i] - t * VX[i]) ** 2 for i in range(3)) > 900:
        return None
    return max(0.0, min(1.0, t))


def _solve3(A, b):
    M = [row[:] + [b[i]] for i, row in enumerate(A)]
    for i in range(3):
        p = max(range(i, 3), key=lambda r: abs(M[r][i]))
        M[i], M[p] = M[p], M[i]
        for r in range(3):
            if r != i and M[i][i]:
                f = M[r][i] / M[i][i]
                for c in range(i, 4):
                    M[r][c] -= f * M[i][c]
    return [M[i][3] / M[i][i] for i in range(3)]


def fit_circle(pts):
    """Kasa algebraic fit of x^2 + y^2 + Dx + Ey + F = 0."""
    A, b = [[0.0] * 3 for _ in range(3)], [0.0] * 3
    for x, y in pts:
        z, v = x * x + y * y, (x, y, 1.0)
        for i in range(3):
            for j in range(3):
                A[i][j] += v[i] * v[j]
            b[i] -= v[i] * z
    D, E, F = _solve3(A, b)
    cx, cy = -D / 2, -E / 2
    return cx, cy, math.sqrt(max(cx * cx + cy * cy - F, 0.0))


def trim(pts, tol=1.0, rounds=8):
    for _ in range(rounds):
        cx, cy, r = fit_circle(pts)
        keep = [p for p in pts if abs(math.hypot(p[0] - cx, p[1] - cy) - r) < tol]
        if len(keep) < 20 or len(keep) == len(pts):
            break
        pts = keep
    return fit_circle(pts)


def crossings(rows, h, x0, x1):
    """Sub-pixel ink edges, scanning rows then columns."""
    out = []
    for y in range(h):
        r = rows[y]
        cv = [coverage(r[x * 4:x * 4 + 3]) for x in range(x0, x1)]
        for i in range(1, len(cv)):
            a, b = cv[i - 1], cv[i]
            if a is not None and b is not None and ((a < 0.5 <= b) or (b < 0.5 <= a)):
                out.append((x0 + i - 1 + (0.5 - a) / (b - a), float(y)))
    for x in range(x0, x1):
        cv = [coverage(rows[y][x * 4:x * 4 + 3]) for y in range(h)]
        for i in range(1, len(cv)):
            a, b = cv[i - 1], cv[i]
            if a is not None and b is not None and ((a < 0.5 <= b) or (b < 0.5 <= a)):
                out.append((float(x), i - 1 + (0.5 - a) / (b - a)))
    return out


def eyeball(rows, h, x0, x1):
    """Fit the outer circle using only arcs the lid cannot have touched."""
    left, right = {}, {}
    for y in range(h):
        r = rows[y]
        cv = [coverage(r[x * 4:x * 4 + 3]) for x in range(x0, x1)]
        for i in range(1, len(cv)):
            a, b = cv[i - 1], cv[i]
            if a is not None and b is not None and a < 0.5 <= b:
                left[y] = x0 + i - 1 + (0.5 - a) / (b - a)
                break
        for i in range(len(cv) - 2, -1, -1):
            a, b = cv[i + 1], cv[i]
            if a is not None and b is not None and a < 0.5 <= b:
                right[y] = x0 + i + 1 - (0.5 - a) / (b - a)
                break
    ys = sorted(left)
    top, bot = min(ys), max(ys)
    mid = (top + bot) / 2.0
    pts = [(left[y], y) for y in ys if y > top + 0.12 * (bot - top)]
    pts += [(right[y], y) for y in ys if y > mid + 0.25 * (bot - mid)]
    return trim(pts)


def pupil(rows, h, x0, x1):
    pts = []
    for y in range(h):
        r = rows[y]
        xs = [x for x in range(x0, x1)
              if sum((r[x * 4 + i] - TEAL[i]) ** 2 for i in range(3)) < 2000]
        if xs:
            pts += [(min(xs) - 0.5, float(y)), (max(xs) + 0.5, float(y))]
    return trim(pts)


def icon(rows, h, x0, x1, label):
    ecx, ecy, er = eyeball(rows, h, x0, x1)
    pcx, pcy, pr = pupil(rows, h, x0, x1)
    inside = [p for p in crossings(rows, h, x0, x1)
              if math.hypot(p[0] - ecx, p[1] - ecy) < er - 2.0]
    lcx, lcy, lr = trim(inside)
    ld = math.hypot(lcx - ecx, lcy - ecy)
    pd = math.hypot(pcx - ecx, pcy - ecy)
    print('%-8s eyeball c=(%8.3f,%8.3f) R=%7.3f' % (label, ecx, ecy, er))
    print('         lid    d=%.5fR r=%.5fR axis=%6.2f deg' %
          (ld / er, lr / er, math.degrees(math.atan2(ecy - lcy, lcx - ecx))))
    print('         pupil  d=%.5fR r=%.5fR axis=%6.2f deg' %
          (pd / er, pr / er, math.degrees(math.atan2(ecy - pcy, pcx - ecx))))
    return ecx, ecy, er


def wordmark(cx, cy, r):
    """Fit scale, tracking, pen origin and baseline to the measured ink boxes."""
    k = 100.0 / r
    runs = (
        ('ptic', 'InstrumentSans-Bold.ttf',
         {'p': (170, 303), 't': (140, 267), 'i': (128, 264), 'c': (170, 266)},
         [('p', 'l', 574), ('t', 'r', 745), ('i', 'l', 755),
          ('i', 'r', 783), ('c', 'l', 792), ('c', 'r', 889)],
         (0.180, 0.0002, 120), (-12, 0.1, 160)),
        ('.fyi', 'InstrumentSans-Regular.ttf',
         {'.': (257, 266), 'f': (209, 265), 'y': (225, 280), 'i': (210, 265)},
         [('.', 'l', 899), ('.', 'r', 908), ('f', 'l', 916), ('f', 'r', 939),
          ('y', 'l', 944), ('y', 'r', 979), ('i', 'l', 987), ('i', 'r', 995)],
         (0.068, 0.0002, 140), (-6, 0.1, 140)),
    )
    for text, fname, ymeas, xmeas, srange, trange in runs:
        font = TTF(os.path.join(HERE, 'fonts', fname))
        g = {}
        for ch in text:
            cs = font.contours(font.cm[ord(ch)])
            xs = [p[0] for c in cs for p in c]
            ys = [p[1] for c in cs for p in c]
            g[ch] = (min(xs), max(xs), min(ys), max(ys), font.advance(font.cm[ord(ch)]))
        anchor = xmeas[0]
        best = None
        for si in range(srange[2]):
            s = srange[0] + srange[1] * si
            for ti in range(trange[2]):
                t = trange[0] + trange[1] * ti
                for bi in range(25):
                    base = 263.0 + 0.25 * bi
                    pen = anchor[2] - g[anchor[0]][0] * s
                    x, pos = pen, {}
                    for ch in text:
                        pos[ch] = x
                        x += g[ch][4] * s + t
                    err = 0.0
                    for ch, side, val in xmeas:
                        edge = g[ch][0] if side == 'l' else g[ch][1]
                        err += (pos[ch] + edge * s - val) ** 2
                    for ch, (ylo, yhi) in ymeas.items():
                        err += (base - g[ch][3] * s - ylo) ** 2
                        err += (base - g[ch][2] * s - (yhi + 1)) ** 2
                    if best is None or err < best[0]:
                        best = (err, s, t, pen, base)
        e, s, t, pen, base = best
        n = len(xmeas) + 2 * len(ymeas)
        print("'%s'  rms=%.2fpx -> x-height=%.3f  pen=%.3f  track=%+.3f  baseline=%.3f"
              % (text, (e / n) ** 0.5, 520 * s * k, 100 + (pen - cx) * k, t * k,
                 100 + (base - cy) * k))


def main():
    w, h, rows = read_png(REF)
    print('reference %dx%d' % (w, h))
    icon(rows, h, 0, 300, 'left')
    cx, cy, r = icon(rows, h, 300, 570, 'lockup')
    print()
    wordmark(cx, cy, r)


if __name__ == '__main__':
    main()
