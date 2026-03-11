# tests/test_menu_wheel.py — 滑动式菜单滚轮 属性测试 + 单元测试
# Properties 1-8 + 绘制逻辑单元测试

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from hypothesis import given, strategies as st, settings
from screens.menu import (
    calc_visible_items, MenuWheelScreen, MENU_ITEMS,
    SCREEN_W, ITEM_SPACING, CENTER_X, MAX_VISIBLE, ACCENT,
    DOTS_Y,
)
from ui.theme import WHITE, GRAY, BLACK
from hal.input import EVT_CLICK, EVT_NONE
from app_state import AppState


# ── Mock 对象 ──

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


# ══════════════════════════════════════════════════════════════
# Property 1: 可见项数量上界
# ══════════════════════════════════════════════════════════════

class TestProperty1VisibleItemCount:
    @given(
        total=st.integers(min_value=1, max_value=20),
        data=st.data(),
    )
    @settings(max_examples=50)
    def test_count_le_max_visible(self, total, data):
        idx = data.draw(st.integers(min_value=0, max_value=total - 1))
        items = calc_visible_items(idx, total)
        assert len(items) <= MAX_VISIBLE


# ══════════════════════════════════════════════════════════════
# Property 2: 选中项居中且参数正确
# ══════════════════════════════════════════════════════════════

class TestProperty2SelectedCentered:
    @given(
        total=st.integers(min_value=1, max_value=20),
        data=st.data(),
    )
    @settings(max_examples=50)
    def test_exactly_one_accent_at_center(self, total, data):
        idx = data.draw(st.integers(min_value=0, max_value=total - 1))
        items = calc_visible_items(idx, total)
        accent_items = [i for i in items if i[3] == ACCENT]
        assert len(accent_items) == 1
        _, cx, icon_size, _, font_size = accent_items[0]
        assert cx == CENTER_X
        assert icon_size == 32
        assert font_size == 16


# ══════════════════════════════════════════════════════════════
# Property 3: 尺寸梯度
# ══════════════════════════════════════════════════════════════

class TestProperty3SizeGradient:
    @given(idx=st.integers(min_value=0, max_value=5))
    @settings(max_examples=20)
    def test_size_gradient_with_6_items(self, idx):
        items = calc_visible_items(idx, 6)
        for item_idx, cx, icon_size, fg_color, font_size in items:
            offset_px = cx - CENTER_X
            offset = offset_px // ITEM_SPACING if ITEM_SPACING != 0 else 0
            if offset == 0:
                assert icon_size == 32
                assert fg_color == ACCENT
                assert font_size == 16
            elif abs(offset) == 1:
                assert icon_size == 20
                assert fg_color == WHITE
                assert font_size == 16
            elif abs(offset) >= 2:
                assert icon_size == 16
                assert fg_color == GRAY
                assert font_size == 16


# ══════════════════════════════════════════════════════════════
# Property 4: 可见项坐标边界
# ══════════════════════════════════════════════════════════════

class TestProperty4CoordinateBounds:
    @given(
        total=st.integers(min_value=1, max_value=20),
        data=st.data(),
    )
    @settings(max_examples=50)
    def test_cx_within_bounds(self, total, data):
        idx = data.draw(st.integers(min_value=0, max_value=total - 1))
        items = calc_visible_items(idx, total)
        lo = -ITEM_SPACING // 2
        hi = SCREEN_W + ITEM_SPACING // 2
        for _, cx, _, _, _ in items:
            assert lo <= cx <= hi


# ══════════════════════════════════════════════════════════════
# Property 5: 可见项索引唯一性
# ══════════════════════════════════════════════════════════════

class TestProperty5UniqueIndices:
    @given(
        total=st.integers(min_value=1, max_value=20),
        data=st.data(),
    )
    @settings(max_examples=50)
    def test_no_duplicate_item_idx(self, total, data):
        idx = data.draw(st.integers(min_value=0, max_value=total - 1))
        items = calc_visible_items(idx, total)
        indices = [i[0] for i in items]
        assert len(indices) == len(set(indices))


# ══════════════════════════════════════════════════════════════
# Property 6: 菜单索引边界不变量
# ══════════════════════════════════════════════════════════════

