#!/usr/bin/env python3
"""The space checker in Python: the eight rules of FINAL-space.md 9.2, written
again from the spec so it can cross check src/levelcheck.c.

    build/smoke --json > build/levels.json
    python3 -I tools/gapcheck.py build/levels.json [-v]

The levels come from the C tables (tools/smoke.c dumps them after levels_init
has applied every level's deltas), so nothing here can drift from levels.c.
Run it on every edit to levels.c; tools/accept.sh runs it too. Prints one line
per error (and per warning with -v) and exits 1 on any error. The same two
deliberate departures from the letter of 9.2 as levelcheck.c: prop discs are
errors only inside a solid or on an approach point, and RECT asks for 40
percent of the samples clear.
"""
import json
import math
import sys

GAP = 0.76
CX, CZB, CZF = 11.6, -6.6, 7.0


def gaps(a, b):
    return abs(a['x'] - b['x']) - a['hw'] - b['hw'], abs(a['z'] - b['z']) - a['hd'] - b['hd']


def dist(x, z, b):
    dx = max(0.0, abs(x - b['x']) - b['hw'])
    dz = max(0.0, abs(z - b['z']) - b['hd'])
    return math.hypot(dx, dz)


def seg_hits(a, b, box):
    t0, t1 = 0.0, 1.0
    for o, d, lo, hi in ((a['x'], b['x'] - a['x'], box['x'] - box['hw'], box['x'] + box['hw']),
                         (a['z'], b['z'] - a['z'], box['z'] - box['hd'], box['z'] + box['hd'])):
        if abs(d) < 1e-6:
            if o < lo or o > hi:
                return False
            continue
        p, q = sorted(((lo - o) / d, (hi - o) / d))
        t0, t1 = max(t0, p), min(t1, q)
        if t0 > t1:
            return False
    return True


