# screens/menu.py — 3图标轮播菜单 (基于 test_slide3 验证通过的方案)
# 布局: 284×76, 3个可见图标, 中间64大两侧40小
# 滑动: 直接 blit_block + fill_rect 整行擦除, 零帧缓冲

from screens import Screen
from screen_manager import UI_MENU, UI_SPEED, UI_SMOKE, UI_PRESET, UI_RGB, UI_BRIGHT
from hal.input import EVT_CLICK

try:
    from utime import sleep_ms
except ImportError:
    from time import sleep
    def sleep_ms(ms):
        sleep(ms / 1000.0)

SCREEN_W = 284
SCREEN_H = 76
ICON_Y = 6
SPACING = 96
CENTER = SCREEN_W // 2
FRAMES = 10
DELAY = 10
BIG = 64
SMALL = 40

# 5个菜单项: (name, ui_target, icon_file_prefix)
MENU_ITEMS = [
    ("Speed",  UI_SPEED,  "speed"),
    ("Smoke",  UI_SMOKE,  "smoke"),
    ("Color",  UI_PRESET, "color"),
    ("RGB",    UI_RGB,    "rgb"),
    ("Bright", UI_BRIGHT, "bright"),
]
ITEM_COUNT = len(MENU_ITEMS)
MAX_SLOTS = 3


def _pick_size(cx):
    if abs(cx - CENTER) <= 30:
        return BIG
    return SMALL


def _icon_y(sz):
    return ICON_Y + (BIG - sz) // 2


class MenuWheelScreen(Screen):

    def __init__(self, ctx):
        super().__init__(ctx)
        self._bufs = {}
        self._prev_idx = -1

    def on_enter(self):
        self._prev_idx = -1
        self._load_icons()

    def _load_icons(self):
        self._bufs = {}
        for ci, (_, _, prefix) in enumerate(MENU_ITEMS):
            self._bufs[ci] = {}
            for sz in (BIG, SMALL):
                try:
                    with open('icons/%s_%d.bin' % (prefix, sz), 'rb') as f:
                        self._bufs[ci][sz] = f.read()
                except OSError:
                    # 生成纯色占位 buffer
                    colors = [0xF800, 0x07E0, 0x001F, 0xFFE0, 0xF81F]
                    c = colors[ci % len(colors)]
                    b = bytearray(sz * sz * 2)
                    hi = (c >> 8) & 0xFF
                    lo = c & 0xFF
                    for i in range(0, len(b), 2):
                        b[i] = hi
                        b[i + 1] = lo
                    self._bufs[ci][sz] = b

    def _draw_static(self, disp, center_idx):
        disp.fill_rect(0, 0, SCREEN_W, SCREEN_H, 0)
        for s in range(MAX_SLOTS):
            offset = s - 1
            ci = (center_idx + offset) % ITEM_COUNT
            cx = CENTER + offset * SPACING
            sz = _pick_size(cx)
            ix = cx - sz // 2
            iy = _icon_y(sz)
            buf = self._bufs.get(ci, {}).get(sz)
            if buf and ix >= 0 and ix + sz <= SCREEN_W:
                disp.blit_block(buf, ix, iy, sz, sz)

    def _do_slide(self, disp, center_idx, direction):
        shift = -direction * SPACING

        anim_items = []
        if direction > 0:
            offsets = [-1, 0, 1, 2]
        else:
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

            disp.fill_rect(0, ICON_Y, SCREEN_W, BIG, 0)

            for i in range(n):
                ci, scx = anim_items[i]
                cx = scx + (shift * p >> 8)
                sz = _pick_size(cx)
                ix = cx - sz // 2
                iy = _icon_y(sz)
                buf = self._bufs.get(ci, {}).get(sz)
                if buf and ix >= 0 and ix + sz <= SCREEN_W:
                    disp.blit_block(buf, ix, iy, sz, sz)

            if not last:
                sleep_ms(DELAY)

        return (center_idx + direction) % ITEM_COUNT

    def on_input(self, event):
        state = self.ctx.state
        disp = self.ctx.renderer._display

        if event.delta != 0:
            d = 1 if event.delta > 0 else -1
            old_idx = state.menu_idx
            new_idx = self._do_slide(disp, old_idx, d)
            state.menu_idx = new_idx
            # 清掉动画期间积累的 delta
            self.ctx.input.poll()
            self._prev_idx = new_idx

        if event.key == EVT_CLICK:
            _, target, _ = MENU_ITEMS[state.menu_idx]
            self.ctx.screen_manager.switch(target)

    def draw_full(self, r):
        disp = r._display
        self._draw_static(disp, self.ctx.state.menu_idx)
        self._prev_idx = self.ctx.state.menu_idx

    def draw_update(self, r):
        # 动画在 on_input 里直接驱动, draw_update 不需要做什么
        pass

    def on_exit(self):
        # 保留 icon buffer, 不释放 (返回菜单时不用重新加载)
        pass


MenuScreen = MenuWheelScreen
