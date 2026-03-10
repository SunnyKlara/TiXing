#!/usr/bin/env python3
"""生成菜单图标PNG并转换为4-bit灰度RLE .bin图标集。

用Pillow程序化绘制6个菜单图标(模拟你的设计风格):
  Speed(风), Smoke(烟雾), Pump(气泵), Color(调色板), RGB(油漆滚筒), Bright(太阳)

每个图标40×40像素, 白色图形+黑色背景, 带抗锯齿。
最终打包为 assets/menu_icons_alpha.bin

用法: py tools/generate_menu_icons.py
"""

import sys
import os
import math
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont, ImageFilter
except ImportError:
    print("ERROR: 需要 Pillow。运行: pip install Pillow")
    sys.exit(1)

# 图标尺寸
ICON_SIZE = 40
# 输出目录
OUT_DIR = Path("assets/icons_png")
BIN_OUT = Path("assets/menu_icons_alpha.bin")


def draw_speed_icon(size=ICON_SIZE):
    """风/速度图标: 三条弧形风线"""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2 - 2
    white = (255, 255, 255, 255)

    # 三条风线 (从上到下, 长度递增)
    lines = [
        (cx - 8, cy - 8, cx + 10, cy - 8, 6),   # 短线
        (cx - 12, cy, cx + 14, cy, 8),            # 中线
        (cx - 8, cy + 8, cx + 10, cy + 8, 6),    # 短线
    ]

    for x1, y1, x2, y2, curve in lines:
        # 画弧形风线: 主线 + 右端向上弯曲
        d.line([(x1, y1), (x2, y2)], fill=white, width=2)
        # 右端弯曲
        d.arc([x2 - curve, y1 - curve, x2 + curve, y1 + curve],
              start=-60, end=30, fill=white, width=2)

    return img


def draw_smoke_icon(size=ICON_SIZE):
    """烟雾图标: 波浪形烟雾上升"""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx = size // 2
    white = (255, 255, 255, 255)

    # 底部容器(小矩形)
    bw, bh = 14, 6
    bx = cx - bw // 2
    by = size - 12
    d.rounded_rectangle([bx, by, bx + bw, by + bh], radius=2, fill=white)

    # 三条波浪烟雾线
    for i, offset in enumerate([-4, 0, 4]):
        x = cx + offset
        points = []
        for t in range(20):
            py = by - 4 - t
            px = x + int(3 * math.sin(t * 0.5 + i * 1.0))
            points.append((px, py))
        if len(points) > 1:
            d.line(points, fill=white, width=2)

    return img


def draw_pump_icon(size=ICON_SIZE):
    """气泵图标: 简化的气泵形状"""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2
    white = (255, 255, 255, 255)

    # 泵体(圆角矩形)
    pw, ph = 16, 20
    px = cx - pw // 2
    py = cy - ph // 2 + 2
    d.rounded_rectangle([px, py, px + pw, py + ph], radius=3, fill=white)

    # 泵头(顶部小矩形)
    hw = 8
    hx = cx - hw // 2
    d.rectangle([hx, py - 5, hx + hw, py], fill=white)

    # 手柄(顶部横线)
    d.line([(cx - 2, py - 8), (cx - 2, py - 5)], fill=white, width=2)
    d.line([(cx - 6, py - 8), (cx + 6, py - 8)], fill=white, width=2)

    # 出气口(右侧小管)
    d.line([(px + pw, cy), (px + pw + 5, cy)], fill=white, width=2)
    d.ellipse([px + pw + 3, cy - 2, px + pw + 7, cy + 2], fill=white)

    return img


def draw_color_icon(size=ICON_SIZE):
    """调色板图标: 画家调色板形状"""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2
    white = (255, 255, 255, 255)
    black = (0, 0, 0, 255)

    # 调色板主体(椭圆)
    r = 14
    d.ellipse([cx - r, cy - r + 1, cx + r, cy + r + 1], fill=white)

    # 拇指孔(左下)
    d.ellipse([cx - 8, cy + 2, cx - 2, cy + 8], fill=black)

    # 颜料点(黑色圆点在白色调色板上)
    dots = [
        (cx - 4, cy - 8, 3),
        (cx + 5, cy - 6, 3),
        (cx + 8, cy + 1, 3),
        (cx - 1, cy - 2, 3),
        (cx + 3, cy + 4, 3),
    ]
    for dx, dy, dr in dots:
        d.ellipse([dx - dr, dy - dr, dx + dr, dy + dr], fill=black)

    return img


