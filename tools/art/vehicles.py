#!/usr/bin/env python3
# Copyright (C) 2026 SKIrf Labs. Part of SKIrf Wardriver, free software under the GNU GPL v3 or later (see ../../LICENSE).
# SPDX-License-Identifier: GPL-3.0-or-later
"""SKIrf vehicle sprites, drawn from scratch. All face RIGHT (front = high x).
Palette indices: 0 clear, 1-4 body ramp (light..dark, recoloured at runtime),
5 tyre, 6 rim, 7 hub/white, 8 glass, 9 glass glint, 10 headlight, 11 taillight,
12 trim, 13 shadow/outline, 14 chrome, 15 skin."""
import math
from PIL import Image, ImageDraw

PAL = [0x000000, 0x8CC8FF, 0x2C6EE6, 0x1C4AA0, 0x10285A, 0x121216, 0xA8AEB8, 0xE8ECF0,
       0x1E3550, 0x7FB4E0, 0xFFF2B0, 0xF02828, 0x3A3C44, 0x08080A, 0x8C909C, 0xE6B48C]

def new(w, h): return Image.new('L', (w, h), 0)

def wheel(d, cx, cy, r, frame, nframes, spokes=3):
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=5)
    rr = max(2, r - 2)
    d.ellipse((cx - rr, cy - rr, cx + rr, cy + rr), fill=6)
    ri = max(1, rr - 1)
    d.ellipse((cx - ri, cy - ri, cx + ri, cy + ri), fill=12)
    for k in range(spokes):
        a = (frame / nframes) * (2 * math.pi / spokes) + k * 2 * math.pi / spokes
        d.line((cx, cy, cx + round(ri * math.cos(a)), cy + round(ri * math.sin(a))), fill=6)
    d.point((cx, cy), fill=7)

def arch(d, cx, cy, r):   # dark wheel-well behind the tyre
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=13)

def shade_body(img, top, bottom):
    """Light top edge, darker lower third, darkest rocker: gives volume."""
    px = img.load(); w, h = img.size
    for x in range(w):
        col = [y for y in range(h) if px[x, y] in (1, 2, 3, 4)]
        if not col: continue
        y0 = col[0]
        for y in col:
            if px[x, y] not in (1, 2, 3, 4): continue
            if y == y0: px[x, y] = 1
            elif y >= bottom - 1: px[x, y] = 4
            elif y >= top + (bottom - top) * 0.62: px[x, y] = 3
            else: px[x, y] = 2

