#!/usr/bin/env python3
"""PC 端位图生成工具：将 PNG 转换为 RGB565 .bin 文件。

用法:
    单图:   python tools/bitmap_gen.py --input=logo.png --out=assets/logo.bin
    图标集: python tools/bitmap_gen.py --icons=icons/ --size=24 --out=assets/icons_24.bin

依赖: Pillow (pip install Pillow)

单图输出格式:
    Header(4B): width(2B LE) + height(2B LE)
    像素数据: width * height * 2 字节 (RGB565 big-endian)

图标集输出格式:
    Header(6B): icon_w(2B LE) + icon_h(2B LE) + count(2B LE)
    图标数据: count × (icon_w × icon_h × 2) 字节
"""

import argparse
import os
import struct
import sys

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow is required. Install with: pip install Pillow")
    sys.exit(1)


def rgb888_to_565(r, g, b):
    """RGB888 → RGB565 (big-endian bytes)."""
    val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return struct.pack('>H', val)


def image_to_rgb565(img):
    """将 PIL Image 转换为 RGB565 字节数据。"""
    img = img.convert('RGB')
    data = bytearray()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b = img.getpixel((x, y))
            data.extend(rgb888_to_565(r, g, b))
    return bytes(data)


def convert_single(input_path, out_path):
    """转换单张图片为 .bin。"""
    img = Image.open(input_path)
    w, h = img.size
    data = image_to_rgb565(img)
    with open(out_path, 'wb') as f:
        f.write(struct.pack('<HH', w, h))
        f.write(data)
    print(f"Written: {out_path} ({w}x{h}, {4 + len(data)} bytes)")


def convert_icons(icons_dir, icon_size, out_path):
    """将目录中的图片打包为图标集 .bin。"""
    files = sorted(f for f in os.listdir(icons_dir)
                   if f.lower().endswith(('.png', '.bmp', '.jpg')))
    if not files:
        print(f"ERROR: No image files found in {icons_dir}")
        sys.exit(1)

    count = len(files)
    with open(out_path, 'wb') as f:
        f.write(struct.pack('<HHH', icon_size, icon_size, count))
        for fname in files:
            img = Image.open(os.path.join(icons_dir, fname))
            img = img.resize((icon_size, icon_size), Image.LANCZOS)
            data = image_to_rgb565(img)
            f.write(data)
    total = 6 + count * icon_size * icon_size * 2
    print(f"Written: {out_path} ({count} icons @ {icon_size}x{icon_size}, {total} bytes)")


def main():
    parser = argparse.ArgumentParser(description='Convert images to RGB565 .bin')
    parser.add_argument('--input', help='Single image file path')
    parser.add_argument('--icons', help='Directory of icon images')
    parser.add_argument('--size', type=int, default=24, help='Icon size (default: 24)')
    parser.add_argument('--out', required=True, help='Output .bin file path')
    args = parser.parse_args()

    if args.input:
        convert_single(args.input, args.out)
    elif args.icons:
        convert_icons(args.icons, args.size, args.out)
    else:
        print("ERROR: Specify --input for single image or --icons for icon set")
        sys.exit(1)


if __name__ == '__main__':
    main()
