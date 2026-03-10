# screens/menu.py — UI1: 菜单导航界面
# 新版: 居中大图标 + 文字标签 + 底部导航点

from screens import Screen
from screen_manager import UI_SPEED, UI_SMOKE, UI_PUMP, UI_PRESET, UI_RGB, UI_BRIGHT
from hal.input import EVT_CLICK
from ui.theme import SCREEN_W, SCREEN_H, WHITE, BLACK, GRAY, DARK_GRAY, ACCENT
from ui.font import get_font
from ui.widgets import draw_nav_dots

# (标签, 目标界面, 状态属性, 后缀, 图标索引/-1无图标)
MENU_ITEMS = [
    ("Speed",  UI_SPEED,  "fan_speed",   "%",  0),
    ("Smoke",  UI_SMOKE,  "smoke_speed", "%", -1),
    ("Pump",   UI_PUMP,   "pump_speed",  "%", -1),
    ("Color",  UI_PRESET, "preset_idx",  "",   1),
    ("RGB",    UI_RGB,    None,          "",   2),
    ("Bright", UI_BRIGHT, "brightness",  "%", -1),
]

# 布局常量
ICON_Y = 4
LABEL_GAP = 3
DOTS_Y = SCREEN_H - 6


def _pad(s, w):
    return s + ' ' * (w - len(s)) if len(s) < w else s


class MenuScreen(Screen):

    def __init__(self, ctx):
        super().__init__(ctx)
        self._icons = None
        self._prev_idx = -1

    def on_enter(self):
        self._prev_idx = -1
        try:
            from ui.icon import AlphaIconSheet
            self._icons = AlphaIconSheet('assets/menu_icons_alpha.bin')
        except (OSError, ImportError) as e:
            print('WARN: menu icons load failed:', e)
            self._icons = None

    def on_input(self, event):
        state = self.ctx.state
        if event.delta != 0:
            state.menu_idx = (state.menu_idx + event.delta) % len(MENU_ITEMS)
        if event.key == EVT_CLICK:
            _, target, _, _, _ = MENU_ITEMS[state.menu_idx]
            self.ctx.screen_manager.switch(target)

    def draw_full(self, r):
        r.clear(BLACK)
        self._draw_content(r)
        self._prev_idx = self.ctx.state.menu_idx

    def draw_update(self, r):
        state = self.ctx.state
        if state.menu_idx != self._prev_idx:
            self._draw_content(r)
            self._prev_idx = state.menu_idx

    def _draw_content(self, r):
        state = self.ctx.state
        idx = state.menu_idx
        n = len(MENU_ITEMS)
        font8 = get_font(8)
        display = r._display
        cx = SCREEN_W // 2

        # 清除全屏
        r.fill_rect(0, 0, SCREEN_W, SCREEN_H, BLACK)

        label, _, attr, suffix, icon_idx = MENU_ITEMS[idx]

        # ── 中间图标 ──
        has_icon = False
        if self._icons and icon_idx >= 0 and icon_idx < self._icons.count:
            iw = self._icons.icon_w
            ih = self._icons.icon_h
            ix = cx - iw // 2
            self._icons.draw(display, icon_idx, ix, ICON_Y, WHITE, BLACK)
            label_y = ICON_Y + ih + LABEL_GAP
            has_icon = True

        if not has_icon:
            # 无图标: 用大字体显示标签名
            font16 = get_font(16)
            padded = _pad(label, 6)
            tw = font16.text_width(padded)
            r.draw_text(font16, padded, cx - tw // 2, 16, WHITE, BLACK)
            label_y = 36

        # ── 文字标签 ──
        padded = _pad(label, 6)
        tw = font8.text_width(padded)
        r.draw_text(font8, padded, cx - tw // 2, label_y, WHITE, BLACK)

        # ── 底部导航圆点 ──
        draw_nav_dots(r, n, idx, cy=DOTS_Y)

    def on_exit(self):
        self._icons = None
