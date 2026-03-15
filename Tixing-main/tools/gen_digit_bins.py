# tools/gen_digit_bins.py — 设计师数字 PNG → RGB565 bin 转换
# 用法: python tools/gen_digit_bins.py
#
# 读取 assets/digits_png/0.png ~ 9.png
# 缩放到两套尺寸, 透明区域填黑, 输出 RGB565 大端 .bin
# 输出: assets/digits_bin/ 和 icons/

from PIL import Image
import os
import shutil

SRC_DIR = 'assets/digits_png'
DST_DIR = 'assets/digits_bin'
RUNTIME_DIR = 'icons'

# 大号 24×40 (中间), 小号 14×24 (两侧)
ALL_SIZES = [24, 14]
SIZE_CFG = {
    24: (24, 40),   # w, h
    14: (14, 24),
}

os.makedirs(DST_DIR, exist_ok=True)
os.makedirs(RUNTIME_DIR, exist_ok=True)

for digit in range(10):
    src = os.path.join(SRC_DIR, '%d.png' % digit)
    img_orig = Image.open(src).convert('RGBA')

    for sz_key in ALL_SIZES:
        w, h = SIZE_CFG[sz_key]

        img = img_orig.copy()
        img.thumbnail((w, h), Image.LANCZOS)

        # 黑底居中
        bg = Image.new('RGBA', (w, h), (0, 0, 0, 255))
        ox = (w - img.width) // 2
        oy = (h - img.height) // 2
        bg.paste(img, (ox, oy), img)
        rgb = bg.convert('RGB')

        # RGB565 大端
        buf = bytearray(w * h * 2)
        idx = 0
        for y in range(h):
            for x in range(w):
                r, g, b = rgb.getpixel((x, y))
                c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                buf[idx] = (c >> 8) & 0xFF
                buf[idx + 1] = c & 0xFF
                idx += 2

        dst = os.path.join(DST_DIR, 'digit_%d_%d.bin' % (digit, sz_key))
        with open(dst, 'wb') as f:
            f.write(buf)

        # 复制到 runtime
        shutil.copy2(dst, os.path.join(RUNTIME_DIR, 'digit_%d_%d.bin' % (digit, sz_key)))
        print('%d_%d: %dx%d -> %s (%d bytes)' % (digit, sz_key, w, h, dst, len(buf)))

print('done')
