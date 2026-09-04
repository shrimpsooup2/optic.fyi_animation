#!/usr/bin/env python3
"""Minimal TrueType outline extractor -> SVG path data. No third-party deps."""
import struct, sys

class TTF:
    def __init__(self, path):
        self.d = open(path, 'rb').read()
        n = struct.unpack('>H', self.d[4:6])[0]
        self.tables = {}
        for i in range(n):
            o = 12 + i * 16
            tag = self.d[o:o+4].decode('latin-1')
            off, ln = struct.unpack('>II', self.d[o+8:o+16])
            self.tables[tag] = (off, ln)
        head = self.tables['head'][0]
        self.upem = struct.unpack('>H', self.d[head+18:head+20])[0]
        self.indexToLoc = struct.unpack('>h', self.d[head+50:head+52])[0]
        maxp = self.tables['maxp'][0]
        self.numGlyphs = struct.unpack('>H', self.d[maxp+4:maxp+6])[0]
        hhea = self.tables['hhea'][0]
        self.numHMetrics = struct.unpack('>H', self.d[hhea+34:hhea+36])[0]
        self._loca()
        self._cmap()

    def _loca(self):
        off, ln = self.tables['loca']
        n = self.numGlyphs + 1
        if self.indexToLoc == 0:
            self.loca = [v * 2 for v in struct.unpack('>%dH' % n, self.d[off:off+2*n])]
        else:
            self.loca = list(struct.unpack('>%dI' % n, self.d[off:off+4*n]))

    def _cmap(self):
        off = self.tables['cmap'][0]
        n = struct.unpack('>H', self.d[off+2:off+4])[0]
        best = None
        for i in range(n):
            pid, eid, so = struct.unpack('>HHI', self.d[off+4+i*8:off+12+i*8])
            fmt = struct.unpack('>H', self.d[off+so:off+so+2])[0]
            score = {(3, 10): 5, (3, 1): 4, (0, 4): 3, (0, 3): 3, (0, 6): 3}.get((pid, eid), 1)
            if fmt in (4, 12) and (best is None or score > best[0]):
                best = (score, off + so, fmt)
        _, so, fmt = best
        self.cm = {}
        if fmt == 4:
            segX2 = struct.unpack('>H', self.d[so+6:so+8])[0]
            seg = segX2 // 2
            base = so + 14
            end = struct.unpack('>%dH' % seg, self.d[base:base+segX2])
            base += segX2 + 2
            start = struct.unpack('>%dH' % seg, self.d[base:base+segX2])
            base += segX2
            delta = struct.unpack('>%dh' % seg, self.d[base:base+segX2])
            rbase = base + segX2
            rng = struct.unpack('>%dH' % seg, self.d[rbase:rbase+segX2])
            for i in range(seg):
                for c in range(start[i], min(end[i], 0xFFFF) + 1):
                    if rng[i] == 0:
                        g = (c + delta[i]) & 0xFFFF
                    else:
                        p = rbase + i * 2 + rng[i] + (c - start[i]) * 2
                        g = struct.unpack('>H', self.d[p:p+2])[0]
                        if g:
                            g = (g + delta[i]) & 0xFFFF
                    if g:
                        self.cm[c] = g
        else:
            ng = struct.unpack('>I', self.d[so+12:so+16])[0]
            for i in range(ng):
                s, e, gi = struct.unpack('>III', self.d[so+16+i*12:so+28+i*12])
                for c in range(s, e + 1):
                    self.cm[c] = gi + (c - s)

    def advance(self, gid):
        off = self.tables['hmtx'][0]
        i = min(gid, self.numHMetrics - 1)
        return struct.unpack('>H', self.d[off+i*4:off+i*4+2])[0]

    def contours(self, gid, depth=0):
        """Returns list of contours; each is a list of (x, y, on_curve)."""
        go, _ = self.tables['glyf']
        s, e = self.loca[gid], self.loca[gid+1]
        if s == e:
            return []
        p = go + s
        nc = struct.unpack('>h', self.d[p:p+2])[0]
        p += 10
        if nc < 0:
            return self._composite(p, depth)
        ends = struct.unpack('>%dH' % nc, self.d[p:p+2*nc])
        p += 2 * nc
        il = struct.unpack('>H', self.d[p:p+2])[0]
        p += 2 + il
        npts = ends[-1] + 1
        flags = []
        while len(flags) < npts:
            f = self.d[p]; p += 1
            flags.append(f)
            if f & 8:
                r = self.d[p]; p += 1
                flags.extend([f] * r)
        flags = flags[:npts]
        xs, v = [], 0
        for f in flags:
            if f & 2:
                dx = self.d[p]; p += 1
                v += dx if f & 16 else -dx
            elif not (f & 16):
                v += struct.unpack('>h', self.d[p:p+2])[0]; p += 2
            xs.append(v)
        ys, v = [], 0
        for f in flags:
            if f & 4:
                dy = self.d[p]; p += 1
                v += dy if f & 32 else -dy
            elif not (f & 32):
                v += struct.unpack('>h', self.d[p:p+2])[0]; p += 2
            ys.append(v)
        out, st = [], 0
        for en in ends:
            out.append([(xs[i], ys[i], bool(flags[i] & 1)) for i in range(st, en + 1)])
            st = en + 1
        return out

    def _composite(self, p, depth):
        if depth > 4:
            return []
        out = []
        while True:
            flags, gi = struct.unpack('>HH', self.d[p:p+4]); p += 4
            if flags & 1:
                a1, a2 = struct.unpack('>hh', self.d[p:p+4]); p += 4
            else:
                a1, a2 = struct.unpack('>bb', self.d[p:p+2]); p += 2
            sx = sy = 1.0; s01 = s10 = 0.0
            f2d = lambda v: v / 16384.0
            if flags & 8:
                sx = sy = f2d(struct.unpack('>h', self.d[p:p+2])[0]); p += 2
            elif flags & 0x40:
                sx = f2d(struct.unpack('>h', self.d[p:p+2])[0])
                sy = f2d(struct.unpack('>h', self.d[p+2:p+4])[0]); p += 4
            elif flags & 0x80:
                sx, s01, s10, sy = [f2d(v) for v in struct.unpack('>hhhh', self.d[p:p+8])]; p += 8
            dx, dy = (a1, a2) if flags & 2 else (0, 0)
            for c in self.contours(gi, depth + 1):
                out.append([(x*sx + y*s10 + dx, x*s01 + y*sy + dy, on) for x, y, on in c])
            if not (flags & 0x20):
                break
        return out


