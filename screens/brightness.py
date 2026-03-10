# screens/brightness.py — UI7: 亮度调节界面

from screens import Screen
from screen_manager import UI_MENU
from hal.input import EVT_CLICK, EVT_DOUBLE
from app_state import clamp
from ui.theme import (
    SCREEN_W, TOP_Y, TOP_H, MAIN_Y, MAIN_H, BOT_Y,
    WHITE, BLACK, ACCENT, GRAY
)
from ui.font import get_font
from ui.widgets import draw_capsule_bar, draw_big_number, draw_status_dot, draw_hint


class BrightnessScreen(Screen):

    def __init__(self, ctx):
        super().__init__(ctx)
        self._prev_br = -1
        self._prev_breath = None

    def on_enter(self):
        self._prev_br = -1
        self._prev_breath = None

    def on_input(self, event):
        state = self.ctx.state
        if event.delta != 0:
            state.brightness = clamp(state.brightness + event.delta, 0, 100)
        if event.key == EVT_CLICK:
            state.breath_mode = not state.breath_mode
        if event.key == EVT_DOUBLE:
            self.ctx.config.save_from_state(state)
            self.ctx.screen_manager.switch(UI_MENU)

    def draw_full(self, r):
        state = self.ctx.state
        font8 = get_font(8)

        r.draw_text(font8, "BRIGHT", 4, TOP_Y + 2, ACCENT, BLACK)
        self._draw_breath_label(r, state)
        draw_big_number(r, state.brightness, SCREEN_W // 2, MAIN_Y + MAIN_H // 2)
        draw_capsule_bar(r, 20, BOT_Y + 4, SCREEN_W - 40, 12, state.brightness, ACCENT)
        draw_hint(r, "1x:Breath 2x:Back")

        self._prev_br = state.brightness
        self._prev_breath = state.breath_mode

    def draw_update(self, r):
        state = self.ctx.state
        changed = False

        if state.breath_mode != self._prev_breath:
            self._draw_breath_label(r, state)
            self._prev_breath = state.breath_mode
            changed = True

        if state.brightness != self._prev_br or changed:
            draw_big_number(r, state.brightness, SCREEN_W // 2, MAIN_Y + MAIN_H // 2, bg=BLACK)
            draw_capsule_bar(r, 20, BOT_Y + 4, SCREEN_W - 40, 12, state.brightness, ACCENT)
            self._prev_br = state.brightness

    def _draw_breath_label(self, r, state):
        font8 = get_font(8)
        # 固定宽度覆盖，避免残影
        label = "Breath:ON " if state.breath_mode else "Breath:OFF"
        lw = font8.text_width(label)
        r.draw_text(font8, label, SCREEN_W - lw - 16, TOP_Y + 2, GRAY, BLACK)
        draw_status_dot(r, SCREEN_W - 6, TOP_Y + 6, 3, state.breath_mode)
