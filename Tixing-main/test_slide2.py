# test_slide2.py — 完整菜单滑动模拟
# py -m mpremote cp test_slide2.py :test_slide2.py + exec "import test_slide2"
#
# 模拟效果:
# - 5个彩色方块横排, 中间大(40) 两侧中(30) 最外小(22)
# - 自动向左滑动一格, 滑动过程中大小跟位置连续变化
# - 滑完停1秒, 再向左滑一格, 共滑3次

from machine import Pin, SPI
from utime import sleep_ms, ticks_ms, ticks_diff
import gc

gc.collect()
print("=== slide2 test ===")

spi = SPI(0, baudrate=62_500_000, sck=Pin(2), mosi=Pin(3))
from drivers.st7789 import ST7789
disp = ST7789(spi=spi)
disp.init()

SCREEN_W = 284
ICON_Y = 4
SPACING = 56
CENTER = SCREEN_W // 2
FRAMES = 14
DELAY = 20

# 3种尺寸的纯色方块 buffer
SIZES = [40, 30, 22]
COLORS = [0xF800, 0x07E0, 0x001F, 0xFFE0, 0xF81F, 0x07FF]

def make_buf(sz, color):
    b = bytearray(sz * sz * 2)
    hi = (color >> 8) & 0xFF
    lo = color & 0xFF
    for i in range(0, len(b), 2):
        b[i] = hi
        b[i + 1] = lo
    return b

# 预渲染: 每个颜色 x 每个尺寸
icon_bufs = {}
for ci, c in enumerate(COLORS):
    icon_bufs[ci] = {}
    for sz in SIZES:
        icon_bufs[ci][sz] = make_buf(sz, c)

def pick_size(cx):
    """离中心越近越大"""
    d = abs(cx - CENTER)
    if d <= 20:
        return 40
    elif d <= SPACING:
        return 30
    else:
        return 22

def draw_state(color_indices, positions):
    """画当前状态: 先清图标区, 再画所有方块"""
    disp.fill_rect(0, ICON_Y, SCREEN_W, 40, 0)
    for i in range(len(positions)):
        cx = positions[i]
        sz = pick_size(cx)
        ci = color_indices[i]
        buf = icon_bufs[ci][sz]
        ix = cx - sz // 2
        iy = ICON_Y + (40 - sz) // 2
        if ix >= 0 and ix + sz <= SCREEN_W:
            disp.blit_block(buf, ix, iy, sz, sz)

def slide_one_step(color_indices, start_positions, direction):
    """滑动一格动画。direction: -1=左, +1=右"""
    shift = direction * SPACING
    
    # 记录上一帧每个图标的绘制区域 (ix, iy, sz)
    prev_rects = []
    for i in range(len(start_positions)):
        cx = start_positions[i]
        sz = pick_size(cx)
        ix = cx - sz // 2
        iy = ICON_Y + (40 - sz) // 2
        prev_rects.append((ix, iy, sz))
    
    for frame in range(FRAMES):
        last = (frame == FRAMES - 1)
        if last:
            progress = 256
        else:
            progress = (frame + 1) * 256 // FRAMES
        
        new_rects = []
        
        # 阶段1: 画所有图标到新位置
        for i in range(len(start_positions)):
            cx = start_positions[i] + (shift * progress >> 8)
            sz = pick_size(cx)
            ci = color_indices[i]
            buf = icon_bufs[ci][sz]
            ix = cx - sz // 2
            iy = ICON_Y + (40 - sz) // 2
            new_rects.append((ix, iy, sz))
            if ix >= 0 and ix + sz <= SCREEN_W:
                disp.blit_block(buf, ix, iy, sz, sz)
        
        # 阶段2: 擦每个图标的残留区域
        for i in range(len(start_positions)):
            pix, piy, psz = prev_rects[i]
            nix, niy, nsz = new_rects[i]
            
            # 水平方向尾迹
            if nix != pix:
                if nix > pix:
                    el = max(0, pix)
                    er = min(nix, SCREEN_W)
                    if er > el:
                        disp.fill_rect(el, ICON_Y, er - el, 40, 0)
                else:
                    el = max(0, nix + nsz)
                    er = min(pix + psz, SCREEN_W)
                    if er > el:
                        disp.fill_rect(el, ICON_Y, er - el, 40, 0)
            
            # 尺寸变化的上下残留
            if nsz != psz:
                # 上方
                if niy > piy:
                    disp.fill_rect(max(0, min(pix, nix)), piy,
                                   max(psz, nsz), niy - piy, 0)
                # 下方
                p_bot = piy + psz
                n_bot = niy + nsz
                if n_bot < p_bot:
                    disp.fill_rect(max(0, min(pix, nix)), n_bot,
                                   max(psz, nsz), p_bot - n_bot, 0)
        
        prev_rects = new_rects
        
        if not last:
            sleep_ms(DELAY)
    
    # 返回终点位置
    end_positions = [p + shift for p in start_positions]
    return end_positions

# 初始状态: 6个图标, 显示中间5个
# color_indices 对应 COLORS 里的颜色
all_colors = [0, 1, 2, 3, 4, 5]
cur_center = 2  # 当前中心是第2个

disp.fill(0)

# 画初始静态
positions = [CENTER + (s - 2) * SPACING for s in range(5)]
vis_colors = [(cur_center - 2 + s) % 6 for s in range(5)]
draw_state(vis_colors, positions)

print("ready, sliding in 1s...")
sleep_ms(1000)

# 连续滑3次, 每次向左一格
for step in range(3):
    print("slide %d" % (step + 1))
    t0 = ticks_ms()
    
    # 滑动时用6个位置(含即将进入的新图标)
    slide_colors = list(vis_colors)
    slide_pos = list(positions)
    # 右边加一个即将滑入的
    new_ci = (cur_center + 3) % 6
    slide_colors.append(new_ci)
    slide_pos.append(CENTER + 3 * SPACING)
    
    end_pos = slide_one_step(slide_colors, slide_pos, -1)
    
    # 更新状态
    cur_center = (cur_center + 1) % 6
    positions = [CENTER + (s - 2) * SPACING for s in range(5)]
    vis_colors = [(cur_center - 2 + s) % 6 for s in range(5)]
    
    # 画最终静态(清理残留)
    draw_state(vis_colors, positions)
    
    print("  %dms" % ticks_diff(ticks_ms(), t0))
    sleep_ms(1000)

print("done")
