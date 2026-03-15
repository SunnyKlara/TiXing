# test_digit_slide.py — 数字轮播测试 (0-340, 5个可见)
# 布局: 284×76 屏幕, 5个可见数字
# 中间大号, 两侧小号, 滚轮滑动切换
# 用 0-9 单字符 bin 做字模, 运行时拼多位数

from machine import Pin, SPI
from utime import sleep_ms, ticks_ms, ticks_diff
from hal.input import Input, EVT_CLICK
import gc

gc.collect()
print("=== digit slide 0-340 ===")

spi = SPI(0, baudrate=62_500_000, sck=Pin(2), mosi=Pin(3))
from drivers.st7789 import ST7789
disp = ST7789(spi=spi)
disp.init()
inp = Input()

SCREEN_W = 284
SCREEN_H = 76
CENTER = SCREEN_W // 2
SPACING = 72
FRAMES = 3
DELAY = 3

BIG_W = 24
BIG_H = 40
SMALL_W = 14
SMALL_H = 24

NUM_MIN = 0
NUM_MAX = 340
NUM_COUNT = NUM_MAX - NUM_MIN + 1
VISIBLE = 5
STEP = 5                     # 每次滚轮跳的步长
ICON_Y = (SCREEN_H - BIG_H) // 2

# 加载 0-9 字模 (两套尺寸)
glyph = {}
for d in range(10):
    glyph[d] = {}
    for sz in (BIG_W, SMALL_W):
        with open('icons/digit_%d_%d.bin' % (d, sz), 'rb') as f:
            glyph[d][sz] = f.read()
print('glyphs loaded')

gc.collect()
print('free:', gc.mem_free())


def pick_size(cx):
    dist = abs(cx - CENTER)
    if dist <= 20:
        return (BIG_W, BIG_H)
    return (SMALL_W, SMALL_H)


def digit_y(h):
    return ICON_Y + (BIG_H - h) // 2


def draw_number(num, cx, char_w, char_h):
    """在 cx 居中绘制多位数, 每个字符用 blit_block"""
    s = str(num)
    gap = 2
    total_w = len(s) * char_w + (len(s) - 1) * gap
    sx = cx - total_w // 2
    iy = digit_y(char_h)
    for ch in s:
        d = ord(ch) - 48
        buf = glyph[d][char_w]
        if sx >= 0 and sx + char_w <= SCREEN_W:
            disp.blit_block(buf, sx, iy, char_w, char_h)
        sx += char_w + gap


def draw_static(center_val):
    disp.fill_rect(0, 0, SCREEN_W, SCREEN_H, 0)
    for s in range(VISIBLE):
        offset = s - 2
        val = (center_val + offset * STEP) % NUM_COUNT
        cx = CENTER + offset * SPACING
        w, h = pick_size(cx)
        draw_number(val, cx, w, h)


def do_slide(center_val, direction):
    shift = -direction * SPACING

    anim_items = []
    if direction > 0:
        offsets = [-2, -1, 0, 1, 2, 3]
    else:
        offsets = [-3, -2, -1, 0, 1, 2]
    for s in offsets:
        val = (center_val + s * STEP) % NUM_COUNT
        start_cx = CENTER + s * SPACING
        anim_items.append((val, start_cx))
    n = len(anim_items)

    for frame in range(FRAMES):
        last = (frame == FRAMES - 1)
        if last:
            p = 256
        else:
            p = (frame + 1) * 256 // FRAMES

        disp.fill_rect(0, ICON_Y, SCREEN_W, BIG_H, 0)

        for i in range(n):
            val, scx = anim_items[i]
            cx = scx + (shift * p >> 8)
            w, h = pick_size(cx)
            draw_number(val, cx, w, h)

        if not last:
            sleep_ms(DELAY)

    return (center_val + direction * STEP) % NUM_COUNT


# ── 主循环 ──
disp.fill(0)
cur = 0
draw_static(cur)
print("turn encoder: 0-340, click to exit")

animating = False
while True:
    evt = inp.poll()
    if evt.key == EVT_CLICK:
        print("exit, val=%d" % cur)
        break
    if evt.delta != 0:
        if not animating:
            animating = True
            d = 1 if evt.delta > 0 else -1
            t0 = ticks_ms()
            cur = do_slide(cur, d)
            print("slide %dms val=%d" % (ticks_diff(ticks_ms(), t0), cur))
            while True:
                e = inp.poll()
                if e.delta == 0:
                    break
            animating = False
    sleep_ms(10)
