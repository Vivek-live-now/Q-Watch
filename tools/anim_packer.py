#!/usr/bin/env python3
"""
007 Q-Watch Tactical Animation & Media Suite
=============================================
A comprehensive CLI utility for the ESP32-S3 Q-Watch:
- Converts animated GIFs, BMPs, and image sequences into .anim binary files.
- High-grade monochrome dithering: Atkinson, Floyd-Steinberg, Bayer, Threshold.
- Standalone pure-Python GIF87a/GIF89a & BMP decoders (zero required dependencies).
- Optional Pillow integration when installed for PNG/JPEG/WEBP.
- Full-terminal Unicode half-block animated player (128x64 on 128x32 console).
- Generates 128x64 1-bit boot splashes (/boot/boot.bmp).
- Procedural tactical generators: Radar, Gunbarrel, Matrix Rain, ECG Pulse, Lock-On.
- Extracts .anim frames to standard 1-bit BMP or XBM format.
"""

import os
import sys
import time
import math
import struct
import argparse
import glob

# Try optional Pillow
try:
    from PIL import Image as PILImage
    HAS_PILLOW = True
except ImportError:
    HAS_PILLOW = False

# ==============================================================================
# .anim Format Constants (QANM Spec v1)
# ==============================================================================
ANIM_MAGIC = 0x4D4E4151  # "QANM"
ANIM_VERSION = 1
ANIM_WIDTH = 128
ANIM_HEIGHT = 64
ANIM_FRAME_BYTES = 1024  # (128 * 64) // 8

ANIM_FLAG_LOOP = 0x0001
ANIM_FLAG_INVERT = 0x0002

# ==============================================================================
# Pure Python GIF87a / GIF89a LZW Decoder
# ==============================================================================
class GifDecoder:
    """Lightweight pure-Python GIF87a/GIF89a parser and frame extractor."""
    def __init__(self, data):
        self.data = data
        self.pos = 0

    def read(self, n):
        res = self.data[self.pos : self.pos + n]
        self.pos += n
        return res

    def read_byte(self):
        b = self.data[self.pos]
        self.pos += 1
        return b

    def decode(self):
        sig = self.read(6)
        if sig not in (b'GIF87a', b'GIF89a'):
            raise ValueError(f"Invalid GIF signature: {sig}")

        canvas_w, canvas_h, packed, bg_idx, aspect = struct.unpack('<HHBBB', self.read(7))
        has_gct = bool(packed & 0x80)
        gct_size = 1 << ((packed & 0x07) + 1)
        gct = []
        if has_gct:
            for _ in range(gct_size):
                gct.append(struct.unpack('BBB', self.read(3)))

        frames = []
        last_gce = {'delay_ms': 50, 'trans_idx': None, 'disposal': 0}
        canvas = [[(0, 0, 0)] * canvas_w for _ in range(canvas_h)]

        while self.pos < len(self.data):
            b = self.read_byte()
            if b == 0x3B:  # Trailer
                break
            elif b == 0x21:  # Extension
                ext_type = self.read_byte()
                if ext_type == 0xF9:  # Graphic Control Extension
                    sz = self.read_byte()
                    gce_bytes = self.read(sz)
                    gce_packed = gce_bytes[0]
                    delay_10ms = struct.unpack('<H', gce_bytes[1:3])[0]
                    trans_flag = bool(gce_packed & 0x01)
                    disposal = (gce_packed >> 2) & 0x07
                    delay_ms = delay_10ms * 10
                    if delay_ms < 15:
                        delay_ms = 50
                    trans_idx = gce_bytes[3] if trans_flag else None
                    last_gce = {'delay_ms': delay_ms, 'trans_idx': trans_idx, 'disposal': disposal}
                    self.read_byte()  # Terminator 0x00
                else:
                    # Skip unknown extension
                    while True:
                        sz = self.read_byte()
                        if sz == 0:
                            break
                        self.read(sz)
            elif b == 0x2C:  # Image Descriptor
                left, top, iw, ih, ipacked = struct.unpack('<HHHHB', self.read(9))
                has_lct = bool(ipacked & 0x80)
                interlaced = bool(ipacked & 0x40)
                if has_lct:
                    lct_size = 1 << ((ipacked & 0x07) + 1)
                    palette = [struct.unpack('BBB', self.read(3)) for _ in range(lct_size)]
                else:
                    palette = gct

                min_code_size = self.read_byte()
                comp_data = bytearray()
                while True:
                    sz = self.read_byte()
                    if sz == 0:
                        break
                    comp_data.extend(self.read(sz))

                # LZW Decompress
                idx_stream = self._decompress_lzw(comp_data, min_code_size, iw * ih)

                # Un-interlace if necessary
                if interlaced:
                    idx_stream = self._uninterlace(idx_stream, iw, ih)

                # Composite into canvas
                frame_canvas = [row[:] for row in canvas]
                trans_idx = last_gce.get('trans_idx')

                for y in range(ih):
                    cy = top + y
                    if cy >= canvas_h:
                        break
                    for x in range(iw):
                        cx = left + x
                        if cx >= canvas_w:
                            break
                        px_idx = y * iw + x
                        if px_idx < len(idx_stream):
                            c_idx = idx_stream[px_idx]
                            if c_idx != trans_idx and c_idx < len(palette):
                                frame_canvas[cy][cx] = palette[c_idx]

                frames.append((last_gce['delay_ms'], canvas_w, canvas_h, frame_canvas))

                # Handle disposal
                if last_gce['disposal'] != 2:
                    canvas = [row[:] for row in frame_canvas]
                last_gce = {'delay_ms': 50, 'trans_idx': None, 'disposal': 0}

        return frames

    def _decompress_lzw(self, data, min_code_size, expected_pixels):
        clear_code = 1 << min_code_size
        end_code = clear_code + 1
        code_size = min_code_size + 1
        max_code = (1 << code_size) - 1

        def reset_table():
            return {i: [i] for i in range(clear_code)}

        table = reset_table()
        next_code = end_code + 1

        bit_buf = 0
        bits_count = 0
        byte_idx = 0
        data_len = len(data)

        def read_bits(n):
            nonlocal bit_buf, bits_count, byte_idx
            while bits_count < n:
                if byte_idx < data_len:
                    bit_buf |= (data[byte_idx] << bits_count)
                    byte_idx += 1
                    bits_count += 8
                else:
                    break
            val = bit_buf & ((1 << n) - 1)
            bit_buf >>= n
            bits_count -= n
            return val

        pixels = []
        prev_code = None

        while len(pixels) < expected_pixels:
            code = read_bits(code_size)
            if code == end_code or (byte_idx >= data_len and bits_count < code_size and code == 0):
                break
            if code == clear_code:
                table = reset_table()
                code_size = min_code_size + 1
                max_code = (1 << code_size) - 1
                next_code = end_code + 1
                prev_code = None
                continue

            if prev_code is None:
                if code in table:
                    pixels.extend(table[code])
                    prev_code = code
                continue

            if code in table:
                entry = table[code]
            elif code == next_code:
                entry = table[prev_code] + [table[prev_code][0]]
            else:
                break

            pixels.extend(entry)

            if next_code < 4096:
                table[next_code] = table[prev_code] + [entry[0]]
                next_code += 1
                if next_code > max_code and code_size < 12:
                    code_size += 1
                    max_code = (1 << code_size) - 1

            prev_code = code

        return pixels

    def _uninterlace(self, pixels, w, h):
        out = [0] * (w * h)
        passes = [
            (0, 8),  # Pass 1: row 0, 8, 16...
            (4, 8),  # Pass 2: row 4, 12, 20...
            (2, 4),  # Pass 3: row 2, 6, 10...
            (1, 2)   # Pass 4: row 1, 3, 5...
        ]
        src_row = 0
        for start_row, step in passes:
            for y in range(start_row, h, step):
                if src_row * w >= len(pixels):
                    break
                row_data = pixels[src_row * w : (src_row + 1) * w]
                out[y * w : (y + 1) * w] = row_data
                src_row += 1
        return out

