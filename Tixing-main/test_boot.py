# test_boot.py — Logo + 底部渐变进度条测试
# 部署: py -m mpremote cp test_boot.py :test_boot.py + exec "import test_boot"

from machine import Pin, SPI
from utime import sleep_ms, ticks_ms, ticks_diff
import struct
import gc

gc.collect()
print("=== boot progress bar test ===")

spi = SPI(0, baudrate=62_500_000, sck=Pin(2), mosi=Pin(3))
from drivers.st7789 import ST7789
disp = ST7789(spi=spi)
disp.init()

W = 284
H = 76

# 加载并显示 Logo
with open('assets/logo.bin', 'rb') as f:
    hdr = f.read(4)
    lw, lh = struct.unpack('<HH', hdr)
    logo = f.read()
disp.blit_block(logo, 0, 0, lw, lh)
del logo
gc.collect()

# ── 进度条参数 ──
BAR_H = 3           # 进度条高度 3px
BAR_Y = H - BAR_H   # 贴底部
BAR_W = W            # 全宽
STEPS = 40           # 分 40 步走完
STEP_W = (BAR_W + STEPS - 1) // STEPS  # 每步像素宽度
DURATION = 2000      # 总时长 2 秒
DELAY = DURATION // STEPS

# 纯白色进度条
WHITE = 0xFFFF

print("progress bar: %d steps, %dms each" % (STEPS, DELAY))
t0 = ticks_ms()

for i in range(STEPS):
    x = i * STEP_W
    w = min(STEP_W, BAR_W - x)
    if w <= 0:
        break
    disp.fill_rect(x, BAR_Y, w, BAR_H, WHITE)
    sleep_ms(DELAY)

print("done: %dms" % ticks_diff(ticks_ms(), t0))
print("holding...")
