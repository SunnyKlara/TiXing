# tools/png_to_rgb565.py — PNG → RGB565 bin 转换
# 用法: python tools/png_to_rgb565.py
#
# 读取 assets/icons_png/*.png, 缩放到 40x40,
# 透明区域填黑色, 输出 RGB565 大端 .bin 到 assets/icons_bin/

from PIL import Image
import os
import struct

SRC_DIR = 'assets/icons_png'
DST_DIR = 'assets/icons_bin'
TARGET_SZ = 64
ALL_SIZES = [64, 40]

os.makedirs(DST_DIR, exist_ok=True)

names = ['speed', 'smoke', 'color', 'rgb', 'bright']

for name in names:
    src = os.path.join(SRC_DIR, f'{name}.png')
    img_orig = Image.open(src).convert('RGBA')
    
    for sz in ALL_SIZES:
        img = img_orig.copy()
        img.thumbnail((sz, sz), Image.LANCZOS)
        bg = Image.new('RGBA', (sz, sz), (0, 0, 0, 255))
        ox = (sz - img.width) // 2
        oy = (sz - img.height) // 2
        bg.paste(img, (ox, oy), img)
        rgb = bg.convert('RGB')
        
        buf = bytearray(sz * sz * 2)
        idx = 0
        for y in range(sz):
            for x in range(sz):
                r, g, b = rgb.getpixel((x, y))
                c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                buf[idx] = (c >> 8) & 0xFF
                buf[idx + 1] = c & 0xFF
                idx += 2
        
        dst = os.path.join(DST_DIR, f'{name}_{sz}.bin')
        with open(dst, 'wb') as f:
            f.write(buf)
        
        print(f'{name}_{sz}: {img.width}x{img.height} -> {sz}x{sz} -> {dst} ({len(buf)} bytes)')

print('done')
