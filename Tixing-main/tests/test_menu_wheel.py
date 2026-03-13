# tests/test_menu_wheel.py — 滑动式菜单滚轮测试

import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from hypothesis import given, strategies as st, settings
from screens.menu import (
    calc_visible_items, MenuWheelScreen, MENU_ITEMS,
    SCREEN_W, ITEM_SPACING, CENTER_X, MAX_VISIBLE, ITEM_COUNT, DOTS_Y,
)
from ui.theme import ACCENT, WHITE, GRAY, BLACK
from hal.input import EVT_CLICK, EVT_NONE
from app_state import AppState


class MockDisplay:
    def __init__(self):
        self.calls = []
    def fill(self, color=0):
        self.calls.append(('fill', color))
    def fill_rect(self, x, y, w, h, color):
        self.calls.append(('fill_rect', x, y, w, h, color))
    def blit(self, data, x, y, w, h):
        self.calls.append(('blit', x, y, w, h))

class MockRenderer:
    def __init__(self):
        self._display = MockDisplay()
        self.calls = []
    def clear(self, color=0):
        self._display.fill(color)
        self.calls.append(('clear', color))
    def fill_rect(self, x, y, w, h, color):
        self._display.fill_rect(x, y, w, h, color)
        self.calls.append(('fill_rect', x, y, w, h, color))
    def draw_text(self, font, text, x, y, fg, bg=0):
        self.calls.append(('draw_text', text, x, y, fg))
        return len(text) * 8
    def draw_gradient_text(self, font, text, x, y, c1, c2, bg=0):
        self.calls.append(('draw_gradient_text', text, x, y, c1, c2))
        return len(text) * 8
    def draw_dot(self, cx, cy, r, color):
        self.calls.append(('draw_dot', cx, cy, r, color))

class MockEvent:
    __slots__ = ('delta', 'key')
    def __init__(self, delta=0, key=EVT_NONE):
        self.delta = delta
        self.key = key

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
    return ctx


class TestProperty1VisibleItemCount:
    @given(total=st.integers(1, 20), data=st.data())
    @settings(max_examples=50)
    def test_count_le_max_visible(self, total, data):
        idx = data.draw(st.integers(0, total - 1))
        assert len(calc_visible_items(idx, total)) <= MAX_VISIBLE


class TestProperty2SelectedCentered:
    @given(total=st.integers(1, 20), data=st.data())
    @settings(max_examples=50)
    def test_exactly_one_accent_at_center(self, total, data):
        idx = data.draw(st.integers(0, total - 1))
        items = calc_visible_items(idx, total)
        accent = [i for i in items if i[2] == ACCENT]
        assert len(accent) == 1
        assert accent[0][1] == CENTER_X


class TestProperty3ColorGradient:
    @given(idx=st.integers(0, 5))
    @settings(max_examples=20)
    def test_color_gradient_with_6_items(self, idx):
        for _, cx, fg, is_c in calc_visible_items(idx, 6):
            off = (cx - CENTER_X) // ITEM_SPACING if ITEM_SPACING else 0
            if off == 0:
                assert fg == ACCENT and is_c
            elif abs(off) == 1:
                assert fg == WHITE
            else:
                assert fg == GRAY


class TestProperty4CoordinateBounds:
    @given(total=st.integers(1, 20), data=st.data())
    @settings(max_examples=50)
    def test_cx_within_bounds(self, total, data):
        idx = data.draw(st.integers(0, total - 1))
        lo, hi = -ITEM_SPACING // 2, SCREEN_W + ITEM_SPACING // 2
        for _, cx, _, _ in calc_visible_items(idx, total):
            assert lo <= cx <= hi


class TestProperty5UniqueIndices:
    @given(total=st.integers(1, 20), data=st.data())
    @settings(max_examples=50)
    def test_no_duplicate_item_idx(self, total, data):
        idx = data.draw(st.integers(0, total - 1))
        indices = [i[0] for i in calc_visible_items(idx, total)]
        assert len(indices) == len(set(indices))


class TestProperty6IndexBounds:
    @given(deltas=st.lists(st.integers(-10, 10), min_size=1, max_size=50))
    @settings(max_examples=50)
    def test_menu_idx_always_valid(self, deltas):
        ctx = _make_ctx()
        screen = MenuWheelScreen(ctx)
        for d in deltas:
            screen.on_input(MockEvent(delta=d))
            assert 0 <= ctx.state.menu_idx < len(MENU_ITEMS)


class TestProperty7StateIsolation:
    @given(delta=st.integers(-10, 10))
    @settings(max_examples=30)
    def test_rotation_only_changes_menu_idx(self, delta):
        ctx = _make_ctx()
        s = ctx.state
        before = dict(fan_speed=s.fan_speed, smoke_speed=s.smoke_speed,
                      pump_speed=s.pump_speed, brightness=s.brightness,
                      preset_idx=s.preset_idx, rgb_mode=s.rgb_mode)
        MenuWheelScreen(ctx).on_input(MockEvent(delta=delta))
        for k, v in before.items():
            assert getattr(s, k) == v


class TestProperty8ClickMapping:
    @given(idx=st.integers(0, len(MENU_ITEMS) - 1))
    @settings(max_examples=20)
    def test_click_switches_to_correct_target(self, idx):
        ctx = _make_ctx()
        ctx.state.menu_idx = idx
        MenuWheelScreen(ctx).on_input(MockEvent(key=EVT_CLICK))
        assert ctx.screen_manager.switched_to == MENU_ITEMS[idx][1]


class TestDrawLogic:
    def test_draw_full_clears_and_draws(self):
        ctx = _make_ctx()
        screen = MenuWheelScreen(ctx)
        r = ctx.renderer
        screen.draw_full(r)
        assert any(c[0] == 'clear' for c in r.calls)
        assert any(c[0] in ('draw_text', 'draw_gradient_text') for c in r.calls)

    def test_draw_update_skips_when_unchanged(self):
        ctx = _make_ctx()
        screen = MenuWheelScreen(ctx)
        r = ctx.renderer
        screen.draw_full(r)
        r.calls.clear()
        screen.draw_update(r)
        assert len(r.calls) == 0

    def test_draw_update_redraws_on_change(self):
        ctx = _make_ctx()
        screen = MenuWheelScreen(ctx)
        r = ctx.renderer
        screen.draw_full(r)
        r.calls.clear()
        ctx.state.menu_idx = 1
        screen.draw_update(r)
        assert any(c[0] == 'fill_rect' for c in r.calls)
        assert any(c[0] in ('draw_text', 'draw_gradient_text') for c in r.calls)

    def test_draw_item_fallback_without_icons(self):
        ctx = _make_ctx()
        screen = MenuWheelScreen(ctx)
        r = ctx.renderer
        screen.draw_full(r)
        assert any(c[0] in ('draw_text', 'draw_gradient_text') for c in r.calls)

    def test_on_enter_on_exit_lifecycle(self):
        ctx = _make_ctx()
        screen = MenuWheelScreen(ctx)
        assert screen._icons is None
        screen.on_enter()
        screen.on_exit()
        assert screen._icons is None
