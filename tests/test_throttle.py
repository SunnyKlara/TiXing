# tests/test_throttle.py — 油门模式属性测试
# 属性 7: ∀ throttle_mode == True: 旋转事件 → throttle_mode 重置为 False

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from hypothesis import given, strategies as st, settings
from app_state import AppState
from hal.input import InputEvent, EVT_NONE, EVT_TRIPLE


class MockCtx:
    def __init__(self):
        self.state = AppState()
        self.coordinator = MockCoordinator()
        self.config = MockConfig()
        self.screen_manager = MockSM()
        self.input = MockInput()
        self.renderer = None
        self.devices = None
        self.effects = None

class MockCoordinator:
    def sync_fan(self, state): pass
class MockConfig:
    def save_from_state(self, state): pass
class MockSM:
    def switch(self, ui_id): pass
class MockInput:
    def __init__(self):
        self._pressed = False
    def raw_key_pressed(self):
        return self._pressed


class TestThrottleProperties:

    def _make_speed_screen(self):
        from screens.speed import SpeedScreen
        ctx = MockCtx()
        return SpeedScreen(ctx), ctx

    @given(delta=st.integers(min_value=-10, max_value=10).filter(lambda x: x != 0))
    @settings(max_examples=30)
    def test_rotate_exits_throttle_mode(self, delta):
        """∀ throttle_mode == True: 旋转事件 → throttle_mode = False。"""
        screen, ctx = self._make_speed_screen()
        ctx.state.throttle_mode = True
        ctx.state.fan_speed = 50

        event = InputEvent()
        event.delta = delta
        event.key = EVT_NONE
        screen.on_input(event)

        assert ctx.state.throttle_mode is False

    @given(speed=st.integers(min_value=0, max_value=100))
    @settings(max_examples=50)
    def test_throttle_speed_always_clamped(self, speed):
        """油门模式下风速始终在 [0,100]。"""
        screen, ctx = self._make_speed_screen()
        ctx.state.throttle_mode = True
        ctx.state.fan_speed = speed
        ctx.input._pressed = True

        # 模拟多帧加速
        event = InputEvent()
        event.delta = 0
        event.key = EVT_NONE
        for _ in range(200):
            screen.on_input(event)

        assert 0 <= ctx.state.fan_speed <= 100

    @given(speed=st.integers(min_value=0, max_value=100))
    @settings(max_examples=50)
    def test_throttle_decel_speed_clamped(self, speed):
        """松开后减速，风速不低于 0。"""
        screen, ctx = self._make_speed_screen()
        ctx.state.throttle_mode = True
        ctx.state.fan_speed = speed
        ctx.input._pressed = False

        event = InputEvent()
        event.delta = 0
        event.key = EVT_NONE
        for _ in range(200):
            screen.on_input(event)

        assert ctx.state.fan_speed == 0

    def test_triple_click_activates_throttle(self):
        """三击激活油门模式。"""
        screen, ctx = self._make_speed_screen()
        ctx.state.throttle_mode = False

        event = InputEvent()
        event.delta = 0
        event.key = EVT_TRIPLE
        screen.on_input(event)

        assert ctx.state.throttle_mode is True
