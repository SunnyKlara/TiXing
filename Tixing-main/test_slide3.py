# test_slide3.py — 3图标轮播菜单测试 (v5)
# 布局: 284×76 屏幕, 3个可见图标
# 中间 64×64 垂直居中, 左右 40×40 垂直居中
# 间距 96px (中心到中心), 三个图标均匀分布

from machine import Pin, SPI
from utime import sleep_ms, ticks_ms, ticks_diff
from hal.input import Input, EVT_CLICK
import gc

gc.collect()
print("=== slide3 v5: 3 icons ===")

spi = SPI(0, baudrate=62_500_000, sck=Pin(2), mosi=Pin(3))
from drivers.st7789 import ST7789
disp = ST7789(spi=spi)
disp.init()
inp = Input()

SCREEN_W = 284
SCREEN_H = 76
SPACING = 96          # 中心间距: (284/2 - 40/2) ≈ 左图标中心在 46, 中间142, 右238
CENTER = SCREEN_W // 2  # 142
FRAMES = 10
DELAY = 10
BIG = 64              # 中间图标尺寸
SMALL = 40            # 两侧图标尺寸
ICON_Y = (SCREEN_H - BIG) // 2  # 垂直居中基准 = 6
ITEM_COUNT = 5
MAX_SLOTS = 3         # 可见3个

NAMES = ['speed', 'smoke', 'color', 'rgb', 'bright']

# 加载预渲染 bin
bufs = {}
for ci, name in enumerate(NAMES):
    bufs[ci] = {}
    for sz in (BIG, SMALL):
        with open('icons/%s_%d.bin' % (name, sz), 'rb') as f:
            bufs[ci][sz] = f.read()
    print('loaded %s' % name)

gc.collect()
print('free:', gc.mem_free())


def pick_size(cx):
    """离中心近 → 大, 远 → 小"""
    if abs(cx - CENTER) <= 30:
        return BIG
    return SMALL


def icon_y(sz):
    """垂直居中"""
    return ICON_Y + (BIG - sz) // 2


def draw_static(center_idx):
    """画静态3个图标"""
    disp.fill_rect(0, 0, SCREEN_W, SCREEN_H, 0)
    for s in range(MAX_SLOTS):
        offset = s - 1  # -1, 0, +1
        ci = (center_idx + offset) % ITEM_COUNT
        cx = CENTER + offset * SPACING
        sz = pick_size(cx)
        ix = cx - sz // 2
        iy = icon_y(sz)
        if ix >= 0 and ix + sz <= SCREEN_W:
            disp.blit_block(bufs[ci][sz], ix, iy, sz, sz)


def do_slide(center_idx, direction):
    shift = -direction * SPACING

    # 3个可见 + 1个即将进入(根据方向) = 4个参与动画
    anim_items = []
    if direction > 0:
        # 顺时针: 选下一个, 图标左移, 右边滑入
        offsets = [-1, 0, 1, 2]
    else:
        # 逆时针: 选上一个, 图标右移, 左边滑入
        offsets = [-2, -1, 0, 1]
    for s in offsets:
        ci = (center_idx + s) % ITEM_COUNT
        start_cx = CENTER + s * SPACING
        anim_items.append((ci, start_cx))
    n = len(anim_items)

    for frame in range(FRAMES):
        last = (frame == FRAMES - 1)
        if last:
            p = 256
        else:
            p = (frame + 1) * 256 // FRAMES

        # 一次擦整行 (284×64 ≈ 2ms)
        disp.fill_rect(0, ICON_Y, SCREEN_W, BIG, 0)

        # 画所有图标
        for i in range(n):
            ci, scx = anim_items[i]
            cx = scx + (shift * p >> 8)
            sz = pick_size(cx)
            ix = cx - sz // 2
            iy = icon_y(sz)
            if ix >= 0 and ix + sz <= SCREEN_W:
                disp.blit_block(bufs[ci][sz], ix, iy, sz, sz)

        if not last:
            sleep_ms(DELAY)

    new_center = (center_idx + direction) % ITEM_COUNT
    return new_center


# ── 主循环 ──
disp.fill(0)
cur = 0
draw_static(cur)
print("turn encoder, click to exit")

animating = False
while True:
    evt = inp.poll()
    if evt.key == EVT_CLICK:
        print("exit")
        break
    if evt.delta != 0:
        print("delta=%d" % evt.delta)
        if not animating:
            animating = True
            d = 1 if evt.delta > 0 else -1
            t0 = ticks_ms()
            cur = do_slide(cur, d)
            print("slide %dms" % ticks_diff(ticks_ms(), t0))
            # 清掉动画期间积累的所有delta
            while True:
                e = inp.poll()
                if e.delta == 0:
                    break
            animating = False
    sleep_ms(10)
