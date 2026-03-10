# screens/menu.py — UI1: 菜单导航界面

from screens import Screen
from screen_manager import UI_SPEED, UI_SMOKE, UI_PUMP, UI_PRESET, UI_RGB, UI_BRIGHT
from hal.input import EVT_CLICK
from ui.theme import (
    SCREEN_W, TOP_Y, TOP_H, MAIN_Y, MAIN_H, BOT_Y, BOT_H,
    WHITE, BLACK, GRAY, DARK_GRAY, ACCENT
)
from ui.font import get_font
from ui.widgets import draw_nav_dots

MENU_ITEMS = [
    ("WIND",   UI_SPEED,  "fan_speed",   "%"),
    ("SMOKE",  UI_SMOKE,  "smoke_speed", "%"),
    ("PUMP",   UI_PUMP,   "pump_speed",  "%"),
    ("COLOR",  UI_PRESET, "preset_idx",  ""),
    ("RGB",    UI_RGB,    None,          ""),
    ("LIGHT",  UI_BRIGHT, "brightness",  "%"),
]

# 各标签最大宽度（用空格补齐避免残影）
_MAX_LABEL = 5  # "SMOKE" = 5 chars


class MenuScreen(Screen):

    def __init__(self, ctx):
        super().__init__(ctx)
        self._icons = None
        self._prev_idx = -1

    def on_enter(self):
        self._prev_idx = -1
        try:
            from ui.bitmap import IconSheet
            self._icons = IconSheet('assets/icons_24.bin')
        except (OSError, ImportError):
            self._icons = None

    def on_input(self, event):
        state = self.ctx.state
        if event.delta != 0:
            state.menu_idx = (state.menu_idx + event.delta) % len(MENU_ITEMS)
        if event.key == EVT_CLICK:
            _, target, _, _ = MENU_ITEMS[state.menu_idx]
            self.ctx.screen_manager.switch(target)

    def draw_full(self, r):
        r.clear(BLACK)
        self._draw_top(r)
        self._draw_content(r)
        self._prev_idx = self.ctx.state.menu_idx

    def draw_update(self, r):
        state = self.ctx.state
        if state.menu_idx != self._prev_idx:
            # 只重绘主区+底栏，用覆盖式绘制
            self._draw_content(r)
            self._prev_idx = state.menu_idx

    def _draw_top(self, r):
        r.draw_text(get_font(8), "MENU", 4, TOP_Y + 2, ACCENT, BLACK)

    def _draw_content(self, r):
        state = self.ctx.state
        idx = state.menu_idx
        n = len(MENU_ITEMS)
        font16 = get_font(16)
        font8 = get_font(8)
        cx = SCREEN_W // 2

        # 中间大标签（固定宽度覆盖）
        label, _, attr, suffix = MENU_ITEMS[idx]
        padded = label.ljust(_MAX_LABEL)
        tw = font16.text_width(padded)
        r.draw_text(font16, padded, cx - tw // 2, MAIN_Y + 4, WHITE, BLACK)

        # 值预览（固定宽度覆盖）
        if attr:
            val_text = (str(getattr(state, attr, 0)) + suffix).ljust(6)
        else:
            val_text = "      "
        vw = font8.text_width(val_text)
        r.draw_text(font8, val_text, cx - vw // 2,
                    MAIN_Y + 4 + font16.height + 4, GRAY, BLACK)

        # 左侧小标签
        left_label = MENU_ITEMS[(idx - 1) % n][0].ljust(_MAX_LABEL)
        r.draw_text(font8, left_label, 8, MAIN_Y + 14, DARK_GRAY, BLACK)

        # 右侧小标签
        right_label = MENU_ITEMS[(idx + 1) % n][0].rjust(_MAX_LABEL)
        rw = font8.text_width(right_label)
        r.draw_text(font8, right_label, SCREEN_W - rw - 8, MAIN_Y + 14, DARK_GRAY, BLACK)

        # 底部导航圆点（先清底栏再画）
        r.fill_rect(0, BOT_Y, SCREEN_W, BOT_H, BLACK)
        draw_nav_dots(r, n, idx)

    def on_exit(self):
        self._icons = None