def draw_rgb_icon(size=ICON_SIZE):
    """RGB/油漆滚筒图标"""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2
    white = (255, 255, 255, 255)

    # 滚筒头(圆角矩形)
    rw, rh = 18, 8
    rx = cx - rw // 2
    ry = cy - 12
    d.rounded_rectangle([rx, ry, rx + rw, ry + rh], radius=2, fill=white)

    # 连接杆
    d.line([(cx, ry + rh), (cx, cy + 2)], fill=white, width=3)

    # 手柄(下方)
    d.line([(cx, cy + 2), (cx, cy + 10)], fill=white, width=2)
    d.line([(cx - 4, cy + 2), (cx + 4, cy + 2)], fill=white, width=2)

    # 红色指示点
    red = (255, 50, 50, 255)
    d.ellipse([cx + 6, cy - 2, cx + 12, cy + 4], fill=red)

    return img


def draw_bright_icon(size=ICON_SIZE):
    """亮度/太阳图标: 中心圆+放射线"""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2
    white = (255, 255, 255, 255)

    # 中心圆
    cr = 6
    d.ellipse([cx - cr, cy - cr, cx + cr, cy + cr], fill=white)

    # 8条放射线
    inner_r = 9
    outer_r = 14
    for i in range(8):
        angle = i * math.pi / 4
        x1 = cx + int(inner_r * math.cos(angle))
        y1 = cy + int(inner_r * math.sin(angle))
        x2 = cx + int(outer_r * math.cos(angle))
        y2 = cy + int(outer_r * math.sin(angle))
        d.line([(x1, y1), (x2, y2)], fill=white, width=2)

    return img


def png_to_gray4(img):
    """PIL Image → 4-bit灰度像素列表。"""
    img_rgba = img.convert('RGBA')
    w, h = img_rgba.size
    pixels = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = img_rgba.getpixel((x, y))
            gray = int(0.299 * r + 0.587 * g + 0.114 * b)
            gray = gray * a // 255
            gray4 = min(15, gray * 15 // 255)
            pixels.append(gray4)
    return pixels


def rle_encode(pixels):
    """RLE编码。"""
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
    result.append(0)
    return bytes(result)


def build_icon_sheet(icons, out_path):
    """将多个PIL Image打包为图标集.bin。"""
    icon_w, icon_h = icons[0].size
    count = len(icons)

    all_rle = []
    total_rle = 0
    for i, img in enumerate(icons):
        pixels = png_to_gray4(img)
        rle = rle_encode(pixels)
        all_rle.append(rle)
        total_rle += len(rle)
        raw = icon_w * icon_h
        print(f"  Icon {i}: RLE={len(rle)}B (压缩率={len(rle)*100//max(raw,1)}%)")

    # 计算偏移
    offsets = []
    off = 0
    for rle in all_rle:
        offsets.append(off)
        off += len(rle)

    header_size = 6 + count * 2
    total = header_size + total_rle
    print(f"\n  图标集: {count}个 {icon_w}x{icon_h}, 总大小={total}B")

    with open(out_path, 'wb') as f:
        f.write(bytes([icon_w, icon_h, count, 0x01, 0x00, 0x00]))
        for o in offsets:
            f.write(bytes([o & 0xFF, (o >> 8) & 0xFF]))
        for rle in all_rle:
            f.write(rle)

    return total


def main():
    # 确保输出目录存在
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    print("=" * 50)
    print("生成菜单图标 (40×40, 4-bit灰度RLE)")
    print("=" * 50)

    # 生成6个图标
    generators = [
        ("speed", draw_speed_icon),
        ("smoke", draw_smoke_icon),
        ("pump", draw_pump_icon),
        ("color", draw_color_icon),
        ("rgb", draw_rgb_icon),
        ("bright", draw_bright_icon),
    ]

    icons = []
    for name, gen_func in generators:
        img = gen_func(ICON_SIZE)
        # 轻微高斯模糊增加抗锯齿效果
        img = img.filter(ImageFilter.GaussianBlur(radius=0.5))
        # 保存PNG供预览
        png_path = OUT_DIR / f"{name}.png"
        img.save(str(png_path))
        print(f"  ✅ {png_path}")
        icons.append(img)

    print(f"\n生成图标集 .bin ...")
    total = build_icon_sheet(icons, str(BIN_OUT))
    print(f"\n✅ 输出: {BIN_OUT} ({total} bytes)")
    print(f"✅ PNG预览: {OUT_DIR}/")
    print(f"\n提示: 查看 {OUT_DIR}/ 下的PNG预览效果")
    print("如果效果不满意, 可以用Figma/PS重新设计PNG, 然后用 icon_converter.py 转换")


if __name__ == '__main__':
    main()
