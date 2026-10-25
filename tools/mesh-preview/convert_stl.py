#!/usr/bin/env python3
"""Standalone STL -> PHM display mesh. Python standard library only.

This changes no slicer/project/G-code file. Quantized coordinates and optional
vertex clustering create a display approximation, not replacement print geometry.
"""
from array import array
import argparse
import math
from pathlib import Path
import re
import struct
import tempfile
import os
import zlib

MAX_SOURCE = 2_000_000
MAX_VERTICES = 65535
MAX_TRIANGLES = 100000
HEADER = struct.Struct('<8s6I6fIIfIII')
assert HEADER.size == 80


def checked_vertex(values):
    point = tuple(float(v) for v in values)
    if len(point) != 3 or any(not math.isfinite(v) or abs(v) > 1e6 for v in point):
        raise ValueError('STL coordinates must be finite and within +/-1,000,000 mm')
    return point


def read_stl(path):
    size = path.stat().st_size
    if size > 256 * 1024 * 1024:
        raise ValueError('STL input exceeds 256 MiB')
    data = array('d')
    with path.open('rb') as source:
        head = source.read(84)
        count = struct.unpack_from('<I', head, 80)[0] if len(head) == 84 else 0
        if len(head) == 84 and size == 84 + count * 50:
            if not count or count > MAX_SOURCE:
                raise ValueError('STL facet count outside 1..2,000,000')
            for _ in range(count):
                row = source.read(50)
                xyz = struct.unpack('<12fH', row)
                for j in (3, 6, 9):
                    data.extend(checked_vertex(xyz[j:j+3]))
        else:
            source.seek(0)
            state = 'outside'
            vertices = 0
            for raw in iter(lambda: source.readline(1025), b''):
                if len(raw) > 1024:
                    raise ValueError('ASCII STL line exceeds 1024 bytes')
                line = raw.decode('ascii').strip()
                if not line:
                    continue
                words = line.split()
                word = words[0].lower()
                if state == 'outside' and word in ('solid', 'endsolid'):
                    continue
                if state == 'outside' and word == 'facet' and len(words) == 5 and words[1].lower() == 'normal':
                    checked_vertex(words[2:]); state = 'loop'
                elif state == 'loop' and line.lower() == 'outer loop':
                    vertices = 0; state = 'vertices'
                elif state == 'vertices' and word == 'vertex' and len(words) == 4 and vertices < 3:
                    data.extend(checked_vertex(words[1:])); vertices += 1
                    if vertices == 3:
                        state = 'endloop'
                elif state == 'endloop' and line.lower() == 'endloop':
                    state = 'endfacet'
                elif state == 'endfacet' and line.lower() == 'endfacet':
                    state = 'outside'
                else:
                    raise ValueError('Malformed ASCII STL facet structure')
                if len(data) // 9 > MAX_SOURCE:
                    raise ValueError('STL exceeds 2,000,000 facets')
            if state != 'outside' or not data:
                raise ValueError('Incomplete or empty ASCII STL')
    return data


def demo():
    data = array('d')
    def vertex(u, v):
        radius = 8 * (1 + .12 * math.cos(12 * u))
        return ((25 + radius * math.cos(v)) * math.cos(u),
                (25 + radius * math.cos(v)) * math.sin(u), 10 + radius * math.sin(v))
    for i in range(96):
        for j in range(32):
            a, b, c, d = (vertex(i*math.tau/96, j*math.tau/32),
                          vertex((i+1)*math.tau/96, j*math.tau/32),
                          vertex((i+1)*math.tau/96, (j+1)*math.tau/32),
                          vertex(i*math.tau/96, (j+1)*math.tau/32))
            for point in (*a, *b, *c, *a, *c, *d):
                data.append(point)
    return data


def bounds(data):
    return ([min(data[j::3]) for j in range(3)], [max(data[j::3]) for j in range(3)])


def build(data, lo, hi, cell, budget):
    """Grid clustering preserves connected triangles; never drop random faces."""
    span = [hi[j] - lo[j] for j in range(3)]
    positions, lookup, faces, seen = [], {}, [], set()
    for i in range(0, len(data), 9):
        ids = []
        for k in (0, 3, 6):
            p = data[i+k:i+k+3]
            if cell:
                p = [min(hi[j], max(lo[j], lo[j] + round((p[j]-lo[j])/cell)*cell)) for j in range(3)]
            q = tuple(round((p[j]-lo[j])*65535/span[j]) if span[j] else 0 for j in range(3))
            index = lookup.get(q)
            if index is None:
                index = len(positions); lookup[q] = index; positions.append(q)
            ids.append(index)
        if len(set(ids)) != 3:
            continue
        key = tuple(sorted(ids))
        if key in seen:
            continue
        seen.add(key); faces.append(tuple(ids))
        if len(faces) > budget or len(positions) > MAX_VERTICES:
            return None
    if not faces:
        return None
    # Crease-aware normal groups: retain sharp edges, smooth nearby facets.
    world = [tuple(lo[j] + p[j]*span[j]/65535 for j in range(3)) for p in positions]
    groups = [[] for _ in positions]
    vertices, normals, out = [], [], []
    cosine = math.cos(math.radians(45))
    for face in faces:
        a, b, c = (world[i] for i in face)
        u = [b[j]-a[j] for j in range(3)]; v = [c[j]-a[j] for j in range(3)]
        n = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
        length = math.sqrt(sum(x*x for x in n))
        if length < 1e-12:
            continue
        unit = tuple(x/length for x in n)
        tri = []
        for old in face:
            found = None
            for idx, reference in groups[old]:
                if sum(unit[j]*reference[j] for j in range(3)) >= cosine:
                    found = idx; break
            if found is None:
                found = len(vertices)
                if found == MAX_VERTICES:
                    return None
                vertices.append(positions[old]); normals.append([0., 0., 0.]); groups[old].append((found, unit))
            for j in range(3):
                normals[found][j] += n[j]
            tri.append(found)
        out.append(tuple(tri))
    if not out:
        return None
    return vertices, normals, out