# ==============================================================================
# Pure Python BMP Parser & Writer
# ==============================================================================
def parse_bmp(filepath):
    """Parses uncompressed 1-bit or 24-bit Windows BMP files."""
    with open(filepath, 'rb') as f:
        file_hdr = f.read(14)
        if len(file_hdr) < 14 or file_hdr[:2] != b'BM':
            raise ValueError("Not a valid BMP file")
        px_offset = struct.unpack('<I', file_hdr[10:14])[0]

        info_hdr = f.read(40)
        w, h, planes, bpp, comp = struct.unpack('<iiHHI', info_hdr[4:20])
        flip_y = (h > 0)
        h = abs(h)

        if comp != 0:
            raise ValueError("Compressed BMP formats are not supported")

        f.seek(px_offset)
        if bpp == 1:
            row_bytes = ((w + 31) // 32) * 4
            grid = []
            for y in range(h):
                row_raw = f.read(row_bytes)
                row_px = []
                for x in range(w):
                    byte_val = row_raw[x // 8]
                    bit = (byte_val >> (7 - (x % 8))) & 1
                    c = (255, 255, 255) if bit else (0, 0, 0)
                    row_px.append(c)
                grid.append(row_px)
            if flip_y:
                grid.reverse()
            return w, h, grid
        elif bpp == 24:
            row_bytes = ((w * 3 + 3) // 4) * 4
            grid = []
            for y in range(h):
                row_raw = f.read(row_bytes)
                row_px = []
                for x in range(w):
                    b = row_raw[x * 3]
                    g = row_raw[x * 3 + 1]
                    r = row_raw[x * 3 + 2]
                    row_px.append((r, g, b))
                grid.append(row_px)
            if flip_y:
                grid.reverse()
            return w, h, grid
        else:
            raise ValueError(f"Unsupported BMP bit depth: {bpp} (only 1-bit and 24-bit supported)")

def write_monochrome_bmp(filepath, xbm_bytes):
    """Writes a 1024-byte 128x64 XBM bitmap as a standard 1-bit Windows BMP."""
    # BMP requires bottom-up scanlines with MSB-first bits
    row_bytes = 16  # 128 / 8 (already multiple of 4, so no pad needed)
    bmp_pixels = bytearray(1024)

    for y in range(64):
        src_row = 63 - y  # bottom-up
        for b in range(16):
            xbm_byte = xbm_bytes[src_row * 16 + b]
            # Bit reversal (XBM LSB-first -> BMP MSB-first)
            msb_byte = 0
            for bit in range(8):
                if (xbm_byte >> bit) & 1:
                    msb_byte |= (1 << (7 - bit))
            bmp_pixels[y * 16 + b] = msb_byte

    file_size = 14 + 40 + 8 + 1024  # 1086 bytes
    file_hdr = struct.pack('<2sIHHI', b'BM', file_size, 0, 0, 62)
    info_hdr = struct.pack('<IIIHHIIIIII',
        40, 128, 64, 1, 1, 0, 1024, 0, 0, 2, 0)
    palette = b'\x00\x00\x00\x00\xFF\xFF\xFF\x00'

    with open(filepath, 'wb') as f:
        f.write(file_hdr)
        f.write(info_hdr)
        f.write(palette)
        f.write(bmp_pixels)

# ==============================================================================
# Image Scaling & Aspect Ratio Geometry
# ==============================================================================
def resize_canvas(grid, src_w, src_h, target_w=128, target_h=64, mode='fit'):
    """Resizes RGB grid into target_w x target_h using fit, stretch, or crop."""
    out = [[(0, 0, 0)] * target_w for _ in range(target_h)]

    if mode == 'stretch':
        scale_x = src_w / target_w
        scale_y = src_h / target_h
        for ty in range(target_h):
            sy = min(int(ty * scale_y), src_h - 1)
            for tx in range(target_w):
                sx = min(int(tx * scale_x), src_w - 1)
                out[ty][tx] = grid[sy][sx]
        return out

    aspect_src = src_w / src_h
    aspect_target = target_w / target_h

    if mode == 'fit':
        if aspect_src > aspect_target:
            # Width bound
            render_w = target_w
            render_h = max(1, int(target_w / aspect_src))
        else:
            # Height bound
            render_h = target_h
            render_w = max(1, int(target_h * aspect_src))

        offset_x = (target_w - render_w) // 2
        offset_y = (target_h - render_h) // 2

        scale_x = src_w / render_w
        scale_y = src_h / render_h

        for ry in range(render_h):
            sy = min(int(ry * scale_y), src_h - 1)
            ty = offset_y + ry
            if ty < 0 or ty >= target_h:
                continue
            for rx in range(render_w):
                sx = min(int(rx * scale_x), src_w - 1)
                tx = offset_x + rx
                if 0 <= tx < target_w:
                    out[ty][tx] = grid[sy][sx]
        return out

    elif mode == 'crop':
        if aspect_src > aspect_target:
            # Crop horizontally
            scale = target_h / src_h
            src_crop_w = int(target_w / scale)
            src_start_x = (src_w - src_crop_w) // 2
            scale_x = src_crop_w / target_w
            for ty in range(target_h):
                sy = min(int(ty / scale), src_h - 1)
                for tx in range(target_w):
                    sx = min(src_start_x + int(tx * scale_x), src_w - 1)
                    out[ty][tx] = grid[sy][sx]
        else:
            # Crop vertically
            scale = target_w / src_w
            src_crop_h = int(target_h / scale)
            src_start_y = (src_h - src_crop_h) // 2
            scale_y = src_crop_h / target_h
            for ty in range(target_h):
                sy = min(src_start_y + int(ty * scale_y), src_h - 1)
                for tx in range(target_w):
                    sx = min(int(tx / scale), src_w - 1)
                    out[ty][tx] = grid[sy][sx]
        return out

    return out

# ==============================================================================
# Monochrome Quantization & Dithering Algorithms
# ==============================================================================
BAYER_4X4 = [
    [ 0,  8,  2, 10],
    [12,  4, 14,  6],
    [ 3, 11,  1,  9],
    [15,  7, 13,  5]
]

def rgb_to_gray(rgb):
    return 0.299 * rgb[0] + 0.587 * rgb[1] + 0.114 * rgb[2]

def dither_to_xbm(rgb_grid, dither_method='atkinson', threshold=128, invert=False):
    """
    Quantizes a 128x64 RGB grid to a 1024-byte 1-bit XBM format.
    XBM layout: 64 rows, 16 bytes per row, bit 0 is leftmost pixel.
    """
    h = len(rgb_grid)
    w = len(rgb_grid[0])
    assert w == 128 and h == 64

    # Convert to mutable 2D float grayscale
    gray = [[rgb_to_gray(rgb_grid[y][x]) for x in range(w)] for y in range(h)]
    out_bits = [[0] * w for _ in range(h)]

    if dither_method == 'none':
        for y in range(h):
            for x in range(w):
                out_bits[y][x] = 1 if gray[y][x] >= threshold else 0

    elif dither_method == 'bayer':
        for y in range(h):
            for x in range(w):
                matrix_val = (BAYER_4X4[y % 4][x % 4] + 0.5) * (256.0 / 16.0)
                out_bits[y][x] = 1 if gray[y][x] >= matrix_val else 0

    elif dither_method == 'floyd':
        for y in range(h):
            for x in range(w):
                old_val = gray[y][x]
                new_val = 255.0 if old_val >= threshold else 0.0
                out_bits[y][x] = 1 if new_val == 255.0 else 0
                err = old_val - new_val
                if x + 1 < w:
                    gray[y][x + 1] += err * (7.0 / 16.0)
                if y + 1 < h:
                    if x - 1 >= 0:
                        gray[y + 1][x - 1] += err * (3.0 / 16.0)
                    gray[y + 1][x] += err * (5.0 / 16.0)
                    if x + 1 < w:
                        gray[y + 1][x + 1] += err * (1.0 / 16.0)

    elif dither_method == 'atkinson':
        for y in range(h):
            for x in range(w):
                old_val = gray[y][x]
                new_val = 255.0 if old_val >= threshold else 0.0
                out_bits[y][x] = 1 if new_val == 255.0 else 0
                err = old_val - new_val
                fraction = err / 8.0
                if x + 1 < w: gray[y][x + 1] += fraction
                if x + 2 < w: gray[y][x + 2] += fraction
                if y + 1 < h:
                    if x - 1 >= 0: gray[y + 1][x - 1] += fraction
                    gray[y + 1][x] += fraction
                    if x + 1 < w: gray[y + 1][x + 1] += fraction
                if y + 2 < h:
                    gray[y + 2][x] += fraction

    # Pack into XBM byte array (LSB-first)
    xbm_bytes = bytearray(ANIM_FRAME_BYTES)
    for y in range(h):
        for byte_idx in range(16):
            byte_val = 0
            for bit in range(8):
                x = byte_idx * 8 + bit
                bit_val = out_bits[y][x]
                if invert:
                    bit_val = 1 - bit_val
                if bit_val:
                    byte_val |= (1 << bit)
            xbm_bytes[y * 16 + byte_idx] = byte_val

    return bytes(xbm_bytes)

# ==============================================================================
# .anim File Packing and Inspection
# ==============================================================================
def create_anim_header(frame_count, delay_ms=50, loop=True, invert=False):
    flags = 0
    if loop:
        flags |= ANIM_FLAG_LOOP
    if invert:
        flags |= ANIM_FLAG_INVERT
    return struct.pack('<IHBBHHHH',
        ANIM_MAGIC,
        ANIM_VERSION,
        ANIM_WIDTH,
        ANIM_HEIGHT,
        frame_count,
        delay_ms,
        flags,
        0
    )

def pack_anim(output_path, frames_xbm, delay_ms=50, loop=True, invert=False):
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    hdr = create_anim_header(len(frames_xbm), delay_ms, loop, invert)
    with open(output_path, 'wb') as f:
        f.write(hdr)
        for frame in frames_xbm:
            f.write(frame)
    file_sz = os.path.getsize(output_path)
    print(f"[OK] Packed {len(frames_xbm)} frames to {output_path} ({file_sz} bytes, {delay_ms} ms/frame)")

def inspect_anim(filepath):
    if not os.path.exists(filepath):
        print(f"Error: File '{filepath}' not found.")
        return False

    file_size = os.path.getsize(filepath)
    with open(filepath, 'rb') as f:
        hdr_data = f.read(16)
        if len(hdr_data) < 16:
            print(f"Error: File '{filepath}' is smaller than AnimHeader (16 bytes).")
            return False

        magic, ver, w, h, frames, delay, flags, _ = struct.unpack('<IHBBHHHH', hdr_data)
        if magic != ANIM_MAGIC:
            print(f"Error: Invalid magic 0x{magic:08X} (expected 0x{ANIM_MAGIC:08X} 'QANM').")
            return False

        loop = bool(flags & ANIM_FLAG_LOOP)
        invert = bool(flags & ANIM_FLAG_INVERT)
        fps = (1000.0 / delay) if delay > 0 else 0.0
        expected_size = 16 + (frames * ANIM_FRAME_BYTES)

        print("=== Q-Watch Animation Info ===")
        print(f"  File:        {filepath}")
        print(f"  File Size:   {file_size} bytes")
        print(f"  Version:     {ver}")
        print(f"  Dimensions:  {w}x{h} px")
        print(f"  Frames:      {frames}")
        print(f"  Delay:       {delay} ms (~{fps:.1f} FPS)")
        print(f"  Loop:        {'YES' if loop else 'NO'}")
        print(f"  Inverted:    {'YES' if invert else 'NO'}")
        print(f"  Integrity:   {'VALID' if file_size == expected_size else f'CORRUPT (Expected {expected_size} bytes)'}")
        return True

# ==============================================================================
# Terminal Live Animation Player (Unicode Half-Block 128x32)
# ==============================================================================
def render_terminal_frame(xbm_bytes, frame_idx, total_frames, delay_ms, filepath=""):
    fps = 1000.0 / delay_ms if delay_ms > 0 else 0.0
    hud = f" 007 Q-WATCH PLAYER | {os.path.basename(filepath)} | Frame {frame_idx + 1}/{total_frames} | {delay_ms}ms ({fps:.1f} FPS) "

    lines = []
    lines.append("\033[1;36m┌" + "─" * 128 + "┐\033[0m")
    hud_padded = hud.center(128)
    lines.append(f"\033[1;36m│\033[1;37;44m{hud_padded}\033[0;1;36m│\033[0m")
    lines.append("\033[1;36m├" + "─" * 128 + "┤\033[0m")

    for row in range(0, 64, 2):
        r0 = row * 16
        r1 = (row + 1) * 16
        chars = []
        for b in range(16):
            b0 = xbm_bytes[r0 + b]
            b1 = xbm_bytes[r1 + b]
            for bit in range(8):
                p0 = (b0 >> bit) & 1
                p1 = (b1 >> bit) & 1
                if p0 and p1:
                    chars.append("█")
                elif p0 and not p1:
                    chars.append("▀")
                elif not p0 and p1:
                    chars.append("▄")
                else:
                    chars.append(" ")
        lines.append("\033[1;36m│\033[0m" + "".join(chars) + "\033[1;36m│\033[0m")

    # Progress bar
    bar_width = 124
    prog = int((frame_idx / max(1, total_frames - 1)) * bar_width)
    bar_str = " [" + ("#" * prog) + ("-" * (bar_width - prog)) + "] "
    lines.append("\033[1;36m├" + "─" * 128 + "┤\033[0m")
    lines.append(f"\033[1;36m│\033[90m{bar_str}\033[1;36m│\033[0m")
    lines.append("\033[1;36m└" + "─" * 128 + "┘\033[0m")

    return "\n".join(lines)

def play_anim(filepath, override_fps=None, speed=1.0, loop=True, max_cycles=None):
    if not os.path.exists(filepath):
        print(f"Error: File '{filepath}' not found.")
        return

    with open(filepath, 'rb') as f:
        hdr = f.read(16)
        magic, ver, w, h, frames, delay_ms, flags, _ = struct.unpack('<IHBBHHHH', hdr)
        if magic != ANIM_MAGIC or w != 128 or h != 64:
            print("Error: Invalid .anim file format")
            return

        all_frames = []
        for _ in range(frames):
            fdata = f.read(ANIM_FRAME_BYTES)
            if len(fdata) != ANIM_FRAME_BYTES:
                break
            all_frames.append(fdata)

    if override_fps:
        delay_ms = int(1000.0 / override_fps)

    eff_delay = max(0.015, (delay_ms / 1000.0) / speed)
    file_loop = bool(flags & ANIM_FLAG_LOOP) and loop

    # Hide cursor & clear screen
    sys.stdout.write("\033[?25l\033[2J")
    sys.stdout.flush()

    cycles = 0
    try:
        while True:
            for idx, frame in enumerate(all_frames):
                frame_start = time.time()
                # Jump cursor to home (0,0) without clearing flicker
                sys.stdout.write("\033[H")
                sys.stdout.write(render_terminal_frame(frame, idx, len(all_frames), delay_ms, filepath))
                sys.stdout.flush()

                elapsed = time.time() - frame_start
                sleep_time = eff_delay - elapsed
                if sleep_time > 0:
                    time.sleep(sleep_time)

            cycles += 1
            if max_cycles and cycles >= max_cycles:
                break
            if not file_loop:
                break
    except KeyboardInterrupt:
        pass
    finally:
        # Restore cursor
        sys.stdout.write("\033[?25h\n")
        sys.stdout.flush()

# ==============================================================================
# Procedural Tactical Generators
# ==============================================================================
def generate_radar_frames(total_frames=24):
    frames = []
    cx, cy = 64, 32
    r_outer = 28
    r_mid = 18
    r_inner = 8

    for t in range(total_frames):
        buf = bytearray(ANIM_FRAME_BYTES)
        def px(x, y, v=1):
            if 0 <= x < 128 and 0 <= y < 64:
                idx = x + (y // 8) * 128
                if v: buf[idx] |= (1 << (y % 8))
                else: buf[idx] &= ~(1 << (y % 8))

        # Corner brackets
        for i in range(8):
            px(i, 0); px(0, i); px(127 - i, 0); px(127, i)
            px(i, 63); px(0, 63 - i); px(127 - i, 63); px(127, 63 - i)

        # Concentric circles
        for deg in range(0, 360, 4):
            rad = math.radians(deg)
            px(cx + int(math.cos(rad) * r_outer), cy + int(math.sin(rad) * r_outer))
            px(cx + int(math.cos(rad) * r_mid), cy + int(math.sin(rad) * r_mid))
            px(cx + int(math.cos(rad) * r_inner), cy + int(math.sin(rad) * r_inner))

        # Crosshairs
        for x in range(cx - r_outer - 4, cx + r_outer + 5):
            if abs(x - cx) > 2: px(x, cy)
        for y in range(cy - r_outer - 4, cy + r_outer + 5):
            if abs(y - cy) > 2: px(cx, y)

        # Rotating radar sweep line
        ang = (t / total_frames) * 2.0 * math.pi
        for dist in range(2, r_outer + 1):
            px(cx + int(math.cos(ang) * dist), cy + int(math.sin(ang) * dist))

        # Blips
        if t in [6, 7, 8, 9]:
            for dx in [-1, 0, 1]:
                for dy in [-1, 0, 1]: px(cx + 12 + dx, cy - 10 + dy)
        if t in [14, 15, 16, 17]:
            for dx in [-1, 0, 1]:
                for dy in [-1, 0, 1]: px(cx - 14 + dx, cy + 8 + dy)

        frames.append(bytes(buf))
    return frames

def generate_gunbarrel_frames(total_frames=20):
    frames = []
    cx, cy = 64, 32
    max_radius = 45

    for t in range(total_frames):
        buf = bytearray(ANIM_FRAME_BYTES)
        def px(x, y, v=1):
            if 0 <= x < 128 and 0 <= y < 64:
                idx = x + (y // 8) * 128
                if v: buf[idx] |= (1 << (y % 8))
                else: buf[idx] &= ~(1 << (y % 8))

        phase = t / total_frames
        grooves = 8
        rot = phase * math.pi * 0.5

        for g in range(grooves):
            base_theta = rot + (g * 2.0 * math.pi / grooves)
            for r in range(12, max_radius, 2):
                curve = base_theta + (r * 0.05)
                px(cx + int(math.cos(curve) * r), cy + int(math.sin(curve) * r))
                px(cx + 1 + int(math.cos(curve) * r), cy + int(math.sin(curve) * r))

        target_r = 10 + int(2.0 * math.sin(phase * 4.0 * math.pi))
        for deg in range(0, 360, 6):
            rad = math.radians(deg)
            px(cx + int(math.cos(rad) * target_r), cy + int(math.sin(rad) * target_r))

        for dx in [-1, 0, 1]:
            for dy in [-1, 0, 1]: px(cx + dx, cy + dy)

        frames.append(bytes(buf))
    return frames

def generate_matrix_frames(total_frames=24):
    frames = []
    import random
    cols = 32  # 128 / 4 px columns
    col_speeds = [random.randint(2, 5) for _ in range(cols)]
    col_offsets = [random.randint(0, 64) for _ in range(cols)]

    for t in range(total_frames):
        buf = bytearray(ANIM_FRAME_BYTES)
        def px(x, y, v=1):
            if 0 <= x < 128 and 0 <= y < 64:
                idx = x + (y // 8) * 128
                if v: buf[idx] |= (1 << (y % 8))

        for c in range(cols):
            head_y = (col_offsets[c] + t * col_speeds[c]) % 80
            col_x = c * 4
            # Trail behind head
            for trail in range(12):
                ty = head_y - trail
                if 0 <= ty < 64:
                    if trail == 0:
                        # Bright leading head (2x2 pixel cluster)
                        px(col_x, ty)
                        px(col_x + 1, ty)
                        px(col_x, ty - 1)
                        px(col_x + 1, ty - 1)
                    elif (trail + (t % 3)) % 2 == 0:
                        px(col_x, ty)
        frames.append(bytes(buf))
    return frames

def generate_ecg_frames(total_frames=24):
    frames = []
    cy = 32
    # Baseline ECG waveform template for 128 width
    ecg_y = [cy] * 128
    # P wave
    for i in range(10): ecg_y[30 + i] = cy - int(3 * math.sin(i * math.pi / 10))
    # Q wave
    ecg_y[44] = cy + 4
    # R wave (spike)
    ecg_y[46] = cy - 22
    ecg_y[47] = cy - 26
    ecg_y[48] = cy - 20
    # S wave
    ecg_y[50] = cy + 10
    # T wave
    for i in range(16): ecg_y[60 + i] = cy - int(6 * math.sin(i * math.pi / 16))

    for t in range(total_frames):
        buf = bytearray(ANIM_FRAME_BYTES)
        def px(x, y):
            if 0 <= x < 128 and 0 <= y < 64:
                buf[x + (y // 8) * 128] |= (1 << (y % 8))

        # Faint grid background
        for gx in range(0, 128, 16):
            for gy in range(0, 64, 4): px(gx, gy)

        sweep_x = int((t / total_frames) * 128)
        # Draw waveform up to sweep_x
        for x in range(128):
            dist = (x - sweep_x) % 128
            if dist > 110 or dist < 2:  # Glowing head region
                y = ecg_y[x]
                px(x, y)
                if x > 0:
                    prev_y = ecg_y[x - 1]
                    for ly in range(min(y, prev_y), max(y, prev_y) + 1):
                        px(x, ly)

        # Pulse sweep marker
        for sy in range(cy - 12, cy + 13):
            px(sweep_x, sy)

        frames.append(bytes(buf))
    return frames

def generate_lockon_frames(total_frames=20):
    frames = []
    cx, cy = 64, 32

    for t in range(total_frames):
        buf = bytearray(ANIM_FRAME_BYTES)
        def px(x, y):
            if 0 <= x < 128 and 0 <= y < 64:
                buf[x + (y // 8) * 128] |= (1 << (y % 8))

        # Contracting box
        box_sz = 26 - int(6 * math.sin((t / total_frames) * math.pi))
        # Draw 4 corner reticle brackets
        k = 6
        # Top-left
        for i in range(k): px(cx - box_sz + i, cy - box_sz); px(cx - box_sz, cy - box_sz + i)
        # Top-right
        for i in range(k): px(cx + box_sz - i, cy - box_sz); px(cx + box_sz, cy - box_sz + i)
        # Bottom-left
        for i in range(k): px(cx - box_sz + i, cy + box_sz); px(cx - box_sz, cy + box_sz - i)
        # Bottom-right
        for i in range(k): px(cx + box_sz - i, cy + box_sz); px(cx + box_sz, cy + box_sz - i)

        # Center target crosshairs
        for x in range(cx - 10, cx + 11):
            if abs(x - cx) > 2: px(x, cy)
        for y in range(cy - 10, cy + 11):
            if abs(y - cy) > 2: px(cx, y)

        # Rotating outer ticks
        rot = (t / total_frames) * 2.0 * math.pi
        for deg in [0, 90, 180, 270]:
            rad = rot + math.radians(deg)
            for r in range(box_sz + 4, box_sz + 8):
                px(cx + int(math.cos(rad) * r), cy + int(math.sin(rad) * r))

        frames.append(bytes(buf))
    return frames

# ==============================================================================
# Frame Extraction & Input Conversion Pipeline
# ==============================================================================
def convert_input_to_anim(input_path, output_path, dither='atkinson', threshold=128,
                          mode='fit', delay_ms=None, fps=None, invert=False, loop=True,
                          step=1, max_frames=None):
    """Converts a GIF, BMP, or image sequence to a .anim file."""
    frames_xbm = []
    detected_delays = []

    # 1. Handle Animated GIF
    if input_path.lower().endswith('.gif'):
        print(f"Decoding GIF: {input_path}...")
        with open(input_path, 'rb') as f:
            data = f.read()
        decoder = GifDecoder(data)
        raw_frames = decoder.decode()
        if not raw_frames:
            raise ValueError("No frames extracted from GIF")

        print(f"Decoded {len(raw_frames)} source frames.")
        for idx, (frame_delay, w, h, canvas) in enumerate(raw_frames):
            if idx % step != 0:
                continue
            detected_delays.append(frame_delay * step)
            grid = resize_canvas(canvas, w, h, 128, 64, mode=mode)
            xbm = dither_to_xbm(grid, dither_method=dither, threshold=threshold, invert=invert)
            frames_xbm.append(xbm)
            if max_frames and len(frames_xbm) >= max_frames:
                break

    # 2. Handle Single BMP
    elif input_path.lower().endswith('.bmp'):
        w, h, grid = parse_bmp(input_path)
        resized = resize_canvas(grid, w, h, 128, 64, mode=mode)
        xbm = dither_to_xbm(resized, dither_method=dither, threshold=threshold, invert=invert)
        frames_xbm.append(xbm)
        detected_delays.append(1000)

    # 3. Handle Other Formats via Pillow if available
    elif HAS_PILLOW:
        img = PILImage.open(input_path)
        n_frames = getattr(img, 'n_frames', 1)
        for i in range(n_frames):
            if i % step != 0:
                continue
            img.seek(i)
            frame_rgb = img.convert('RGB')
            w, h = frame_rgb.size
            pixels = list(frame_rgb.getdata())
            grid = [pixels[r * w : (r + 1) * w] for r in range(h)]
            resized = resize_canvas(grid, w, h, 128, 64, mode=mode)
            xbm = dither_to_xbm(resized, dither_method=dither, threshold=threshold, invert=invert)
            frames_xbm.append(xbm)
            d = img.info.get('duration', 50)
            detected_delays.append(d * step)
            if max_frames and len(frames_xbm) >= max_frames:
                break
    else:
        raise ValueError(f"Unsupported format or Pillow required for '{input_path}'. Install Pillow: pip install pillow")

    # Determine final delay
    final_delay = 50
    if delay_ms is not None:
        final_delay = delay_ms
    elif fps is not None:
        final_delay = int(1000.0 / fps)
    elif detected_delays:
        final_delay = int(sum(detected_delays) / len(detected_delays))
        if final_delay < 15:
            final_delay = 50

    pack_anim(output_path, frames_xbm, delay_ms=final_delay, loop=loop, invert=invert)

def extract_anim_frames(anim_path, out_dir, fmt='bmp'):
    """Extracts all frames of a .anim file into .bmp or .xbm files."""
    os.makedirs(out_dir, exist_ok=True)
    with open(anim_path, 'rb') as f:
        hdr = f.read(16)
        magic, ver, w, h, frames, delay, flags, _ = struct.unpack('<IHBBHHHH', hdr)
        if magic != ANIM_MAGIC:
            raise ValueError("Invalid .anim magic")

        base_name = os.path.splitext(os.path.basename(anim_path))[0]
        for i in range(frames):
            fdata = f.read(ANIM_FRAME_BYTES)
            if len(fdata) != ANIM_FRAME_BYTES:
                break

            out_file = os.path.join(out_dir, f"{base_name}_frame_{i:03d}.{fmt}")
            if fmt == 'bmp':
                write_monochrome_bmp(out_file, fdata)
            else:
                with open(out_file, 'wb') as xf:
                    xf.write(fdata)
        print(f"[OK] Extracted {frames} frames to {out_dir}/ ({fmt})")

# ==============================================================================
# CLI Entry Point
# ==============================================================================
def main():
    parser = argparse.ArgumentParser(
        description="007 Q-Watch Tactical Animation & Media Suite",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""Examples:
  anim_packer.py convert mission.gif data/anim/mission.anim --dither atkinson
  anim_packer.py play data/anim/radar.anim
  anim_packer.py generate data/anim/matrix.anim --type matrix
  anim_packer.py bmp-create logo.gif data/boot/boot.bmp --dither atkinson
  anim_packer.py extract data/anim/radar.anim --dir out_frames/
  anim_packer.py info data/anim/radar.anim
"""
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    # Info
    p_info = subparsers.add_parser("info", help="Inspect .anim file header")
    p_info.add_argument("file", help="Path to .anim file")

    # Play (Terminal Live Preview)
    p_play = subparsers.add_parser("play", help="Live terminal Unicode preview player")
    p_play.add_argument("file", help="Path to .anim file")
    p_play.add_argument("--fps", type=float, default=None, help="Override playback FPS")
    p_play.add_argument("--speed", type=float, default=1.0, help="Playback speed multiplier (default: 1.0)")
    p_play.add_argument("--once", action="store_true", help="Play once and exit")
    p_play.add_argument("--cycles", type=int, default=None, help="Number of playback loops before exit")

    # Convert
    p_conv = subparsers.add_parser("convert", help="Convert GIF/BMP/image to .anim")
    p_conv.add_argument("input", help="Source image (GIF, BMP, etc.)")
    p_conv.add_argument("output", help="Target .anim path")
    p_conv.add_argument("--dither", choices=['atkinson', 'floyd', 'bayer', 'none'], default='atkinson',
                        help="Dithering algorithm (default: atkinson)")
    p_conv.add_argument("--threshold", type=int, default=128, help="Binarization threshold (0-255, default: 128)")
    p_conv.add_argument("--mode", choices=['fit', 'stretch', 'crop'], default='fit',
                        help="Aspect ratio fit mode (default: fit)")
    p_conv.add_argument("--delay", type=int, default=None, help="Frame delay in ms (overrides GIF delay)")
    p_conv.add_argument("--fps", type=float, default=None, help="Target FPS (overrides GIF delay)")
    p_conv.add_argument("--invert", action="store_true", help="Invert monochrome pixels")
    p_conv.add_argument("--no-loop", action="store_true", help="Disable loop flag")
    p_conv.add_argument("--step", type=int, default=1, help="Frame decimation step (e.g. 2 for half-rate)")
    p_conv.add_argument("--max-frames", type=int, default=None, help="Maximum number of frames to convert")

    # Generate Procedural
    p_gen = subparsers.add_parser("generate", help="Generate procedural tactical animation")
    p_gen.add_argument("output", help="Output .anim path")
    p_gen.add_argument("--type", choices=['radar', 'gunbarrel', 'matrix', 'ecg', 'lockon'], default='radar',
                       help="Animation type (radar, gunbarrel, matrix, ecg, lockon)")
    p_gen.add_argument("--delay", type=int, default=50, help="Frame delay in ms (default: 50ms)")
    p_gen.add_argument("--frames", type=int, default=24, help="Total frames to generate")
    p_gen.add_argument("--no-loop", action="store_true", help="Disable loop flag")
    p_gen.add_argument("--invert", action="store_true", help="Invert pixel output")

    # BMP Create (for /boot/boot.bmp)
    p_bmp = subparsers.add_parser("bmp-create", help="Create standard 128x64 1-bit boot BMP")
    p_bmp.add_argument("input", help="Source image (GIF, BMP, etc.)")
    p_bmp.add_argument("output", help="Output .bmp path (e.g. data/boot/boot.bmp)")
    p_bmp.add_argument("--dither", choices=['atkinson', 'floyd', 'bayer', 'none'], default='atkinson',
                       help="Dithering algorithm (default: atkinson)")
    p_bmp.add_argument("--threshold", type=int, default=128, help="Binarization threshold (0-255)")
    p_bmp.add_argument("--mode", choices=['fit', 'stretch', 'crop'], default='fit',
                       help="Aspect ratio fit mode (default: fit)")
    p_bmp.add_argument("--invert", action="store_true", help="Invert pixels")

    # Extract
    p_ext = subparsers.add_parser("extract", help="Extract frames from .anim file")
    p_ext.add_argument("file", help="Path to .anim file")
    p_ext.add_argument("--dir", default="extracted_frames", help="Output directory (default: extracted_frames/)")
    p_ext.add_argument("--format", choices=['bmp', 'xbm'], default='bmp', help="Output frame format (default: bmp)")

    args = parser.parse_args()

    if args.command == "info":
        inspect_anim(args.file)

    elif args.command == "play":
        play_anim(args.file, override_fps=args.fps, speed=args.speed, loop=not args.once, max_cycles=args.cycles)

    elif args.command == "convert":
        convert_input_to_anim(
            args.input, args.output, dither=args.dither, threshold=args.threshold,
            mode=args.mode, delay_ms=args.delay, fps=args.fps, invert=args.invert,
            loop=not args.no_loop, step=args.step, max_frames=args.max_frames
        )

    elif args.command == "generate":
        if args.type == "radar":
            frames = generate_radar_frames(args.frames)
        elif args.type == "gunbarrel":
            frames = generate_gunbarrel_frames(args.frames)
        elif args.type == "matrix":
            frames = generate_matrix_frames(args.frames)
        elif args.type == "ecg":
            frames = generate_ecg_frames(args.frames)
        elif args.type == "lockon":
            frames = generate_lockon_frames(args.frames)
        else:
            frames = generate_radar_frames(args.frames)

        pack_anim(args.output, frames, delay_ms=args.delay, loop=not args.no_loop, invert=args.invert)

    elif args.command == "bmp-create":
        # Extract first frame
        if args.input.lower().endswith('.gif'):
            with open(args.input, 'rb') as f:
                decoder = GifDecoder(f.read())
                raw_frames = decoder.decode()
            if not raw_frames:
                raise ValueError("No frames found in GIF")
            _, w, h, canvas = raw_frames[0]
            grid = resize_canvas(canvas, w, h, 128, 64, mode=args.mode)
        elif args.input.lower().endswith('.bmp'):
            w, h, grid = parse_bmp(args.input)
            grid = resize_canvas(grid, w, h, 128, 64, mode=args.mode)
        elif HAS_PILLOW:
            img = PILImage.open(args.input).convert('RGB')
            w, h = img.size
            pixels = list(img.getdata())
            grid = [pixels[r * w : (r + 1) * w] for r in range(h)]
            grid = resize_canvas(grid, w, h, 128, 64, mode=args.mode)
        else:
            raise ValueError("Unsupported format or Pillow missing")

        xbm = dither_to_xbm(grid, dither_method=args.dither, threshold=args.threshold, invert=args.invert)
        write_monochrome_bmp(args.output, xbm)
        print(f"[OK] Generated 1-bit boot BMP: {args.output} ({os.path.getsize(args.output)} bytes)")

    elif args.command == "extract":
        extract_anim_frames(args.file, args.dir, fmt=args.format)

if __name__ == "__main__":
    main()
