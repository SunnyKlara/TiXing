# screens/smoke.py — UI3: 发烟控制界面

from screens import Screen
from screen_manager import UI_MENU
from hal.input import EVT_DOUBLE
from app_state import clamp
from ui.theme import SCREEN_W, TOP_Y, MAIN_Y, MAIN_H, BOT_Y, BLACK, ACCENT
from ui.font import get_font
from ui.widgets import draw_capsule_bar, draw_big_number, draw_hint


class SmokeScreen(Screen):

    def __init__(self, ctx):
        super().__init__(ctx)
        self._prev = -1

    def on_enter(self):
        self._prev = -1

    def on_input(self, event):
        state = self.ctx.state
        if event.delta != 0:
            state.smoke_speed = clamp(state.smoke_speed + event.delta, 0, 100)
            self.ctx.coordinator.sync_smoke(state)
        if event.key == EVT_DOUBLE:
            self.ctx.config.save_from_state(state)
            self.ctx.screen_manager.switch(UI_MENU)

    def draw_full(self, r):
        state = self.ctx.state
        r.draw_text(get_font(8), "SMOKE", 4, TOP_Y + 2, ACCENT, BLACK)
        draw_big_number(r, state.smoke_speed, SCREEN_W // 2, MAIN_Y + MAIN_H // 2)
        draw_capsule_bar(r, 20, BOT_Y + 4, SCREEN_W - 40, 12, state.smoke_speed, ACCENT)
        draw_hint(r)
        self._prev = state.smoke_speed

    def draw_update(self, r):
        state = self.ctx.state
        if state.smoke_speed != self._prev:
            draw_big_number(r, state.smoke_speed, SCREEN_W // 2, MAIN_Y + MAIN_H // 2, bg=BLACK)
            draw_capsule_bar(r, 20, BOT_Y + 4, SCREEN_W - 40, 12, state.smoke_speed, ACCENT)
            self._prev = state.smoke_speed
