# screens/speed.py — UI2: 风速控制界面（含油门模式）
# 三区布局：顶栏 "WIND" + 主区 32px 大数字 + 底栏圆角进度条

from screens import Screen
from screen_manager import UI_MENU
from hal.input import EVT_DOUBLE, EVT_TRIPLE
from app_state import clamp
from ui.theme import (
    SCREEN_W, TOP_Y, TOP_H, MAIN_Y, MAIN_H, BOT_Y, BOT_H,
    WHITE, BLACK, ACCENT, WARNING, GRAY
)
from ui.font import get_font
from ui.widgets import draw_capsule_bar, draw_big_number, draw_hint


class SpeedScreen(Screen):
    """风速控制界面。

    旋转调速(步进1), 双击保存返回, 三击进入油门模式。
    """

    def __init__(self, ctx):
        super().__init__(ctx)
        self._prev_speed = -1
        self._prev_throttle = None

    def on_enter(self):
        self._prev_speed = -1
        self._prev_throttle = None
        # 进入界面时立即同步风扇到当前状态值
        self._sync_fan()

    def on_input(self, event):
        state = self.ctx.state

        if state.throttle_mode:
            if event.delta != 0:
                state.throttle_mode = False
                state.fan_speed = clamp(state.fan_speed + event.delta, 0, 100)
                self._sync_fan()
                return
            if self.ctx.input.raw_key_pressed():
                accel = 1 if state.fan_speed < 50 else 2
                state.fan_speed = min(100, state.fan_speed + accel)
            else:
                state.fan_speed = max(0, state.fan_speed - 1)
            self._sync_fan()
        else:
            if event.delta != 0:
                state.fan_speed = clamp(state.fan_speed + event.delta, 0, 100)
                self._sync_fan()
            if event.key == EVT_TRIPLE:
                state.throttle_mode = True
            if event.key == EVT_DOUBLE:
                self._save_and_back()

    def draw_full(self, r):
        state = self.ctx.state
        font8 = get_font(8)
        fg = WARNING if state.throttle_mode else ACCENT
        title = "THR " if state.throttle_mode else "WIND"

        r.draw_text(font8, title, 4, TOP_Y + 2, fg, BLACK)
        draw_big_number(r, state.fan_speed, SCREEN_W // 2, MAIN_Y + MAIN_H // 2)
        draw_capsule_bar(r, 20, BOT_Y + 4, SCREEN_W - 40, 12, state.fan_speed, fg)
        draw_hint(r, "3x:THR 2x:Back")

        self._prev_speed = state.fan_speed
        self._prev_throttle = state.throttle_mode

    def draw_update(self, r):
        state = self.ctx.state
        changed = False

        # 检查油门模式变化
        if state.throttle_mode != self._prev_throttle:
            font8 = get_font(8)
            fg = WARNING if state.throttle_mode else ACCENT
            # 用固定宽度覆盖，避免残影
            r.draw_text(font8, "THR " if state.throttle_mode else "WIND", 4, TOP_Y + 2, fg, BLACK)
            self._prev_throttle = state.throttle_mode
            changed = True

        # 检查数值变化
        if state.fan_speed != self._prev_speed or changed:
            fg = WARNING if state.throttle_mode else ACCENT
            # 大数字：用 bg=BLACK 直接覆盖旧内容
            draw_big_number(r, state.fan_speed, SCREEN_W // 2, MAIN_Y + MAIN_H // 2, bg=BLACK)
            # 进度条：覆盖式绘制
            draw_capsule_bar(r, 20, BOT_Y + 4, SCREEN_W - 40, 12, state.fan_speed, fg)
            self._prev_speed = state.fan_speed

    def _sync_fan(self):
        self.ctx.coordinator.sync_fan(self.ctx.state)

    def _save_and_back(self):
        self.ctx.config.save_from_state(self.ctx.state)
        self.ctx.state.throttle_mode = False
        self.ctx.screen_manager.switch(UI_MENU)
