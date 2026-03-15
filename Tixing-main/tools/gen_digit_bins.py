# tools/gen_digit_bins.py — 生成 0-9 数字的 RGB565 bin 文件
# 用法: python tools/gen_digit_bins.py
#
# 输出: assets/digits_bin/digit_N_SZ.bin (N=0-9, SZ=48,28)
# 白色数字, 黑色背景, 居中渲染

from PIL import Image, ImageDraw, ImageFont
import os

DST_DIR = 'assets/digits_bin'
RUNTIME_DIR = 'icons'  # 部署到设备的目录

# 大号24×40 (中间), 小号14×24 (两侧)
SIZES = {
    24: {'w': 24, 'h': 40, 'font_size': 34},
    14: {'w': 14, 'h': 24, 'font_size': 20},
}

os.makedirs(DST_DIR, exist_ok=True)

# 尝试加载等宽字体
FONT_PATHS = [
    'C:/Windows/Fonts/consola.ttf',
    'C:/Windows/Fonts/arial.ttf',
    'C:/Windows/Fonts/calibri.ttf',
    '/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf',
    '/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf',
]

def load_font(size):
    for fp in FONT_PATHS:
        try:
            return ImageFont.truetype(fp, size)
        except (OSError, IOError):
            continue
    return ImageFont.load_default()

for digit in range(10):
    ch = str(digit)
    for sz_key, cfg in SIZES.items():
        w, h = cfg['w'], cfg['h']
        font = load_font(cfg['font_size'])

        img = Image.new('RGB', (w, h), (0, 0, 0))
        draw = ImageDraw.Draw(img)

        bbox = draw.textbbox((0, 0), ch, font=font)
        tw = bbox[2] - bbox[0]
        th = bbox[3] - bbox[1]
        tx = (w - tw) // 2 - bbox[0]
        ty = (h - th) // 2 - bbox[1]
        draw.text((tx, ty), ch, fill=(255, 255, 255), font=font)

        # 转 RGB565 大端
        buf = bytearray(w * h * 2)
        idx = 0
        for y in range(h):
            for x in range(w):
                r, g, b = img.getpixel((x, y))
                c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                buf[idx] = (c >> 8) & 0xFF
                buf[idx + 1] = c & 0xFF
                idx += 2

        dst = os.path.join(DST_DIR, 'digit_%d_%d.bin' % (digit, sz_key))
        with open(dst, 'wb') as f:
            f.write(buf)
        print('%s_%d: %dx%d -> %s (%d bytes)' % (ch, sz_key, w, h, dst, len(buf)))

# 也复制到 runtime 目录
os.makedirs(RUNTIME_DIR, exist_ok=True)
import shutil
for f in os.listdir(DST_DIR):
    if f.startswith('digit_'):
        shutil.copy2(os.path.join(DST_DIR, f), os.path.join(RUNTIME_DIR, f))
        
print('done - files in %s and %s' % (DST_DIR, RUNTIME_DIR))
