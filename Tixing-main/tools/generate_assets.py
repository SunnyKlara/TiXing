#!/usr/bin/env python3
"""一键生成所有 assets/ 资源文件。

无需外部 TTF 字体，使用 Pillow 内置字体渲染。
生成:
  - assets/font_16.bin  (16px 数字+符号字体)
  - assets/font_32.bin  (32px 大数字字体)
  - assets/logo.bin     (开机 Logo 位图 RGB565)
  - assets/icons_24.bin (6个 24x24 菜单图标)

用法: py tools/generate_assets.py
"""

import os
import struct
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("ERROR: 需要 Pillow。运行: pip install Pillow")
    sys.exit(1)

ASSETS_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'assets')


def ensure_assets_dir():
    os.makedirs(ASSETS_DIR, exist_ok=True)


# ─────────────────────────────────────────────
# 字体生成（列优先位图格式）
# ─────────────────────────────────────────────

def get_font(size):
    """尝试获取等宽字体，回退到默认字体。"""
    # 尝试系统等宽字体
    candidates = [
        "consola.ttf",      # Windows Consolas
        "cour.ttf",         # Windows Courier New
        "lucon.ttf",        # Windows Lucida Console
        "DejaVuSansMono.ttf",
        "LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    ]
    for name in candidates:
        try:
            return ImageFont.truetype(name, size)
        except (OSError, IOError):
            continue
    # 最终回退：Pillow 默认字体（不支持 size 参数，但能用）
    print(f"  WARN: 未找到系统等宽字体，使用 Pillow 默认字体")
    return ImageFont.load_default()


def render_char(font, ch, target_w, target_h):
    """渲染单个字符为 target_w x target_h 的灰度图像（4-bit抗锯齿）。"""
    # 先渲染到较大画布
    canvas = Image.new('L', (target_w * 2, target_h * 2), 0)
    draw = ImageDraw.Draw(canvas)
    
    bbox = draw.textbbox((0, 0), ch, font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    
    # 居中绘制
    x = (target_w * 2 - tw) // 2 - bbox[0]
    y = (target_h * 2 - th) // 2 - bbox[1]
    draw.text((x, y), ch, fill=255, font=font)
    
    # 缩放到目标尺寸（保留灰度抗锯齿）
    canvas = canvas.resize((target_w, target_h), Image.LANCZOS)
    return canvas


def generate_font_bin(size, chars, out_name):
    """生成 4-bit 抗锯齿 .bin 字体文件。
    
    格式: Header(5B) = width(1B) + height(1B) + first_char(1B) + char_count(1B) + bpp(1B)
    字形: 每字符 = width * height / 2 字节 (4-bit, 每字节2像素, 高4位在前)
    """
    print(f"  生成 {out_name} ({size}px, {len(chars)} chars, 4-bit AA)...")
    font = get_font(size)
    
    # 确定字形尺寸（等宽，宽度取偶数方便打包）
    glyph_w = max(size * 5 // 8, 4)
    if glyph_w % 2 != 0:
        glyph_w += 1
    glyph_h = size
    bytes_per_glyph = glyph_w * glyph_h // 2
    
    first_char = ord(min(chars))
    last_char = ord(max(chars))
    char_count = last_char - first_char + 1
    
    glyphs = bytearray()
    for code in range(first_char, first_char + char_count):
        ch = chr(code)
        if ch in chars:
            img = render_char(font, ch, glyph_w, glyph_h)
            # 4-bit 逐行编码: 每字节存2像素 (高4位=左像素, 低4位=右像素)
            data = bytearray(bytes_per_glyph)
            di = 0
            for y in range(glyph_h):
                for x in range(0, glyph_w, 2):
                    v0 = img.getpixel((x, y)) >> 4      # 0-15
                    v1 = img.getpixel((x + 1, y)) >> 4  # 0-15
                    data[di] = (v0 << 4) | v1
                    di += 1
            glyphs.extend(data)
        else:
            glyphs.extend(bytes(bytes_per_glyph))
    
    out_path = os.path.join(ASSETS_DIR, out_name)
    with open(out_path, 'wb') as f:
        f.write(bytes([glyph_w, glyph_h, first_char, char_count, 4]))  # bpp=4
        f.write(glyphs)
    
    total = 5 + len(glyphs)
    print(f"    → {out_path} ({glyph_w}x{glyph_h}, {char_count} chars, 4-bit AA, {total} bytes)")


# ─────────────────────────────────────────────
# Logo 位图生成 (RGB565)
# ─────────────────────────────────────────────

def rgb888_to_565_be(r, g, b):
    """RGB888 → RGB565 big-endian 2 bytes."""
    val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return struct.pack('>H', val)


def generate_logo():
    """生成开机 Logo 位图 (120x40)。"""
    print("  生成 logo.bin...")
    w, h = 120, 40
    img = Image.new('RGB', (w, h), (0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    font = get_font(28)
    
    # 绘制 "TURBO" 文字
    text = "TURBO"
    bbox = draw.textbbox((0, 0), text, font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    x = (w - tw) // 2 - bbox[0]
    y = (h - th) // 2 - bbox[1]
    
    # 渐变色文字效果：从青色到蓝色
    for i, ch in enumerate(text):
        t = i / max(len(text) - 1, 1)
        r = int(0 * (1 - t) + 100 * t)
        g = int(235 * (1 - t) + 150 * t)
        b = int(255 * (1 - t) + 255 * t)
        ch_bbox = draw.textbbox((0, 0), ch, font=font)
        ch_w = ch_bbox[2] - ch_bbox[0]
        draw.text((x, y), ch, fill=(r, g, b), font=font)
        x += ch_w + 1
    
    # 底部装饰线
    draw.line([(10, h - 6), (w - 10, h - 6)], fill=(0, 180, 255), width=1)
    
    # 转换为 RGB565
    out_path = os.path.join(ASSETS_DIR, 'logo.bin')
    with open(out_path, 'wb') as f:
        f.write(struct.pack('<HH', w, h))
        for py in range(h):
            for px in range(w):
                r, g, b = img.getpixel((px, py))
                f.write(rgb888_to_565_be(r, g, b))
    
    total = 4 + w * h * 2
    print(f"    → {out_path} ({w}x{h}, {total} bytes)")


# ─────────────────────────────────────────────
# 图标集生成 (6个 24x24 菜单图标)
# ─────────────────────────────────────────────

def draw_icon_wind(draw, s):
    """风扇图标：三条弧线。"""
    for i in range(3):
        y = 4 + i * 7
        draw.arc([(4, y), (s - 4, y + 10)], 0, 180, fill=(0, 235, 255), width=2)

def draw_icon_smoke(draw, s):
    """烟雾图标：波浪线。"""
    import math
    for row in range(3):
        y_base = 6 + row * 6
        points = []
        for x in range(4, s - 4):
            y = y_base + int(2 * math.sin((x + row * 4) * 0.5))
            points.append((x, y))
        if len(points) > 1:
            draw.line(points, fill=(180, 180, 180), width=1)

def draw_icon_pump(draw, s):
    """气泵图标：圆形+箭头。"""
    draw.ellipse([(4, 4), (s - 4, s - 4)], outline=(0, 200, 255), width=2)
    draw.line([(s // 2, 8), (s // 2, s - 8)], fill=(0, 200, 255), width=2)
    draw.line([(s // 2 - 4, s // 2 - 2), (s // 2, 8)], fill=(0, 200, 255), width=1)
    draw.line([(s // 2 + 4, s // 2 - 2), (s // 2, 8)], fill=(0, 200, 255), width=1)

def draw_icon_color(draw, s):
    """颜色预设图标：调色板。"""
    draw.ellipse([(3, 3), (s - 3, s - 3)], outline=(255, 100, 0), width=2)
    draw.ellipse([(8, 8), (12, 12)], fill=(255, 0, 0))
    draw.ellipse([(14, 6), (18, 10)], fill=(0, 255, 0))
    draw.ellipse([(8, 14), (12, 18)], fill=(0, 100, 255))

def draw_icon_rgb(draw, s):
    """RGB 图标：三个重叠圆。"""
    draw.ellipse([(4, 2), (14, 12)], outline=(255, 0, 0), width=1)
    draw.ellipse([(10, 2), (20, 12)], outline=(0, 255, 0), width=1)
    draw.ellipse([(7, 8), (17, 18)], outline=(0, 100, 255), width=1)

def draw_icon_light(draw, s):
    """亮度图标：太阳。"""
    c = s // 2
    draw.ellipse([(c - 5, c - 5), (c + 5, c + 5)], fill=(255, 210, 0))
    for angle in range(0, 360, 45):
        import math
        rad = math.radians(angle)
        x1 = c + int(7 * math.cos(rad))
        y1 = c + int(7 * math.sin(rad))
        x2 = c + int(10 * math.cos(rad))
        y2 = c + int(10 * math.sin(rad))
        draw.line([(x1, y1), (x2, y2)], fill=(255, 210, 0), width=1)


def generate_icons():
    """生成 6 个 24x24 菜单图标集。"""
    print("  生成 icons_24.bin...")
    s = 24
    icon_funcs = [
        draw_icon_wind,
        draw_icon_smoke,
        draw_icon_pump,
        draw_icon_color,
        draw_icon_rgb,
        draw_icon_light,
    ]
    count = len(icon_funcs)
    
    out_path = os.path.join(ASSETS_DIR, 'icons_24.bin')
    with open(out_path, 'wb') as f:
        f.write(struct.pack('<HHH', s, s, count))
        for func in icon_funcs:
            img = Image.new('RGB', (s, s), (0, 0, 0))
            draw = ImageDraw.Draw(img)
            func(draw, s)
            for py in range(s):
                for px in range(s):
                    r, g, b = img.getpixel((px, py))
                    f.write(rgb888_to_565_be(r, g, b))
    
    total = 6 + count * s * s * 2
    print(f"    → {out_path} ({count} icons @ {s}x{s}, {total} bytes)")


# ─────────────────────────────────────────────
# 主入口
# ─────────────────────────────────────────────

def main():
    print("=" * 50)
    print("Pico Turbo 资源文件生成器")
    print("=" * 50)
    
    ensure_assets_dir()
    
    # 1. 16px 字体（数字 + 常用 ASCII 符号）
    chars_16 = "0123456789%.:/-ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz "
    generate_font_bin(16, chars_16, 'font_16.bin')
    
    # 2. 32px 大数字字体（仅数字 + % + .）
    chars_32 = "0123456789%."
    generate_font_bin(32, chars_32, 'font_32.bin')
    
    # 3. Logo 位图
    generate_logo()
    
    # 4. 菜单图标集
    generate_icons()
    
    print()
    print("全部完成！assets/ 目录内容：")
    for f in sorted(os.listdir(ASSETS_DIR)):
        if f.endswith('.bin'):
            path = os.path.join(ASSETS_DIR, f)
            size = os.path.getsize(path)
            print(f"  {f:20s} {size:>6d} bytes")
    print()
    print("这些 .bin 文件需要和 .py 文件一起烧录到 Pico。")


if __name__ == '__main__':
    main()