def car(kind, frame, nf=3):
    if kind == 'sedan':
        w, h = 80, 28; g = 25; wr, wf = 17, 63
        body = [(2, 15), (8, 13), (24, 12), (60, 12), (72, 14), (77, 16), (78, 20), (77, g - 2), (2, g - 2), (1, 19)]
        cabin = [(22, 12), (31, 4), (50, 4), (61, 12)]
        win = [[(26, 11), (32, 5), (39, 5), (39, 11)], [(42, 11), (42, 5), (49, 5), (56, 11)]]
        pillar = 40; hl = (75, 15, 77, 16); tl = (1, 15, 3, 17)
    elif kind == 'sports':
        w, h = 80, 26; g = 23; wr, wf = 17, 63
        body = [(2, 13), (10, 11), (24, 10), (56, 10), (68, 12), (76, 14), (78, 17), (77, g - 2), (2, g - 2), (1, 16)]
        cabin = [(22, 10), (34, 4), (46, 4), (60, 10)]           # fastback, cabin set back
        win = [[(28, 9), (35, 5), (40, 5), (40, 9)], [(42, 9), (42, 5), (46, 5), (55, 9)]]
        pillar = 41; hl = (74, 13, 77, 14); tl = (1, 13, 3, 15)
    elif kind == 'pickup':
        w, h = 82, 30; g = 27; wr, wf = 17, 65
        body = [(2, 14), (40, 14), (41, 13), (66, 13), (76, 15), (80, 17), (80, 21), (79, g - 2), (2, g - 2), (1, 18)]
        cabin = [(41, 13), (44, 4), (58, 4), (66, 13)]
        win = [[(46, 12), (47, 5), (57, 5), (62, 12)]]
        pillar = None; hl = (77, 16, 79, 17); tl = (1, 15, 3, 18)
    else:  # van / SUV
        w, h = 78, 32; g = 29; wr, wf = 16, 61
        body = [(2, 5), (6, 3), (54, 3), (66, 13), (74, 15), (77, 18), (77, 23), (76, g - 2), (2, g - 2), (1, 20)]
        cabin = None
        win = [[(6, 12), (6, 6), (18, 6), (18, 12)], [(21, 12), (21, 6), (33, 6), (33, 12)],
               [(36, 12), (36, 6), (48, 6), (48, 12)], [(51, 12), (51, 6), (55, 6), (62, 12)]]
        pillar = None; hl = (74, 16, 76, 17); tl = (1, 15, 3, 19)
    im = new(w, h); d = ImageDraw.Draw(im)
    d.line((8, h - 1, w - 8, h - 1), fill=13)               # ground shadow
    d.polygon(body, fill=2)
    if cabin: d.polygon(cabin, fill=2)
    shade_body(im, 3 if kind == 'van' else 4, g - 1)
    d = ImageDraw.Draw(im)
    for p in win: d.polygon(p, fill=8)
    for p in win:                                            # one diagonal glint per window
        xs = [q[0] for q in p]; ys = [q[1] for q in p]
        x0 = (min(xs) + max(xs)) // 2; d.line((x0, max(ys) - 1, x0 + 2, min(ys) + 1), fill=9)
    if pillar: d.line((pillar, 5, pillar, 11), fill=3)
    if kind == 'pickup':
        d.line((3, 14, 39, 14), fill=12); d.line((40, 14, 40, g - 3), fill=3)   # bed rail + bed/cab gap
    # door seams + handle
    seams = {'sedan': [40], 'sports': [41], 'pickup': [52], 'van': [34, 50]}[kind]
    for sx in seams: d.line((sx, 13 if kind != 'van' else 13, sx, g - 4), fill=3)
    for sx in seams: d.line((sx + 3, 15 if kind != 'van' else 15, sx + 5, 15 if kind != 'van' else 15), fill=12)
    # mirror at the base of the windshield
    mx = {'sedan': 56, 'sports': 55, 'pickup': 62, 'van': 61}[kind]
    d.rectangle((mx, 11 if kind != 'van' else 12, mx + 2, 12 if kind != 'van' else 13), fill=3)
    # bumpers
    d.rectangle((w - 4, g - 5, w - 2, g - 3), fill=12); d.rectangle((1, g - 5, 3, g - 3), fill=12)
    # lights: warm headlight with a soft beam pixel, red tail
    d.rectangle(hl, fill=10); d.point((hl[2] + 1, hl[1]), fill=10)
    d.rectangle(tl, fill=11)
    if kind == 'sports':
        d.line((2, 10, 7, 10), fill=4); d.line((4, 10, 4, 12), fill=4)       # small rear wing
    for cx in (wr, wf):
        arch(d, cx, g - 2, 8)
        wheel(d, cx, g - 2, 6, frame, nf)
    return im

