#!/usr/bin/env python3
"""PNG → 4-bit 灰度 + RLE 压缩 → .bin 图标文件转换工具。

用法:
  单个图标:
    py tools/icon_converter.py icon.png -o assets/icon.bin
  图标集(多个PNG打包):
    py tools/icon_converter.py speed.png color.png rgb.png bright.png smoke.png pump.png -o assets/menu_icons.bin --sheet

.bin 单图标格式:
  Header (4B): width(1B) + height(1B) + flags(1B) + reserved(1B)
    flags bit0: 1=RLE压缩, 0=无压缩
  Data: 4-bit灰度像素, RLE编码
    RLE格式: [count(1B)][value(1B)]
      count高4位=重复次数-1 (0=1次, 15=16次)
      count低4位=第一个像素灰度值
      value: 如果count>0, 后续像素打包(每字节2个4bit像素)
    简化RLE: [run_length(1B)][gray_value(1B)]
      run_length: 1-255 连续相同像素数
      gray_value: 0-15 灰度值
      run_length=0 表示结束

.bin 图标集格式:
  Header (6B): icon_w(1B) + icon_h(1B) + count(1B) + flags(1B) + reserved(2B)
  Offsets: count × 2B (每个图标数据的偏移量, 相对于数据区起始)
  Data: 连续的RLE压缩图标数据

实际采用更简单的RLE:
  像素流: 逐行扫描, 每像素4bit灰度(0=透明/黑, 15=全白)
  RLE编码: 交替的 [count][value] 字节对
    count: 1-255, 连续相同灰度值的像素数
    value: 0-15 灰度级别
  结束标记: count=0
"""

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("ERROR: 需要 Pillow 库。运行: pip install Pillow")
    sys.exit(1)


def png_to_gray4(img_path, target_size=None):
    """将PNG转为4-bit灰度数组。

    Args:
        img_path: PNG文件路径
        target_size: (w, h) 目标尺寸, None则保持原尺寸

    Returns:
        (width, height, pixels) 其中pixels是0-15的灰度值列表
    """
    img = Image.open(img_path).convert('RGBA')

    if target_size:
        img = img.resize(target_size, Image.LANCZOS)

    w, h = img.size
    pixels = []

    for y in range(h):
        for x in range(w):
            r, g, b, a = img.getpixel((x, y))
            # 计算亮度 (加权灰度)
            gray = int(0.299 * r + 0.587 * g + 0.114 * b)
            # 应用alpha
            gray = gray * a // 255
            # 量化到4bit (0-15)
            gray4 = gray * 15 // 255
            pixels.append(gray4)

    return w, h, pixels


def rle_encode(pixels):
    """RLE编码4-bit灰度像素流。

    Returns:
        bytes: RLE编码数据, 以count=0结束
    """
    if not pixels:
        return b'\x00'

    result = bytearray()
    i = 0
    n = len(pixels)

    while i < n:
        val = pixels[i]
        count = 1
        while i + count < n and pixels[i + count] == val and count < 255:
            count += 1
        result.append(count)
        result.append(val)
        i += count

    result.append(0)  # 结束标记
    return bytes(result)


def convert_single(img_path, out_path, target_size=None):
    """转换单个PNG为.bin图标文件。"""
    w, h, pixels = png_to_gray4(img_path, target_size)
    rle_data = rle_encode(pixels)

    raw_size = w * h  # 4bit打包后是 w*h/2, 但我们用字节级RLE
    print(f"  {Path(img_path).name}: {w}x{h}, 原始={raw_size}B, "
          f"RLE={len(rle_data)}B, 压缩率={len(rle_data)*100//max(raw_size,1)}%")

    with open(out_path, 'wb') as f:
        # Header: w(1B) + h(1B) + flags(1B) + reserved(1B)
        f.write(bytes([w, h, 0x01, 0x00]))  # flags=1 表示RLE
        f.write(rle_data)

    return len(rle_data) + 4


def convert_sheet(img_paths, out_path, target_size=None):
    """转换多个PNG为图标集.bin文件。"""
    icons_data = []
    icon_w, icon_h = 0, 0

    for path in img_paths:
        w, h, pixels = png_to_gray4(path, target_size)
        if icon_w == 0:
            icon_w, icon_h = w, h
        elif (w, h) != (icon_w, icon_h):
            print(f"WARNING: {path} 尺寸 {w}x{h} != {icon_w}x{icon_h}, 将缩放")
            w, h, pixels = png_to_gray4(path, (icon_w, icon_h))

        rle_data = rle_encode(pixels)
        raw_size = w * h
        print(f"  {Path(path).name}: RLE={len(rle_data)}B "
              f"(压缩率={len(rle_data)*100//max(raw_size,1)}%)")
        icons_data.append(rle_data)

    count = len(icons_data)

    # 计算偏移表
    header_size = 6  # icon_w + icon_h + count + flags + reserved*2
    offset_table_size = count * 2  # 每个图标2字节偏移
    data_start = header_size + offset_table_size

    offsets = []
    current_offset = 0
    for data in icons_data:
        offsets.append(current_offset)
        current_offset += len(data)

    total_size = data_start + current_offset
    print(f"\n  图标集: {count}个 {icon_w}x{icon_h} 图标, 总大小={total_size}B")

    with open(out_path, 'wb') as f:
        # Header
        f.write(bytes([icon_w, icon_h, count, 0x01, 0x00, 0x00]))
        # Offset table (2B LE each)
        for off in offsets:
            f.write(bytes([off & 0xFF, (off >> 8) & 0xFF]))
        # Icon data
        for data in icons_data:
            f.write(data)

    return total_size


def main():
    parser = argparse.ArgumentParser(description='PNG → 4-bit灰度RLE图标转换')
    parser.add_argument('inputs', nargs='+', help='输入PNG文件')
    parser.add_argument('-o', '--output', required=True, help='输出.bin文件')
    parser.add_argument('--sheet', action='store_true', help='打包为图标集')
    parser.add_argument('--size', type=int, default=None,
                        help='目标尺寸(正方形), 如 --size=40')
    args = parser.parse_args()

    target_size = (args.size, args.size) if args.size else None

    print("=" * 50)
    print("PNG → 4-bit灰度RLE 图标转换")
    print("=" * 50)

    if args.sheet:
        total = convert_sheet(args.inputs, args.output, target_size)
    else:
        total = convert_single(args.inputs[0], args.output, target_size)

    print(f"\n✅ 输出: {args.output} ({total} bytes)")


if __name__ == '__main__':
    main()
