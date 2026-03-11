#!/usr/bin/env python3
"""PC端菜单滚轮界面预览工具。

模拟76×284屏幕, 渲染滑动式菜单滚轮界面。
选中项居中放大高亮, 两侧项缩小淡化。

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

# 布局常量 (与 screens/menu.py 一致)
ITEM_SPACING = 64
CENTER_X = SCREEN_W // 2  # 142
ICON_Y = 4
LABEL_Y_BIG = 40
LABEL_Y_SMALL = 38
DOTS_Y = SCREEN_H - 6
MAX_VISIBLE = 5

# 颜色
ACCENT_RGB = (0, 215, 255)   # 蓝青强调色
WHITE_RGB = (255, 255, 255)
GRAY_RGB = (120, 120, 120)
BLACK_RGB = (0, 0, 0)

NAMES = ["Speed", "Smoke", "Pump", "Color", "RGB", "Bright"]


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
        while len(pixels) < total_px:
            pixels.append(0)
        icons.append(pixels)
    return icon_w, icon_h, count, icons


def calc_visible_items(selected_idx, total_count):
    """与 screens/menu.py 中的 calc_visible_items 逻辑一致。"""
    result = []
    seen = set()
    half = MAX_VISIBLE // 2
    offsets = [0]
    for d in range(1, half + 1):
        offsets.append(-d)
        offsets.append(d)

    for offset in offsets:
        item_idx = (selected_idx + offset) % total_count
        cx = CENTER_X + offset * ITEM_SPACING
        if cx < -ITEM_SPACING // 2 or cx > SCREEN_W + ITEM_SPACING // 2:
            continue
        if item_idx in seen:
            continue
        seen.add(item_idx)

        if offset == 0:
            icon_size = 32
            fg_color = ACCENT_RGB
            font_size = 16
        elif abs(offset) == 1:
            icon_size = 20
            fg_color = WHITE_RGB
            font_size = 16
        else:
            icon_size = 16
            fg_color = GRAY_RGB
            font_size = 16

        result.append((item_idx, cx, icon_size, fg_color, font_size))
    return result


def draw_icon(screen, pixels, icon_w, icon_h, cx, cy, size, fg_color):
    """在screen上绘制缩放后的图标, alpha混合前景色。"""
    half = size // 2
    for py in range(size):
        for px in range(size):
            # 从原始图标采样
            src_y = py * icon_h // size
            src_x = px * icon_w // size
            val = pixels[src_y * icon_w + src_x]
            if val > 0:
                alpha = val / 15.0
                r = int(fg_color[0] * alpha)
                g = int(fg_color[1] * alpha)
                b = int(fg_color[2] * alpha)
                sx = cx - half + px
                sy = cy + py
                if 0 <= sx < SCREEN_W and 0 <= sy < SCREEN_H:
                    screen.putpixel((sx, sy), (r, g, b))


def render_menu_wheel(selected=0):
    """渲染滑动式菜单滚轮预览图。"""
    icon_w, icon_h, count, icons = load_icon_sheet(str(BIN_PATH))
    screen = Image.new('RGB', (SCREEN_W, SCREEN_H), BLACK_RGB)
    draw = ImageDraw.Draw(screen)

    items = calc_visible_items(selected, len(NAMES))

    # 先绘制侧边项, 再绘制选中项 (确保选中项在最上层)
    center_item = None
    side_items = []
    for item in items:
        if item[3] == ACCENT_RGB:
            center_item = item
        else:
            side_items.append(item)

    try:
        font_big = ImageFont.truetype("arial.ttf", 12)
        font_small = ImageFont.truetype("arial.ttf", 9)
    except (OSError, IOError):
        font_big = ImageFont.load_default()
        font_small = font_big

    for item_idx, cx, icon_size, fg_color, font_size in side_items:
        # 绘制图标
        if item_idx < count:
            draw_icon(screen, icons[item_idx], icon_w, icon_h,
                      cx, ICON_Y, icon_size, fg_color)
        # 绘制标签
        name = NAMES[item_idx]
        font = font_small
        bbox = draw.textbbox((0, 0), name, font=font)
        tw = bbox[2] - bbox[0]
        draw.text((cx - tw // 2, LABEL_Y_SMALL), name,
                  fill=fg_color, font=font)

    if center_item:
        item_idx, cx, icon_size, fg_color, font_size = center_item
        if item_idx < count:
            draw_icon(screen, icons[item_idx], icon_w, icon_h,
                      cx, ICON_Y, icon_size, fg_color)
        name = NAMES[item_idx]
        font = font_big
        bbox = draw.textbbox((0, 0), name, font=font)
        tw = bbox[2] - bbox[0]
        draw.text((cx - tw // 2, LABEL_Y_BIG), name,
                  fill=fg_color, font=font)

    # 底部导航圆点
    dot_spacing = 10
    total_dots_w = (len(NAMES) - 1) * dot_spacing
    dots_start_x = SCREEN_W // 2 - total_dots_w // 2
    for i in range(len(NAMES)):
        dx = dots_start_x + i * dot_spacing
        if i == selected:
            draw.ellipse([dx - 3, DOTS_Y - 3, dx + 3, DOTS_Y + 3],
                         fill=ACCENT_RGB)
        else:
            draw.ellipse([dx - 1, DOTS_Y - 1, dx + 1, DOTS_Y + 1],
                         fill=(60, 60, 60))

    return screen


def main():
    if not BIN_PATH.exists():
        print(f"ERROR: {BIN_PATH} 不存在, 先运行 py tools/generate_menu_icons.py")
        sys.exit(1)

    print("生成滑动式菜单滚轮预览...")

    # 生成所有菜单项的预览
    previews = []
    for i in range(6):
        s = render_menu_wheel(selected=i)
        scaled = s.resize((SCREEN_W * SCALE, SCREEN_H * SCALE), Image.NEAREST)
        previews.append(scaled)

    # 拼接所有预览
    gap = 8
    total_h = len(previews) * (SCREEN_H * SCALE + gap) - gap
    combined = Image.new('RGB', (SCREEN_W * SCALE + 40, total_h + 40), (30, 30, 30))
    for i, preview in enumerate(previews):
        y = 20 + i * (SCREEN_H * SCALE + gap)
        combined.paste(preview, (20, y))

    out_path = "assets/menu_preview.png"
    combined.save(out_path)
    print(f"✅ 预览已保存: {out_path}")
    print(f"   包含6个菜单项滚轮状态, {SCALE}x放大, 模拟76×284屏幕")

    # 单项预览
    single = render_menu_wheel(selected=0)
    single_scaled = single.resize((SCREEN_W * SCALE, SCREEN_H * SCALE), Image.NEAREST)
    single_path = "assets/menu_preview_single.png"
    single_scaled.save(single_path)
    print(f"✅ 单项预览: {single_path}")


if __name__ == '__main__':
    main()
