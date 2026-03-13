#!/usr/bin/env python3
"""PC 端字体生成工具：从 TTF 生成 .bin 位图字体文件。

用法:
    python tools/font_gen.py --ttf=font.ttf --size=16 --chars="0123456789%." --out=assets/font_16.bin

依赖: Pillow (pip install Pillow)

输出格式:
    Header(4B): width(1B) + height(1B) + first_char(1B) + char_count(1B)
    字形数据: 每字符 = width * ceil(height/8) 字节，列优先位图
"""

import argparse
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("ERROR: Pillow is required. Install with: pip install Pillow")
    sys.exit(1)


def render_font(ttf_path, size, chars):
    """渲染字符集为位图字形列表。"""
    font = ImageFont.truetype(ttf_path, size)
    glyphs = []
    max_w, max_h = 0, 0

    for ch in chars:
        bbox = font.getbbox(ch)
        w = bbox[2] - bbox[0]
        h = bbox[3] - bbox[1]
        max_w = max(max_w, w)
        max_h = max(max_h, h)

    # 统一尺寸
    glyph_w = max_w
    glyph_h = max_h
    col_bytes = (glyph_h + 7) // 8

    for ch in chars:
        img = Image.new('1', (glyph_w, glyph_h), 0)
        draw = ImageDraw.Draw(img)
        bbox = font.getbbox(ch)
        ox = -bbox[0]
        oy = -bbox[1]
        draw.text((ox, oy), ch, fill=1, font=font)

        # 列优先位图编码
        data = bytearray(glyph_w * col_bytes)
        for x in range(glyph_w):
            for y in range(glyph_h):
                if img.getpixel((x, y)):
                    byte_idx = x * col_bytes + y // 8
                    data[byte_idx] |= 1 << (y % 8)
        glyphs.append(bytes(data))

    return glyph_w, glyph_h, glyphs


def write_bin(out_path, glyph_w, glyph_h, first_char, chars, glyphs):
    """写入 .bin 字体文件。"""
    char_count = len(chars)
    with open(out_path, 'wb') as f:
        f.write(bytes([glyph_w, glyph_h, first_char, char_count]))
        for g in glyphs:
            f.write(g)
    print(f"Written: {out_path} ({glyph_w}x{glyph_h}, {char_count} chars, "
          f"{4 + sum(len(g) for g in glyphs)} bytes)")


def main():
    parser = argparse.ArgumentParser(description='Generate bitmap font .bin from TTF')
    parser.add_argument('--ttf', required=True, help='Path to TTF font file')
    parser.add_argument('--size', type=int, required=True, help='Font size in pixels')
    parser.add_argument('--chars', default='0123456789%./:',
                        help='Characters to include (default: digits + symbols)')
    parser.add_argument('--out', required=True, help='Output .bin file path')
    args = parser.parse_args()

    chars = args.chars
    first_char = ord(min(chars))

    # 构建连续字符范围（填充空白字形）
    last_char = ord(max(chars))
    full_range = [chr(c) for c in range(first_char, last_char + 1)]

    glyph_w, glyph_h, glyphs = render_font(args.ttf, args.size, chars)

    # 为不在 chars 中的字符填充空白字形
    col_bytes = (glyph_h + 7) // 8
    empty = bytes(glyph_w * col_bytes)
    full_glyphs = []
    for ch in full_range:
        if ch in chars:
            idx = chars.index(ch)
            full_glyphs.append(glyphs[idx])
        else:
            full_glyphs.append(empty)

    write_bin(args.out, glyph_w, glyph_h, first_char, full_range, full_glyphs)


if __name__ == '__main__':
    main()
