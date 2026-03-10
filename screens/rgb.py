# screens/rgb.py — UI6: RGB 调色界面（三层状态机）

from screens import Screen
from screen_manager import UI_MENU
from hal.input import EVT_CLICK, EVT_DOUBLE
from app_state import clamp
from ui.theme import (
    SCREEN_W, SCREEN_H, TOP_Y, TOP_H, MAIN_Y, MAIN_H,
    WHITE, BLACK, RED, GREEN, BLUE, GRAY, ACCENT, rgb888_to_565
)
from ui.font import get_font
from ui.widgets import draw_rgb_channel_bar, draw_hint

_STRIP_NAMES = ("COB1", "COB2")
_MODE_LABELS = ("Strip> ", "Chan>  ", "Val>   ")
_CH_COLORS = (RED, GREEN, BLUE)


class RGBScreen(Screen):

    def __init__(self, ctx):
        super().__init__(ctx)
        self._prev_mode = -1
        self._prev_strip = -1
        self._prev_channel = -1
        self._prev_vals = None

    def on_enter(self):
        self.ctx.state.rgb_mode = 0
        self._prev_mode = -1
        self._prev_strip = -1
        self._prev_channel = -1
        self._prev_vals = None

    def on_input(self, event):
        state = self.ctx.state
        if event.delta != 0:
            if state.rgb_mode == 0:
                state.rgb_strip = (state.rgb_strip + event.delta) % 2
            elif state.rgb_mode == 1:
                state.rgb_channel = (state.rgb_channel + event.delta) % 3
            elif state.rgb_mode == 2:
                vals = state.cob1_rgb if state.rgb_strip == 0 else state.cob2_rgb
                vals[state.rgb_channel] = clamp(
                    vals[state.rgb_channel] + event.delta * 2, 0, 255)
                self._sync_cob()

        if event.key == EVT_CLICK:
            if state.rgb_mode < 2:
                state.rgb_mode += 1
            else:
                state.rgb_mode = 1

        if event.key == EVT_DOUBLE:
            state.rgb_mode = 0
            self.ctx.config.save_from_state(state)
            self.ctx.screen_manager.switch(UI_MENU)

    def _sync_cob(self):
        state = self.ctx.state
        br = state.brightness / 100.0
        d = self.ctx.devices
        c1, c2 = state.cob1_rgb, state.cob2_rgb
        d.set_cob1(int(c1[0] * br), int(c1[1] * br), int(c1[2] * br))
        d.set_cob2(int(c2[0] * br), int(c2[1] * br), int(c2[2] * br))

    def draw_full(self, r):
        self._draw_all(r)
        self._snapshot()

    def draw_update(self, r):
        state = self.ctx.state
        vals = state.cob1_rgb if state.rgb_strip == 0 else state.cob2_rgb
        if (state.rgb_mode != self._prev_mode or
            state.rgb_strip != self._prev_strip or
            state.rgb_channel != self._prev_channel or
            list(vals) != self._prev_vals):
            self._draw_all(r)
            self._snapshot()

    def _snapshot(self):
        state = self.ctx.state
        self._prev_mode = state.rgb_mode
        self._prev_strip = state.rgb_strip
        self._prev_channel = state.rgb_channel
        vals = state.cob1_rgb if state.rgb_strip == 0 else state.cob2_rgb
        self._prev_vals = list(vals)

    def _draw_all(self, r):
        state = self.ctx.state
        font8 = get_font(8)

        # 顶栏（覆盖式，固定宽度）
        r.draw_text(font8, _MODE_LABELS[state.rgb_mode], 4, TOP_Y + 2, ACCENT, BLACK)
        strip_name = _STRIP_NAMES[state.rgb_strip]
        strip_name = strip_name + ' ' * (4 - len(strip_name))
        r.draw_text(font8, strip_name, 60, TOP_Y + 2, WHITE, BLACK)

        # 颜色预览
        vals = state.cob1_rgb if state.rgb_strip == 0 else state.cob2_rgb
        r.fill_rect(SCREEN_W - 20, TOP_Y + 1, 16, 10, rgb888_to_565(*vals))

        # 三行通道条
        start_y = MAIN_Y + 4
        for i in range(3):
            y = start_y + i * 12
            active = (state.rgb_mode >= 1 and state.rgb_channel == i)
            draw_rgb_channel_bar(r, y, i, vals[i], active, _CH_COLORS[i])

        draw_hint(r)