def glyph_path(font, ch, scale, ox, oy, prec=2):
    """SVG path data for one character. y is flipped (font y-up -> SVG y-down)."""
    gid = font.cm[ord(ch)]
    r = lambda v: round(v, prec)
    X = lambda x: r(ox + x * scale)
    Y = lambda y: r(oy - y * scale)
    parts = []
    for pts in font.contours(gid):
        if not pts:
            continue
        n = len(pts)
        start = next((i for i, p in enumerate(pts) if p[2]), None)
        if start is None:
            mx = (pts[0][0] + pts[-1][0]) / 2.0
            my = (pts[0][1] + pts[-1][1]) / 2.0
            pts = [(mx, my, True)] + pts
            start, n = 0, n + 1
        pts = pts[start:] + pts[:start]
        parts.append('M%s %s' % (X(pts[0][0]), Y(pts[0][1])))
        i = 1
        while i <= n:
            cur = pts[i % n]
            if cur[2]:
                parts.append('L%s %s' % (X(cur[0]), Y(cur[1])))
                i += 1
            else:
                nxt = pts[(i + 1) % n]
                if nxt[2]:
                    end = nxt; i += 2
                else:
                    end = ((cur[0] + nxt[0]) / 2.0, (cur[1] + nxt[1]) / 2.0, True); i += 1
                parts.append('Q%s %s %s %s' % (X(cur[0]), Y(cur[1]), X(end[0]), Y(end[1])))
        parts.append('Z')
    return ''.join(parts), font.advance(gid) * scale