class TestProperty6IndexBounds:
    @given(deltas=st.lists(st.integers(min_value=-10, max_value=10), min_size=1, max_size=50))
    @settings(max_examples=50)
    def test_menu_idx_always_valid(self, deltas):
        ctx = _make_ctx()
        screen = MenuWheelScreen(ctx)
        n = len(MENU_ITEMS)
        for d in deltas:
            evt = MockEvent(delta=d)
            screen.on_input(evt)
            assert 0 <= ctx.state.menu_idx < n


# ══════════════════════════════════════════════════════════════
# Property 7: 旋转事件状态隔离
# ══════════════════════════════════════════════════════════════

class TestProperty7StateIsolation:
    @given(delta=st.integers(min_value=-10, max_value=10))
    @settings(max_examples=30)
    def test_rotation_only_changes_menu_idx(self, delta):
        ctx = _make_ctx()
        state = ctx.state
        # 记录旋转前的所有非 menu_idx 属性
        before = {
            'fan_speed': state.fan_speed,
            'smoke_speed': state.smoke_speed,
            'pump_speed': state.pump_speed,
            'brightness': state.brightness,
            'preset_idx': state.preset_idx,
            'rgb_mode': state.rgb_mode,
        }
        screen = MenuWheelScreen(ctx)
        evt = MockEvent(delta=delta)
        screen.on_input(evt)
        # 验证其他属性未变
        assert state.fan_speed == before['fan_speed']
        assert state.smoke_speed == before['smoke_speed']
        assert state.pump_speed == before['pump_speed']
        assert state.brightness == before['brightness']
        assert state.preset_idx == before['preset_idx']
        assert state.rgb_mode == before['rgb_mode']


# ══════════════════════════════════════════════════════════════
# Property 8: 单击映射正确性
# ══════════════════════════════════════════════════════════════

class TestProperty8ClickMapping:
    @given(idx=st.integers(min_value=0, max_value=len(MENU_ITEMS) - 1))
    @settings(max_examples=20)
    def test_click_switches_to_correct_target(self, idx):
        ctx = _make_ctx()
        ctx.state.menu_idx = idx
        screen = MenuWheelScreen(ctx)
        evt = MockEvent(key=EVT_CLICK)
        screen.on_input(evt)
        expected_target = MENU_ITEMS[idx][1]
        assert ctx.screen_manager.switched_to == expected_target


# ══════════════════════════════════════════════════════════════
# 绘制逻辑单元测试
# ══════════════════════════════════════════════════════════════

class TestDrawLogic:
    def test_draw_full_clears_and_draws(self):
        ctx = _make_ctx()
        screen = MenuWheelScreen(ctx)
        r = ctx.renderer
        screen.draw_full(r)
        # 应该调用了 clear
        assert any(c[0] == 'clear' for c in r.calls)
        # 应该绘制了文字 (至少选中项标签)
        text_calls = [c for c in r.calls if c[0] == 'draw_text']
        assert len(text_calls) > 0

    def test_draw_update_skips_when_unchanged(self):
        ctx = _make_ctx()
        screen = MenuWheelScreen(ctx)
        r = ctx.renderer
        screen.draw_full(r)
        r.calls.clear()
        # menu_idx 未变, draw_update 应跳过
        screen.draw_update(r)
        assert len(r.calls) == 0

    def test_draw_update_redraws_on_change(self):
        ctx = _make_ctx()
        screen = MenuWheelScreen(ctx)
        r = ctx.renderer
        screen.draw_full(r)
        r.calls.clear()
        # 改变 menu_idx
        ctx.state.menu_idx = 1
        screen.draw_update(r)
        # 应该有 fill_rect 清除 + draw_text 绘制
        assert any(c[0] == 'fill_rect' for c in r.calls)
        assert any(c[0] == 'draw_text' for c in r.calls)

    def test_draw_item_fallback_without_icons(self):
        """无图标时应使用首字母 fallback。"""
        ctx = _make_ctx()
        screen = MenuWheelScreen(ctx)
        # 不调用 on_enter, _icons 为 None
        r = ctx.renderer
        screen.draw_full(r)
        text_calls = [c for c in r.calls if c[0] == 'draw_text']
        # 应该有文字绘制 (首字母 + 标签)
        assert len(text_calls) > 0

    def test_on_enter_on_exit_lifecycle(self):
        """on_enter 加载图标 (可能失败), on_exit 释放。"""
        ctx = _make_ctx()
        screen = MenuWheelScreen(ctx)
        assert screen._icons is None
        screen.on_enter()
        # 图标可能加载成功也可能失败 (取决于文件是否存在)
        screen.on_exit()
        assert screen._icons is None