def check(lv, verbose):
    e = lv['enums']
    errs, warns = [], []
    solids = [dict(s, name=s['label'], st=s['id'], kind=-1) for s in lv['st'] if s['present'] and not s['wall']]
    solids += [dict(o, name=f'OB{o["kind"]}', st=-1) for o in lv['ob']]
    discs = [dict(x=p['x'], z=p['z'], hw=0.3, hd=0.3, name='PROP', st=-1, kind=-2) for p in lv['props'] if p['disc']]
    allb = solids + discs

    def nested(a, b):
        if a['kind'] == e['OBK_FENCE'] and b['kind'] == e['OBK_FENCE']:
            return True
        s, o = (a, b) if a['st'] >= 0 else (b, a)
        return s['st'] >= 0 and o['kind'] == e['OBK_COUNTER'] and abs(s['x'] - o['x']) <= o['hw'] and abs(s['z'] - o['z']) <= o['hd']

    # 1: pairs
    for i, a in enumerate(allb):
        for j in range(i + 1, len(allb)):
            b = allb[j]
            prop = a['kind'] == -2 or b['kind'] == -2
            gx, gz = gaps(a, b)
            if gx <= 0.05 and gz <= 0.05:
                if gx < -0.05 and gz < -0.05 and not nested(a, b):
                    p, o = (a, b) if a['kind'] == -2 else (b, a)
                    stored = prop and abs(p['x'] - o['x']) + 0.3 <= o['hw'] + 0.01 and abs(p['z'] - o['z']) + 0.3 <= o['hd'] + 0.01
                    if not stored:
                        errs.append(f'{"PROP" if prop else "GAP"} overlap {a["name"]} and {b["name"]} ({gx:.2f}, {gz:.2f})')
                continue
            if gx < GAP and gz < GAP and math.hypot(max(gx, 0), max(gz, 0)) < GAP:
                if not any(seg_hits(a, b, c) for k, c in enumerate(allb) if k not in (i, j)):
                    (warns if prop else errs).append(f'{"NEAR" if prop else "GAP"} {a["name"]} and {b["name"]} gap {gx:.2f}, {gz:.2f}')
            elif not prop and ((gx < 0 and GAP <= gz < 1.0) or (gz < 0 and GAP <= gx < 1.0)):
                warns.append(f'NARROW {a["name"]} and {b["name"]} gap {gz if gx < 0 else gx:.2f}')

    # 2: wall strips
    for a in solids:
        strips = (((a['x'] - a['hw']) + CX, ((a['x'] - a['hw']) - CX) / 2, a['z'], 'WEST'),
                  (CX - (a['x'] + a['hw']), ((a['x'] + a['hw']) + CX) / 2, a['z'], 'EAST'),
                  ((a['z'] - a['hd']) - CZB, a['x'], ((a['z'] - a['hd']) + CZB) / 2, 'BACK'),
                  (CZF - (a['z'] + a['hd']), a['x'], ((a['z'] + a['hd']) + CZF) / 2, 'FRONT'))
        for d, mx, mz, wall in strips:
            if d <= 0.06 or d >= GAP:
                continue
            if wall == 'FRONT' and abs(a['x'] - lv['door_x']) < a['hw'] + lv['door_hw']:
                continue
            if any(o is not a and abs(mx - o['x']) <= o['hw'] + 0.05 and abs(mz - o['z']) <= o['hd'] + 0.05 for o in allb):
                continue
            errs.append(f'WALL {a["name"]} {d:.2f} from the {wall} clamp')

    # 3: approach points
    for s in lv['st']:
        if not s['present'] or s['face'] == e['FACE_NONE'] or s['wall']:
            continue
        f, m = s['face'], 0.55
        x, z = ((s['x'], s['z'] - s['hd'] - m) if f == e['FACE_NZ'] else (s['x'] + s['hw'] + m, s['z']) if f == e['FACE_PX']
                else (s['x'] - s['hw'] - m, s['z']) if f == e['FACE_NX'] else (s['x'], s['z'] + s['hd'] + m))
        me = dict(s, st=s['id'], kind=-1)
        ok = -CX <= x <= CX and CZB <= z <= 8.5 and all(
            b['st'] == s['id'] or nested(me, b) or dist(x, z, b) >= 0.38 for b in solids)
        if not ok:
            errs.append(f'APPROACH {s["label"]} at {x:.2f}, {z:.2f}')
        for d in discs:
            if dist(x, z, d) < 0.05:
                errs.append(f'PROP disc at {d["x"]:.2f}, {d["z"]:.2f} stands on the {s["label"]} approach')

    # 4: spawns
    for which, (ax, az), pitch in (('lobby', lv['lobby'], 1.1), ('start', lv['start'], 1.2)):
        for k in range(8):
            x, z = ax + (k % 4) * pitch, az + (k // 4) * 0.9
            hit = next((b for b in solids if dist(x, z, b) < 0.4), None)
            if hit:
                errs.append(f'SPAWN {which} {k} at {x:.2f}, {z:.2f} inside {hit["name"]}')

    # 5: rects, on a 20 x 10 grid of sample points (the C side uses an LCG; both ask 40 percent)
    def clear(x, z):
        for b in solids:
            if b['st'] >= 0:
                if dist(x, z, b) < 0.6:
                    return False
            elif abs(x - b['x']) - b['hw'] < 0.6 and abs(z - b['z']) - b['hd'] < 0.6:
                return False
        return True
    for key in ('spot_box', 'newbie_box', 'wander_box', 'doormat_box'):
        x0, z0, x1, z1 = lv[key]
        pts = [(x0 + (i + 0.5) / 20 * (x1 - x0), z0 + (j + 0.5) / 10 * (z1 - z0)) for i in range(20) for j in range(10)]
        ok = sum(clear(x, z) for x, z in pts)
        if ok < 80:
            errs.append(f'RECT {key} only {ok} of 200 clear')

    # 6, 7: floor and stencils
    tmap = [[e['T_WHITE']] * 24 for _ in range(41)]
    for kind, c0, r0, cw, rh, ring in lv['accents']:
        if c0 + cw > 41 or r0 + rh > 24 or not cw or not rh:
            errs.append(f'FLOOR accent at c{c0} r{r0} runs off the grid')
        if kind == e['T_CHECK'] and (cw, rh) != (2, 2):
            errs.append(f'FLOOR checker is {cw}x{rh}')
        for c in range(c0, min(c0 + cw, 41)):
            for r in range(r0, min(r0 + rh, 24)):
                edge = c in (c0, c0 + cw - 1) or r in (r0, r0 + rh - 1)
                if not ring or edge:
                    tmap[c][r] = kind
    for s in lv['stencils']:
        c, r = math.floor((s['x'] + 12.3) / 0.6), math.floor((s['z'] + 7.15) / 0.6)
        t = tmap[c][r] if 0 <= c < 41 and 0 <= r < 24 else -1
        if t not in (e['T_WHITE'], e['T_GREY']):
            errs.append(f'STENCIL {s["text"]} at c{c} r{r} sits on tile kind {t}')

    # 8: signs and props in the room
    for kind, items in (('sign', [s for s in lv['signs'] if not s['local']]), ('prop', lv['props'])):
        for s in items:
            if abs(s['x']) > 12.3 or not -7.15 <= s['z'] <= 8.5 or not 0 <= s['y'] <= 3.4:
                errs.append(f'PROP {kind} at {s["x"]:.2f}, {s["y"]:.2f}, {s["z"]:.2f} is outside the room')

    for line in errs:
        print(f'{lv["name"]} {line}')
    if verbose:
        for line in warns:
            print(f'{lv["name"]} {line}')
    print(f'gapcheck {lv["name"]}: {len(errs)}')
    return len(errs)


def main(argv):
    verbose = '-v' in argv
    paths = [a for a in argv if a != '-v'] or ['build/levels.json']
    total = sum(check(lv, verbose) for p in paths for lv in json.load(open(p)))
    print(f'gapcheck: {total} error(s)')
    return 1 if total else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