def moto(frame, nf=3):
    w, h = 60, 34; im = new(w, h); d = ImageDraw.Draw(im)
    gy = 25; rx, fx = 12, 47
    d.line((4, h - 1, w - 4, h - 1), fill=13)
    # swingarm, fork, engine
    d.line((rx, gy, 28, 21), fill=14); d.line((rx, gy - 1, 28, 20), fill=12)
    d.line((fx, gy, 43, 12), fill=14); d.line((fx - 1, gy, 42, 12), fill=14)
    d.rectangle((25, 17, 35, 23), fill=12); d.rectangle((26, 18, 34, 20), fill=14)
    d.line((14, 23, 26, 23), fill=14)                                         # exhaust
    # bodywork: tail high at the back, tank, fairing nose at the front
    d.polygon([(6, 13), (14, 14), (24, 16), (26, 13), (38, 11), (46, 11), (51, 14), (53, 17), (47, 19), (38, 18), (30, 18), (20, 18), (10, 16)], fill=2)
    shade_body(im, 11, 19); d = ImageDraw.Draw(im)
    d.polygon([(42, 11), (46, 6), (48, 7), (46, 11)], fill=8)                 # windscreen
    d.rectangle((51, 14, 53, 15), fill=10); d.point((54, 14), fill=10)       # headlight
    d.rectangle((5, 13, 7, 14), fill=11)                                      # tail
    # rider: crouched, facing forward
    d.polygon([(20, 13), (26, 13), (36, 8), (38, 10), (30, 15), (24, 16)], fill=12)   # torso
    d.line((36, 9, 42, 12), fill=12); d.line((37, 10, 43, 12), fill=13)              # arms to bars
    d.polygon([(22, 15), (30, 18), (31, 21), (27, 22), (25, 18)], fill=13)          # leg
    d.ellipse((33, 1, 42, 9), fill=2); d.line((35, 2, 38, 1), fill=1)              # helmet
    d.polygon([(39, 4), (42, 4), (42, 7), (39, 7)], fill=13)                       # dark visor (front)
    d.point((41, 5), fill=9)
    for cx in (rx, fx):
        d.ellipse((cx - 8, gy - 8, cx + 8, gy + 8), outline=5, width=3)   # fat tyre ring
        d.ellipse((cx - 5, gy - 5, cx + 5, gy + 5), outline=14)          # rim lip
        for k in range(3):
            a = (frame / nf) * (2 * math.pi / 3) + k * 2 * math.pi / 3
            d.line((cx, gy, cx + round(5 * math.cos(a)), gy + round(5 * math.sin(a))), fill=6)
        d.ellipse((cx - 1, gy - 1, cx + 1, gy + 1), fill=6)
        d.ellipse((cx - 3, gy - 3, cx + 3, gy + 3), outline=12) if False else None
    return im

def cyclist(frame, nf=4):
    w, h = 48, 36; im = new(w, h); d = ImageDraw.Draw(im)
    gy = 27; rx, fx = 10, 37; bb = (21, 27)
    d.line((3, h - 1, w - 3, h - 1), fill=13)
    for cx in (rx, fx):
        d.ellipse((cx - 8, gy - 8, cx + 8, gy + 8), outline=5)
        d.ellipse((cx - 7, gy - 7, cx + 7, gy + 7), outline=12)
        for k in range(3):
            a = (frame / nf) * (2 * math.pi / 3) + k * 2 * math.pi / 3
            d.line((cx, gy, cx + round(6 * math.cos(a)), gy + round(6 * math.sin(a))), fill=14)
        d.point((cx, gy), fill=7)
    st, ht = (17, 16), (32, 15)                                   # seat tube top, head tube
    for a, b in [((rx, gy), bb), ((rx, gy), st), (st, bb), (st, ht), (ht, bb), (ht, (fx, gy))]:
        d.line((a, b), fill=2)
    d.line((15, 15, 19, 15), fill=13)                             # saddle
    d.line((32, 15, 34, 12), fill=12); d.line((34, 12, 36, 12), fill=12)   # stem + bar
    # pedalling legs
    ang = frame / nf * 2 * math.pi
    for side, sh in ((0, 12), (1, 13)):
        a = ang + side * math.pi
        foot = (bb[0] + round(4 * math.cos(a)), bb[1] + round(4 * math.sin(a)))
        hip = (17, 13)
        kx = (hip[0] + foot[0]) // 2 + 4; ky = (hip[1] + foot[1]) // 2 - 1
        d.line((hip, (kx, ky)), fill=sh); d.line(((kx, ky), foot), fill=sh)
    d.polygon([(15, 13), (19, 11), (28, 6), (30, 8), (21, 14)], fill=2)   # jersey (body colour), leaning in
    d.line((28, 7, 34, 12), fill=15)                                       # arm
    d.ellipse((27, 1, 33, 7), fill=15)                                     # head
    d.chord((26, 0, 34, 7), 180, 360, fill=4)                              # helmet
    d.point((33, 4), fill=13)                                              # eye
    return im

