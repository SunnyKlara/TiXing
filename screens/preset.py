# screens/preset.py — UI5: 灯光预设界面

from screens import Screen
from screen_manager import UI_MENU
from hal.input import EVT_CLICK, EVT_DOUBLE
from ui.theme import (
    SCREEN_W, TOP_Y, TOP_H, MAIN_Y, MAIN_H, BOT_Y, BOT_H,
    WHITE, BLACK, GRAY, ACCENT, PRESETS, rgb888_to_565
)
from ui.font import get_font
from ui.widgets import draw_gradient_bar, draw_status_dot, draw_hint


class PresetScreen(Screen):

    def __init__(self, ctx):
        super().__init__(ctx)
        self._prev_idx = -1
        self._prev_grad = None

    def on_enter(self):
        self._prev_idx = -1
        self._prev_grad = None
        self._apply_preset()

    def on_input(self, event):
        state = self.ctx.state
        if event.delta != 0:
            state.preset_idx = (state.preset_idx + event.delta) % len(PRESETS)
            self._apply_preset()
        if event.key == EVT_CLICK:
            state.gradient_mode = not state.gradient_mode
        if event.key == EVT_DOUBLE:
            self.ctx.config.save_from_state(state)
            self.ctx.screen_manager.switch(UI_MENU)

    def _apply_preset(self):
        state = self.ctx.state
        _, c1, c2 = PRESETS[state.preset_idx]
        state.cob1_rgb = list(c1)
        state.cob2_rgb = list(c2)
        br = state.brightness / 100.0
        d = self.ctx.devices
        d.set_cob1(int(c1[0] * br), int(c1[1] * br), int(c1[2] * br))
        d.set_cob2(int(c2[0] * br), int(c2[1] * br), int(c2[2] * br))

    def draw_full(self, r):
        self._draw_all(r)
        self._prev_idx = self.ctx.state.preset_idx
        self._prev_grad = self.ctx.state.gradient_mode

    def draw_update(self, r):
        state = self.ctx.state
        if state.preset_idx != self._prev_idx or state.gradient_mode != self._prev_grad:
            self._draw_all(r)
            self._prev_idx = state.preset_idx
            self._prev_grad = state.gradient_mode

    def _draw_all(self, r):
        state = self.ctx.state
        idx = state.preset_idx
        name, c1, c2 = PRESETS[idx]
        font8 = get_font(8)

        # 顶栏（覆盖式）
        num_text = "{}/{}".format(idx + 1, len(PRESETS))
        num_text = num_text + ' ' * (5 - len(num_text)) if len(num_text) < 5 else num_text
        r.draw_text(font8, num_text, 4, TOP_Y + 2, ACCENT, BLACK)
        padded_name = name + ' ' * (8 - len(name)) if len(name) < 8 else name
        nw = font8.text_width(padded_name)
        r.draw_text(font8, padded_name, SCREEN_W // 2 - nw // 2, TOP_Y + 2, WHITE, BLACK)

        mode_label = "GRD" if state.gradient_mode else "FIX"
        mlw = font8.text_width(mode_label)
        r.draw_text(font8, mode_label, SCREEN_W - mlw - 16, TOP_Y + 2, GRAY, BLACK)
        draw_status_dot(r, SCREEN_W - 6, TOP_Y + 6, 3, state.gradient_mode)

        # 主区渐变色条
        bar_x, bar_y, bar_w, bar_h = 20, MAIN_Y + 8, SCREEN_W - 40, 24
        if c1 == c2:
            r.fill_rect(bar_x, bar_y, bar_w, bar_h, rgb888_to_565(*c1))
        else:
            draw_gradient_bar(r, bar_x, bar_y, bar_w, bar_h, c1, c2)

        # 底栏色块
        bw, bh, gap = 20, 12, 8
        total = bw * 2 + gap
        bx = SCREEN_W // 2 - total // 2
        by = BOT_Y + 4
        r.fill_rect(bx, by, bw, bh, rgb888_to_565(*c1))
        r.fill_rect(bx + bw + gap, by, bw, bh, rgb888_to_565(*c2))
        r.draw_text(font8, "C1", bx, by + bh + 1, GRAY, BLACK)
        r.draw_text(font8, "C2", bx + bw + gap, by + bh + 1, GRAY, BLACK)
        draw_hint(r)
