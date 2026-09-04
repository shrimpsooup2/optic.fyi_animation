#!/usr/bin/env python3
"""Regenerates assets/*.svg and index.html from tools/optic.py.

    python3 tools/build.py

Nothing here is hand-edited art: the mark is arcs, the wordmark is outlined
from the vendored Instrument Sans, and the animation is the same geometry with
a mask over the lid so the crescent can be cut open over time.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import optic
from optic import BG, INK, MUTED, R, TEAL
from ttf import TTF

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAD = 18.0           # room for the pupil to swing outside the eyeball


def ink_bounds():
    """Tight ink box of the full lockup, with the eyeball centred at (R, R)."""
    top, right = optic.tips()
    dx, dy = optic.dot_centre()
    x0, y0 = 0.0, top[1]
    x1, y1 = right[0], 2 * R
    x0, y0 = min(x0, dx - optic.DOT_R), min(y0, dy - optic.DOT_R)
    x1, y1 = max(x1, dx + optic.DOT_R), max(y1, dy + optic.DOT_R)
    for path, xh, pen, trk in ((optic.BOLD, optic.XH, optic.PEN_X, optic.TRACK),
                               (optic.REG, optic.TAIL_XH, optic.TAIL_PEN, optic.TAIL_TRK)):
        font = TTF(path)
        s, x = xh / optic.FONT_XH, pen
        for ch in ('ptic' if path == optic.BOLD else '.fyi'):
            gid = font.cm[ord(ch)]
            cs = font.contours(gid)
            if cs:
                xs = [p[0] for c in cs for p in c]
                ys = [p[1] for c in cs for p in c]
                x0, x1 = min(x0, x + min(xs) * s), max(x1, x + max(xs) * s)
                y0 = min(y0, optic.BASELINE - max(ys) * s)
                y1 = max(y1, optic.BASELINE - min(ys) * s)
            x += font.advance(gid) * s + trk
    return x0, y0, x1, y1


def svg(body, vb, extra=''):
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="%s"%s>\n%s\n</svg>\n'
            % (vb, extra, body))


def mark_body(ox=0.0, oy=0.0):
    dx, dy = optic.dot_centre(ox, oy)
    return ('  <path fill="%s" d="%s"/>\n'
            '  <circle fill="%s" cx="%.3f" cy="%.3f" r="%.3f"/>'
            % (INK, optic.crescent_path(ox, oy), TEAL, dx, dy, optic.DOT_R))


def word_body(ox=0.0, oy=0.0, indent='  '):
    main, tail = optic.wordmark(ox, oy)
    return ('%s<g fill="%s">%s</g>\n%s<g fill="%s">%s</g>'
            % (indent, INK, ''.join('<path d="%s"/>' % d for _, d in main),
               indent, MUTED, ''.join('<path d="%s"/>' % d for _, d in tail)))


def write(rel, text):
    p = os.path.join(ROOT, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, 'w') as f:
        f.write(text)
    print('wrote %s (%d bytes)' % (rel, len(text)))


PAGE = r'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Optic - logo animation</title>
<style>
  :root{
    --bg:__BG__; --ink:__INK__; --teal:__TEAL__; --muted:__MUTED__;
    --ease-out:cubic-bezier(.22,1,.34,1);
    --ease-back:cubic-bezier(.34,1.42,.5,1);
  }
  *{box-sizing:border-box}
  html,body{height:100%}
  body{
    margin:0; background:var(--bg); color:var(--muted);
    font:14px/1.5 ui-sans-serif,system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
    display:flex; flex-direction:column; align-items:center; justify-content:center;
    gap:40px; padding:32px;
  }
  .stage{width:min(760px,92vw)}
  .logo{width:100%; height:auto; display:block; overflow:visible}

  /* Resting state is the finished logo, so the page degrades to a static
     lockup with no JS and under reduced-motion. */
  .eye,.open,.blink,.idle{transform-box:view-box; transform-origin:__CX__px __CY__px}
  .pupil{transform-box:fill-box; transform-origin:50% 50%}

  .play .open  {animation:eye-open .80s var(--ease-back) .15s both}
  .play .lid   {animation:lid-carve .78s var(--ease-out) .78s both}
  .play .pupil {animation:pupil-in .62s var(--ease-back) 1.00s both}
  .play .eye   {animation:glance .95s ease-in-out 1.75s both}
  .play .blink {animation:blink .30s ease-in-out 2.72s both}
  .play .idle  {animation:idle-blink 5.4s ease-in-out 4.60s infinite both}
  .play .word  {animation:word-slide .95s var(--ease-out) 2.92s both}
  .play .ltr   {animation:letter-in .70s var(--ease-out) both;
                animation-delay:calc(2.96s + var(--i) * .045s)}

  @keyframes eye-open{
    0%{transform:scaleY(.02)} 62%{transform:scaleY(1.06)} 100%{transform:scaleY(1)}
  }
  @keyframes lid-carve{
    0%{transform:translate(__SWEEP__px,-__SWEEP__px)} 100%{transform:translate(0,0)}
  }
  @keyframes pupil-in{
    0%{transform:scale(0)} 70%{transform:scale(1.14)} 100%{transform:scale(1)}
  }
  /* A saccade: dart, hold, dart back, settle. */
  @keyframes glance{
    0%,8%{transform:rotate(0deg)}
    28%,46%{transform:rotate(-27deg)}
    66%,80%{transform:rotate(13deg)}
    100%{transform:rotate(0deg)}
  }
  @keyframes blink{0%,100%{transform:scaleY(1)} 45%{transform:scaleY(.04)}}
  @keyframes idle-blink{
    0%,3%{transform:scaleY(1)} 5.5%{transform:scaleY(.05)} 8%,100%{transform:scaleY(1)}
  }
  @keyframes word-slide{0%{transform:translateX(-88px)} 100%{transform:translateX(0)}}
  @keyframes letter-in{
    0%{transform:translateX(-20px); opacity:0} 100%{transform:translateX(0); opacity:1}
  }

  .bar{display:flex; align-items:center; gap:18px}
  button{
    font:inherit; color:var(--ink); background:transparent;
    border:1px solid #2b323d; border-radius:999px; padding:9px 20px; cursor:pointer;
    transition:border-color .18s, background .18s;
  }
  button:hover{border-color:#465061; background:#1b2029}
  button:focus-visible{outline:2px solid var(--teal); outline-offset:3px}
  .hint{font-size:12.5px; color:#5d6774}

  @media (prefers-reduced-motion:reduce){
    .play .open,.play .lid,.play .pupil,.play .eye,
    .play .blink,.play .idle,.play .word,.play .ltr{animation:none}
  }
</style>
</head>
<body>
  <div class="stage" id="stage">
__SVG__
  </div>
  <div class="bar">
    <button type="button" id="replay">Replay</button>
    <span class="hint">the eye opens, looks around, blinks, then the name follows</span>
  </div>
<script>
  var stage = document.getElementById('stage');
  function play(){ stage.classList.remove('play'); void stage.offsetWidth; stage.classList.add('play'); }
  document.getElementById('replay').addEventListener('click', play);
  play();
</script>
</body>
</html>
'''


def build_animation(x0, y0, x1, y1):
    ox, oy = PAD - x0, PAD - y0
    w, h = (x1 - x0) + 2 * PAD, (y1 - y0) + 2 * PAD
    cx, cy = optic.centre(ox, oy)
    lx, ly = optic.cut_centre(ox, oy)
    dx, dy = optic.dot_centre(ox, oy)
    box = 'x="-500" y="-500" width="2200" height="1600"'
    main, tail = optic.wordmark(ox, oy)
    letters = []
    for i, (ch, d) in enumerate(main + tail):
        fill = INK if i < len(main) else MUTED
        letters.append('        <g class="ltr" style="--i:%d"><path fill="%s" d="%s"/></g>'
                       % (i, fill, d))
    body = (
        '    <svg class="logo" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %.2f %.2f"\n'
        '         role="img" aria-label="Optic.fyi">\n'
        '      <defs>\n'
        '        <!-- The lid is subtracted from the eyeball; sliding it in cuts the crescent. -->\n'
        '        <mask id="lid" maskUnits="userSpaceOnUse" %s>\n'
        '          <rect %s fill="#fff"/>\n'
        '          <circle class="lid" cx="%.3f" cy="%.3f" r="%.3f" fill="#000"/>\n'
        '        </mask>\n'
        '        <!-- Keeps the wordmark hidden while it is still behind the eyeball. -->\n'
        '        <mask id="behind" maskUnits="userSpaceOnUse" %s>\n'
        '          <rect %s fill="#fff"/>\n'
        '          <circle cx="%.3f" cy="%.3f" r="%.3f" fill="#000"/>\n'
        '        </mask>\n'
        '      </defs>\n'
        '      <g mask="url(#behind)">\n'
        '        <g class="word">\n%s\n        </g>\n'
        '      </g>\n'
        '      <g class="eye"><g class="open"><g class="blink"><g class="idle">\n'
        '        <circle class="ball" cx="%.3f" cy="%.3f" r="%.3f" fill="%s" mask="url(#lid)"/>\n'
        '        <circle class="pupil" cx="%.3f" cy="%.3f" r="%.3f" fill="%s"/>\n'
        '      </g></g></g></g>\n'
        '    </svg>'
        % (w, h, box, box, lx, ly, optic.CUT_R, box, box, cx, cy, R,
           '\n'.join(letters), cx, cy, R, INK, dx, dy, optic.DOT_R, TEAL))
    page = (PAGE.replace('__BG__', BG).replace('__INK__', INK).replace('__TEAL__', TEAL)
                .replace('__MUTED__', MUTED)
                .replace('__CX__', '%.3f' % cx).replace('__CY__', '%.3f' % cy)
                .replace('__SWEEP__', '132').replace('__SVG__', body))
    write('index.html', page)


def main():
    x0, y0, x1, y1 = ink_bounds()
    print('lockup ink: x[%.3f..%.3f] y[%.3f..%.3f]' % (x0, x1, y0, y1))

    write('assets/optic-icon.svg', svg(mark_body(), '0 0 200 200',
                                       ' width="200" height="200"'))
    ox, oy = -x0, -y0
    write('assets/optic-logo.svg',
          svg(mark_body(ox, oy) + '\n' + word_body(ox, oy),
              '0 0 %.2f %.2f' % (x1 - x0, y1 - y0)))
    build_animation(x0, y0, x1, y1)


if __name__ == '__main__':
    main()