def encode(data, budget=20000, allow_simplify=True):
    if not data or len(data) % 9 or len(data)//9 > MAX_SOURCE:
        raise ValueError('Invalid triangle data')
    lo, hi = bounds(data)
    if any(not math.isfinite(x) or abs(x) > 1e6 for x in (*lo, *hi)):
        raise ValueError('Coordinates outside preview bounds')
    result, cell = build(data, lo, hi, 0., budget), 0.
    if result is None:
        if not allow_simplify:
            raise ValueError("Mesh exceeds the triangle/vertex budget; increase --triangles or allow simplification")
        low, high = 0., max(hi[j]-lo[j] for j in range(3))/1024
        for _ in range(16):
            result = build(data, lo, hi, high, budget)
            if result is not None:
                break
            low, high = high, high*2
        else:
            raise ValueError('Cannot produce a nonempty mesh within the vertex/triangle budget')
        cell = high
        for _ in range(8):
            mid = (low+high)/2; candidate = build(data, lo, hi, mid, budget)
            if candidate is None:
                low = mid
            else:
                result, cell, high = candidate, mid, mid
    vertices, normals, faces = result
    payload = bytearray()
    for p, normal in zip(vertices, normals):
        length = math.sqrt(sum(x*x for x in normal))
        if not length:
            raise ValueError('Zero accumulated normal')
        qn = tuple(max(-32767, min(32767, round(x/length*32767))) for x in normal)
        payload += struct.pack('<3H3h', *p, *qn)
    for face in faces:
        payload += struct.pack('<3H', *face)
    header = bytearray(HEADER.pack(b'PHMESH1\0', 1, 80, len(vertices), len(faces), 1, len(payload),
                                  *lo, *hi, zlib.crc32(payload), len(data)//9, cell, 0, 0, 0))
    struct.pack_into('<I', header, 68, zlib.crc32(header))
    return bytes(header + payload), {'source_triangles': len(data)//9, 'triangles': len(faces),
                                     'vertices': len(vertices), 'cell_mm': cell,
                                     'quantization_mm': max(hi[j]-lo[j] for j in range(3))/65535}


def write_output(path, blob, force=False):
    if not re.fullmatch(r'[A-Za-z0-9_ -][A-Za-z0-9_. -]{0,75}\.phm', path.name):
        raise ValueError('Use a short ASCII filename ending in .phm (letters/numbers/spaces/_/-/.)')
    if path.exists() and not force:
        raise ValueError(f'{path} exists; use --force to replace it')
    fd, temp = tempfile.mkstemp(prefix='.phm-', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as output:
            output.write(blob); output.flush(); os.fsync(output.fileno())
        os.replace(temp, path)
    finally:
        if os.path.exists(temp):
            os.unlink(temp)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path, nargs='?', help='Binary or ASCII STL, coordinates interpreted as mm')
    parser.add_argument('output', type=Path, nargs='?', help='Default: input name with .phm extension')
    parser.add_argument('--demo', type=Path, metavar='OUTPUT.phm', help='Generate a 6,144-triangle fluted ring test mesh')
    parser.add_argument('--triangles', type=int, default=20000, help='Display triangle budget, 100..100000 (default 20000)')
    parser.add_argument('--force', action='store_true')
    parser.add_argument('--no-simplify', action='store_true', help='Reject meshes that exceed the budget instead of grid simplification')
    args = parser.parse_args()
    try:
        if not 100 <= args.triangles <= MAX_TRIANGLES:
            raise ValueError('--triangles must be 100..100000')
        if args.demo:
            if args.input or args.output:
                raise ValueError('--demo cannot be combined with an input/output pair')
            data, output = demo(), args.demo
        else:
            if args.input is None:
                raise ValueError('Supply an STL input or --demo OUTPUT.phm')
            output = args.output or args.input.with_suffix('.phm')
            if output.resolve() == args.input.resolve():
                raise ValueError('Output must differ from the source STL')
            data = read_stl(args.input)
        blob, info = encode(data, args.triangles, not args.no_simplify)
        write_output(output, blob, args.force)
        print(f'{output}: {info["triangles"]:,} triangles, {info["vertices"]:,} vertices, {len(blob):,} bytes')
        print(f'Source: {info["source_triangles"]:,} facets; clustering grid {info["cell_mm"]:.5g} mm; coordinate step <= {info["quantization_mm"]:.5g} mm')
        print('Display mesh only. Copy into /PrinterHMI/meshes/ on the tablet SD card. Source STL/G-code unchanged.')
    except (OSError, UnicodeError, ValueError, struct.error) as error:
        parser.exit(1, f'Error: {error}\n')


if __name__ == '__main__':
    main()
