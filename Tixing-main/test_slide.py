# test_slide.py — 纯色方块滑动测试
# 部署: py -m mpremote cp test_slide.py :test_slide.py + exec "import test_slide"
# 不依赖任何项目代码，直接操作 SPI 验证滑动动画

from machine import Pin, SPI
from utime import sleep_ms, ticks_ms, ticks_diff
import gc

gc.collect()
print("=== slide test ===")
print("free:", gc.mem_free())

spi = SPI(0, baudrate=62_500_000, sck=Pin(2), mosi=Pin(3))
from drivers.st7789 import ST7789
disp = ST7789(spi=spi)
disp.init()

# 5 个 40x40 纯色方块 (不同颜色方便区分)
COLORS = [0xF800, 0x07E0, 0x001F, 0xFFE0, 0xF81F]  # R G B Y M
SZ = 40
ICON_Y = 4
SCREEN_W = 284
SPACING = 56

# 预渲染 5 个纯色 buffer
bufs = []
for c in COLORS:
    b = bytearray(SZ * SZ * 2)
    hi = (c >> 8) & 0xFF
    lo = c & 0xFF
    for i in range(0, len(b), 2):
        b[i] = hi
        b[i+1] = lo
    bufs.append(b)

# 画初始位置: 5 个方块均匀排列
CENTER = SCREEN_W // 2
positions = [CENTER + (s - 2) * SPACING for s in range(5)]

disp.fill(0)
for i in range(5):
    x = positions[i] - SZ // 2
    disp.blit_block(bufs[i], x, ICON_Y, SZ, SZ)

print("initial drawn, waiting 1s...")
sleep_ms(1000)

# 动画: 所有方块向左滑 56px (模拟切换到下一个菜单项)
FRAMES = 12
DELAY = 25
SHIFT = -SPACING  # 向左移动一个间距

print("starting animation: %d frames, %dms delay" % (FRAMES, DELAY))

prev_xs = [positions[i] - SZ // 2 for i in range(5)]

t_start = ticks_ms()

for frame in range(FRAMES):
    t0 = ticks_ms()
    progress = (frame + 1) * 256 // FRAMES  # 0..256 定点
    
    # 阶段1: 画所有方块到新位置
    new_xs = []
    for i in range(5):
        old_cx = positions[i]
        new_cx = old_cx + (SHIFT * progress >> 8)
        nx = new_cx - SZ // 2
        new_xs.append(nx)
        if 0 <= nx and nx + SZ <= SCREEN_W:
            disp.blit_block(bufs[i], nx, ICON_Y, SZ, SZ)
    
    # 阶段2: 擦所有方块的尾迹
    for i in range(5):
        nx = new_xs[i]
        px = prev_xs[i]
        if nx != px:
            if nx > px:
                # 右移, 擦左边
                el = max(0, px)
                er = min(nx, SCREEN_W)
                if er > el:
                    disp.fill_rect(el, ICON_Y, er - el, SZ, 0)
            else:
                # 左移, 擦右边
                el = max(0, nx + SZ)
                er = min(px + SZ, SCREEN_W)
                if er > el:
                    disp.fill_rect(el, ICON_Y, er - el, SZ, 0)
    
    prev_xs = new_xs
    
    t_frame = ticks_diff(ticks_ms(), t0)
    print("f%d: %dms" % (frame, t_frame))
    
    if frame < FRAMES - 1:
        sleep_ms(DELAY)

t_total = ticks_diff(ticks_ms(), t_start)
print("total: %dms" % t_total)
print("done")
