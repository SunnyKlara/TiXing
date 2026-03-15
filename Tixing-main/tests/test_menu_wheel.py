# tests/test_menu_wheel.py — 3图标轮播菜单测试

import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from hypothesis import given, strategies as st, settings
from screens.menu import (
    MenuWheelScreen, MENU_ITEMS, ITEM_COUNT, MAX_SLOTS,
    CENTER, SPACING, BIG, SMALL, _pick_size,
)
from ui.theme import BLACK
from hal.input import EVT_CLICK, EVT_NONE
from app_state import AppState


class MockDisplay:
    def __init__(self):
        self.calls = []
    def fill(self, color=0):
        self.calls.append(('fill', color))
    def fill_rect(self, x, y, w, h, color):
        self.calls.append(('fill_rect', x, y, w, h, color))
    def blit_block(self, buf, x, y, w, h):
        self.calls.append(('blit_block', x, y, w, h))

class MockRenderer:
    def __init__(self):
        self._display = MockDisplay()
        self.calls = []
    def clear(self, color=0):
        self._display.fill(color)
        self.calls.append(('clear', color))

class MockEvent:
    __slots__ = ('delta', 'key')
    def __init__(self, delta=0, key=EVT_NONE):
        self.delta = delta
        self.key = key

class MockInput:
    def poll(self):
        return MockEvent()

class MockScreenManager:
    def __init__(self):
        self.switched_to = None
    def switch(self, target):
        self.switched_to = target

def _make_ctx():
    from screens import Context
    r = MockRenderer()
    state = AppState()
    ctx = Context(r, None, None, state, None, None)
    ctx.screen_manager = MockScreenManager()
    ctx.input = MockInput()
    return ctx


# ── pick_size 测试 ──

class TestPickSize:
    def test_center_is_big(self):
        assert _pick_size(CENTER) == BIG

    def test_offset_is_small(self):
        assert _pick_size(CENTER + SPACING) == SMALL
        assert _pick_size(CENTER - SPACING) == SMALL

    def test_near_center_is_big(self):
        assert _pick_size(CENTER + 20) == BIG
        assert _pick_size(CENTER - 20) == BIG


# ── 菜单项配置测试 ──

class TestMenuConfig:
    def test_item_count(self):
        assert ITEM_COUNT == 5

    def test_max_slots(self):
        assert MAX_SLOTS == 3

    def test_all_items_have_target(self):
        for name, target, prefix in MENU_ITEMS:
            assert target is not None
            assert len(prefix) > 0


# ── 索引边界测试 ──

class TestIndexBounds:
    @given(deltas=st.lists(st.integers(-10, 10), min_size=1, max_size=50))
    @settings(max_examples=50, deadline=None)
    def test_menu_idx_always_valid(self, deltas):
        ctx = _make_ctx()
        screen = MenuWheelScreen(ctx)
        for d in deltas:
            screen.on_input(MockEvent(delta=d))
            assert 0 <= ctx.state.menu_idx < ITEM_COUNT


# ── 状态隔离测试 ──

class TestStateIsolation:
    @given(delta=st.integers(-10, 10))
    @settings(max_examples=30)
    def test_rotation_only_changes_menu_idx(self, delta):
        ctx = _make_ctx()
        s = ctx.state
        before = dict(fan_speed=s.fan_speed, smoke_speed=s.smoke_speed,
                      brightness=s.brightness, preset_idx=s.preset_idx)
        MenuWheelScreen(ctx).on_input(MockEvent(delta=delta))
        for k, v in before.items():
            assert getattr(s, k) == v


# ── 点击映射测试 ──

class TestClickMapping:
    @given(idx=st.integers(0, ITEM_COUNT - 1))
    @settings(max_examples=20)
    def test_click_switches_to_correct_target(self, idx):
        ctx = _make_ctx()
        ctx.state.menu_idx = idx
        MenuWheelScreen(ctx).on_input(MockEvent(key=EVT_CLICK))
        assert ctx.screen_manager.switched_to == MENU_ITEMS[idx][1]


# ── 绘制测试 ──

class TestDrawLogic:
    def test_draw_full_draws_something(self):
        ctx = _make_ctx()
        screen = MenuWheelScreen(ctx)
        screen.on_enter()
        r = ctx.renderer
        screen.draw_full(r)
        # 应该有 blit_block 调用 (画图标)
        assert any(c[0] == 'blit_block' for c in r._display.calls)

    def test_on_enter_on_exit_lifecycle(self):
        ctx = _make_ctx()
        screen = MenuWheelScreen(ctx)
        screen.on_enter()
        assert len(screen._bufs) == ITEM_COUNT
        screen.on_exit()
        # on_exit 不释放 buffer
        assert len(screen._bufs) == ITEM_COUNT
