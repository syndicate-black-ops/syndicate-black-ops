#!/usr/bin/env python3
"""Test input for tests/spray_anim: writes a test GIF (moving square on a transparent background, a frame with its
own color table, an interlaced frame, disposal 2), converts it with scripts/gif_to_spray.py and writes the frames the
client must show (indices, frame after frame) next to the spray.
    make_case.py <out dir>
"""
import importlib.util
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
spec = importlib.util.spec_from_file_location('gif_to_spray', os.path.join(ROOT, 'scripts', 'gif_to_spray.py'))
g2s = importlib.util.module_from_spec(spec)
spec.loader.exec_module(g2s)


def lzw_encode(indices, min_code):
    """A GIF LZW encoder (dictionary codes, code size growth, a clear code when the table is full)."""
    clear = 1 << min_code
    size = min_code + 1
    table = {bytes([i]): i for i in range(clear)}
    next_code = clear + 2
    out = bytearray()
    state = [0, 0]

    def emit(code):
        state[0] |= code << state[1]
        state[1] += size
        while state[1] >= 8:
            out.append(state[0] & 255)
            state[0] >>= 8
            state[1] -= 8

    emit(clear)
    cur = b''
    for i in indices:
        nxt = cur + bytes([i])
        if nxt in table:
            cur = nxt
            continue
        emit(table[cur])
        if next_code < 4096:
            table[nxt] = next_code
            next_code += 1
            if next_code > (1 << size) and size < 12:
                size += 1
        else:
            emit(clear)
            table = {bytes([k]): k for k in range(clear)}
            next_code = clear + 2
            size = min_code + 1
        cur = bytes([i])
    if cur:
        emit(table[cur])
    emit(clear + 1)
    if state[1]:
        out.append(state[0] & 255)
    return bytes(out)


def sub_blocks(data):
    out = bytearray()
    for i in range(0, len(data), 255):
        out.append(len(data[i:i + 255]))
        out += data[i:i + 255]
    return bytes(out) + b'\0'


def interlace_rows(h):
    return list(range(0, h, 8)) + list(range(4, h, 8)) + list(range(2, h, 4)) + list(range(1, h, 2))


def write_gif(path, w, h, frames):
    """frames: (pixels w*h indices into the 16-color table, delay cs, local table or None, interlaced, disposal)"""
    table = bytes(c for i in range(16) for c in ((i * 16) % 256, (i * 40) % 256, 255 - i * 15))
    out = bytearray(b'GIF89a' + struct.pack('<HHBBB', w, h, 0x80 | 3, 0, 0) + table)
    for pixels, delay, local, interlaced, disposal in frames:
        out += b'\x21\xF9\x04' + struct.pack('<BHB', (disposal << 2) | 1, delay, 0) + b'\0'
        flags = (0x80 | 3 if local else 0) | (0x40 if interlaced else 0)
        out += b'\x2C' + struct.pack('<HHHHB', 0, 0, w, h, flags)
        if local:
            out += local
        rows = interlace_rows(h) if interlaced else range(h)
        data = bytes(pixels[y * w + x] for y in rows for x in range(w))
        out += bytes([4]) + sub_blocks(lzw_encode(data, 4))
    out += b'\x3B'
    open(path, 'wb').write(out)


def main():
    out = sys.argv[1]
    w, h = 48, 40
    frames = []
    for k in range(12):
        pixels = bytearray(w * h)	# index 0 is the GIF's transparent color
        for y in range(10, 22):
            for x in range(2 + k * 3, 14 + k * 3):
                pixels[y * w + x] = 1 + (k % 15)
        for x in range(w):
            pixels[(h - 1) * w + x] = 5	# a still line
        local = None
        if k == 4:
            local = bytes(c for i in range(16) for c in (255 - i * 10, i * 9, i * 5))
        frames.append((pixels, 5 + k, local, k == 7, 2 if k == 9 else 1))
    gif = os.path.join(out, 'test.gif')
    write_gif(gif, w, h, frames)

    wad, n, dropped, total = g2s.make_spray(gif, (64, 64), 64)
    open(os.path.join(out, 'test.wad'), 'wb').write(wad)

    # the frames as the converter made them, before its encoding
    sw, sh, gif_frames = g2s.read_gif(gif)
    scaled = [g2s.fit(rgba, sw, sh, 64, 64) for rgba, _ in gif_frames]
    palette, idx = g2s.quantize(scaled)
    keep = [idx[0]] + [idx[i] for i in range(1, len(idx)) if idx[i] != idx[i - 1]]
    assert len(keep) == n and not dropped, (len(keep), n, dropped)
    open(os.path.join(out, 'test.frames'), 'wb').write(b''.join(keep))
    print('test spray: %d frames of %d, %d bytes' % (n, total, len(wad)))

    # a still spray (one frame) and an oversized animation (frames left out)
    write_gif(os.path.join(out, 'still.gif'), w, h, frames[:1])
    open(os.path.join(out, 'still.wad'), 'wb').write(g2s.make_spray(os.path.join(out, 'still.gif'), (64, 64), 64)[0])
    noisy = []
    for k in range(40):
        noisy.append((bytearray((x * 7 + y * 13 + k * 31 + (x * y * k) % 11) % 15 + 1 for y in range(h) for x in range(w)),
                      4, None, False, 1))
    write_gif(os.path.join(out, 'big.gif'), w, h, noisy)
    wad, n, dropped, total = g2s.make_spray(os.path.join(out, 'big.gif'), (64, 64), 64)
    assert len(wad) <= g2s.MAX_FILE and dropped > 0, (len(wad), dropped)
    open(os.path.join(out, 'big.wad'), 'wb').write(wad)
    print('big spray: %d frames of %d, %d bytes' % (n, total, len(wad)))


if __name__ == '__main__':
    main()
