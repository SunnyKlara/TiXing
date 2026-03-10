#!/usr/bin/env python3
"""PC端菜单界面预览工具。

模拟76×284屏幕, 渲染图标集并显示预览窗口。
用于快速迭代UI设计, 不需要烧录到Pico。

用法: py tools/preview_menu.py
"""

import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("ERROR: 需要 Pillow。运行: pip install Pillow")
    sys.exit(1)

SCREEN_W = 284
SCREEN_H = 76
BIN_PATH = Path("assets/menu_icons_alpha.bin")
SCALE = 4  # 预览放大倍数


def load_icon_sheet(path):
    """加载图标集.bin, 返回(icon_w, icon_h, count, icons_gray4)"""
    with open(path, 'rb') as f:
        hdr = f.read(6)
        icon_w = hdr[0]
        icon_h = hdr[1]
        count = hdr[2]

        offsets = []
        for _ in range(count):
            ob = f.read(2)
            offsets.append(ob[0] | (ob[1] << 8))

        data = f.read()

    # 解码每个图标的RLE
    icons = []
    for i in range(count):
        offset = offsets[i]
        pixels = []
        di = offset
        total_px = icon_w * icon_h

        while len(pixels) < total_px:
            if di >= len(data):
                break
            c = data[di]
            if c == 0:
                break
            v = data[di + 1]
            di += 2
            need = min(c, total_px - len(pixels))
            pixels.extend([v] * need)

        # 补齐
        while len(pixels) < total_px:
            pixels.append(0)

        icons.append(pixels)

    return icon_w, icon_h, count, icons


def render_menu_preview(selected=0):
    """渲染菜单界面预览图。"""
    icon_w, icon_h, count, icons = load_icon_sheet(str(BIN_PATH))

    # 创建屏幕图像
    screen = Image.new('RGB', (SCREEN_W, SCREEN_H), (0, 0, 0))
    draw = ImageDraw.Draw(screen)

    # 菜单项名称
    names = ["Speed", "Smoke", "Pump", "Color", "RGB", "Bright"]

    # 布局: 图标居中显示, 下方文字, 底部导航点
    icon_y = 4
    text_y = icon_y + icon_h + 2
    dots_y = SCREEN_H - 8

    # 当前选中的图标居中
    icon_x = SCREEN_W // 2 - icon_w // 2

    # 渲染选中图标 (alpha混合)
    if selected < count:
        pixels = icons[selected]
        for py in range(icon_h):
            for px in range(icon_w):
                val = pixels[py * icon_w + px]
                if val > 0:
                    gray = val * 255 // 15
                    screen.putpixel((icon_x + px, icon_y + py), (gray, gray, gray))

    # 渲染文字 (简单像素字体模拟)
    if selected < len(names):
        name = names[selected]
        # 用Pillow默认字体
        try:
            font = ImageFont.truetype("arial.ttf", 10)
        except (OSError, IOError):
            font = ImageFont.load_default()
        bbox = draw.textbbox((0, 0), name, font=font)
        tw = bbox[2] - bbox[0]
        tx = SCREEN_W // 2 - tw // 2
        draw.text((tx, text_y), name, fill=(255, 255, 255), font=font)

    # 左侧小图标 (前一个, 缩小+暗色)
    prev_idx = (selected - 1) % count
    if prev_idx < count:
        small_size = icon_w * 2 // 3
        offset_x = 20
        offset_y = icon_y + (icon_h - small_size) // 2
        pixels = icons[prev_idx]
        for py in range(icon_h):
            for px in range(icon_w):
                # 简单缩放
                sy = py * small_size // icon_h
                sx = px * small_size // icon_w
                if sy < small_size and sx < small_size:
                    src_y = py
                    src_x = px
                    val = pixels[src_y * icon_w + src_x]
                    if val > 0:
                        gray = val * 120 // 15  # 暗一些
                        ty = offset_y + sy
                        tx = offset_x + sx
                        if 0 <= tx < SCREEN_W and 0 <= ty < SCREEN_H:
                            screen.putpixel((tx, ty), (gray, gray, gray))

    # 右侧小图标
    next_idx = (selected + 1) % count
    if next_idx < count:
        small_size = icon_w * 2 // 3
        offset_x = SCREEN_W - 20 - small_size
        offset_y = icon_y + (icon_h - small_size) // 2
        pixels = icons[next_idx]
        for py in range(icon_h):
            for px in range(icon_w):
                sy = py * small_size // icon_h
                sx = px * small_size // icon_w
                if sy < small_size and sx < small_size:
                    val = pixels[py * icon_w + px]
                    if val > 0:
                        gray = val * 120 // 15
                        ty = offset_y + sy
                        tx = offset_x + sx
                        if 0 <= tx < SCREEN_W and 0 <= ty < SCREEN_H:
                            screen.putpixel((tx, ty), (gray, gray, gray))

    # 底部导航圆点
    dot_spacing = 10
    total_dots_w = (count - 1) * dot_spacing
    dots_start_x = SCREEN_W // 2 - total_dots_w // 2
    for i in range(count):
        dx = dots_start_x + i * dot_spacing
        if i == selected:
            draw.ellipse([dx - 2, dots_y - 2, dx + 2, dots_y + 2],
                         fill=(255, 255, 255))
        else:
            draw.ellipse([dx - 1, dots_y - 1, dx + 1, dots_y + 1],
                         fill=(80, 80, 80))

    return screen


def main():
    if not BIN_PATH.exists():
        print(f"ERROR: {BIN_PATH} 不存在, 先运行 py tools/generate_menu_icons.py")
        sys.exit(1)

    print("生成菜单预览...")

    # 生成所有菜单项的预览
    previews = []
    for i in range(6):
        screen = render_menu_preview(selected=i)
        # 放大
        scaled = screen.resize((SCREEN_W * SCALE, SCREEN_H * SCALE),
                               Image.NEAREST)
        previews.append(scaled)

    # 拼接所有预览到一张大图
    gap = 8
    total_h = len(previews) * (SCREEN_H * SCALE + gap) - gap
    combined = Image.new('RGB', (SCREEN_W * SCALE + 40, total_h + 40), (30, 30, 30))
    names = ["Speed", "Smoke", "Pump", "Color", "RGB", "Bright"]

    for i, (preview, name) in enumerate(zip(previews, names)):
        y = 20 + i * (SCREEN_H * SCALE + gap)
        combined.paste(preview, (20, y))

    out_path = "assets/menu_preview.png"
    combined.save(out_path)
    print(f"✅ 预览已保存: {out_path}")
    print(f"   包含6个菜单项, {SCALE}x放大, 模拟76×284屏幕")

    # 也保存单个预览
    single = render_menu_preview(selected=0)
    single_scaled = single.resize((SCREEN_W * SCALE, SCREEN_H * SCALE), Image.NEAREST)
    single_path = "assets/menu_preview_single.png"
    single_scaled.save(single_path)
    print(f"✅ 单项预览: {single_path}")


if __name__ == '__main__':
    main()
