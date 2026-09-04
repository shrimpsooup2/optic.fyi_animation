"""Minimal PNG reader -> RGBA pixel rows. No third-party deps."""
import struct, zlib

def read_png(path):
    d = open(path, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n', 'not a png'
    pos, idat, plte, trns = 8, [], None, None
    w = h = bd = ct = None
    while pos < len(d):
        ln, tag = struct.unpack('>I4s', d[pos:pos+8])
        body = d[pos+8:pos+8+ln]
        if tag == b'IHDR':
            w, h, bd, ct, _, _, il = struct.unpack('>IIBBBBB', body)
            assert il == 0, 'interlaced png unsupported'
        elif tag == b'PLTE': plte = body
        elif tag == b'tRNS': trns = body
        elif tag == b'IDAT': idat.append(body)
        elif tag == b'IEND': break
        pos += 12 + ln
    raw = zlib.decompress(b''.join(idat))
    nch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ct]
    assert bd == 8, 'only 8-bit depth supported'
    stride, bpp = w * nch, nch
    out, prev, p = [], bytearray(stride), 0
    for _ in range(h):
        f = raw[p]; p += 1
        line = bytearray(raw[p:p+stride]); p += stride
        if f == 1:
            for i in range(bpp, stride): line[i] = (line[i] + line[i-bpp]) & 255
        elif f == 2:
            for i in range(stride): line[i] = (line[i] + prev[i]) & 255
        elif f == 3:
            for i in range(stride):
                a = line[i-bpp] if i >= bpp else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 255
        elif f == 4:
            for i in range(stride):
                a = line[i-bpp] if i >= bpp else 0
                c = prev[i-bpp] if i >= bpp else 0
                b = prev[i]
                pa, pb, pc = abs(b-c), abs(a-c), abs(a+b-2*c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        out.append(line); prev = line
    # normalise every colour type to RGBA rows
    rows = []
    for line in out:
        r = bytearray()
        for i in range(w):
            if ct == 6:   px = line[i*4:i*4+4]
            elif ct == 2: px = bytes(line[i*3:i*3+3]) + b'\xff'
            elif ct == 0: v = line[i]; px = bytes([v, v, v, 255])
            elif ct == 4: v, a = line[i*2], line[i*2+1]; px = bytes([v, v, v, a])
            else:
                j = line[i]
                px = bytes(plte[j*3:j*3+3]) + bytes([trns[j] if trns and j < len(trns) else 255])
            r += px
        rows.append(bytes(r))
    return w, h, rows