def emit(name, frames, out, recolor=True):
    w, h = frames[0].size; bpr = (w + 1) // 2
    data = []
    for im in frames:
        px = im.load()
        for y in range(h):
            for x in range(0, w, 2):
                a = px[x, y]; b = px[x + 1, y] if x + 1 < w else 0
                data.append((a << 4) | b)
    out.append('static const uint32_t SPR_PAL_%s[16] = {%s};' % (name, ','.join('0x%06X' % c for c in PAL)))
    out.append('static const uint8_t SPR_DAT_%s[%d] = {' % (name, len(data)))
    for i in range(0, len(data), 24): out.append('  ' + ','.join('0x%02X' % v for v in data[i:i + 24]) + ',')
    out.append('};')
    out.append('static const SkiSprite SPR_%s = { %d, %d, %d, %s, SPR_DAT_%s, SPR_PAL_%s };' % (name, w, h, len(frames), 'SPR_RECOLOR' if recolor else '0', name, name))

if __name__ == '__main__':
    import sys
    out = ['#pragma once', '// Copyright (C) 2026 SKIrf Labs. Part of SKIrf Wardriver, free software under the GNU GPL v3 or later (see ../../LICENSE).', '// SPDX-License-Identifier: GPL-3.0-or-later', '// SKIrf vehicle sprites (generated by tools/art/vehicles.py; do not edit by hand).', '// All face RIGHT; the sketch mirrors them for left-bound traffic.',
           '// 4-bit palette, 2 px per byte.',
           '// Indices 1-4 are the paint ramp, rebuilt at runtime from the body colour.',
           '#include <stdint.h>',
           'struct SkiSprite {', '  uint8_t w, h, frames, flags;', '  const uint8_t* data;', '  const uint32_t* pal;', '};',
           '#define SPR_RECOLOR 0x01']
    for k in ('sedan', 'sports', 'pickup', 'van'): emit(k.upper(), [car(k, f) for f in range(3)], out)
    emit('MOTO', [moto(f) for f in range(3)], out)
    emit('CYCLIST', [cyclist(f) for f in range(4)], out)
    open(sys.argv[1], 'w').write('\n'.join(out) + '\n')
    # preview
    rows = []
    allf = [[car(k, 0) for k in ('sedan', 'sports', 'pickup', 'van')], [moto(f) for f in range(3)], [cyclist(f) for f in range(4)]]
    body = (30, 110, 230)
    def col(i):
        if i in (1, 2, 3, 4):
            t = {1: .47, 2: 0, 3: -.35, 4: -.67}[i]
            return tuple(int(c + (255 - c) * t) if t > 0 else int(c * (1 + t)) for c in body)
        c = PAL[i]; return ((c >> 16) & 255, (c >> 8) & 255, c & 255)
    imgs = [im for r in allf for im in r]
    W = 4 * 90 * 3; H = 0
    sheet = Image.new('RGB', (1060, 3 * 40 * 3 + 20), (24, 26, 32))
    x = y = 6
    for r in allf:
        x = 6
        for im in r:
            rgb = Image.new('RGB', im.size, (24, 26, 32)); p = im.load(); q = rgb.load()
            for yy in range(im.size[1]):
                for xx in range(im.size[0]):
                    if p[xx, yy]: q[xx, yy] = col(p[xx, yy])
            rgb = rgb.resize((im.size[0] * 3 * 85 // 100, im.size[1] * 3 * 85 // 100), Image.NEAREST)
            sheet.paste(rgb, (x, y)); x += rgb.size[0] + 10
        y += 40 * 3
    sheet.save(sys.argv[2])
