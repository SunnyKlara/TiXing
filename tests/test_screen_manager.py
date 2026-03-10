# tests/test_screen_manager.py — ScreenManager 属性测试
# 属性 1: ∀ ui_id ∈ [0,7]: switch(ui_id) 后 current_ui == ui_id ∧ init_flag == True
# 属性 2: ∀ frame: init_flag == True → draw_full() 被调用 → init_flag 重置为 False

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from hypothesis import given, strategies as st, settings


class MockRenderer:
    def __init__(self):
        self.cleared = False
    def clear(self, color=0):
        self.cleared = True
    def fill_rect(self, *a):
        pass
    def blit_bitmap(self, *a):
        pass
    def draw_text(self, *a):
        return 0
    def draw_number(self, *a):
        pass
    def draw_hbar(self, *a):
        pass
    def draw_dot(self, *a):
        pass


class MockScreen:
    def __init__(self):
        self.entered = False
        self.exited = False
        self.full_drawn = False
        self.update_drawn = False

    def on_enter(self):
        self.entered = True
    def on_exit(self):
        self.exited = True
    def on_input(self, event):
        pass
    def draw_full(self, r):
        self.full_drawn = True
    def draw_update(self, r):
        self.update_drawn = True


def _make_sm():
    from screens import Context
    from screen_manager import ScreenManager
    r = MockRenderer()
    ctx = Context(r, None, None, None, None, None)
    sm = ScreenManager(ctx)
    screens = {}
    for i in range(8):
        s = MockScreen()
        sm.register(i, s)
        screens[i] = s
    return sm, screens, r


class TestScreenManagerProperties:

    @given(ui_id=st.integers(min_value=0, max_value=7))
    @settings(max_examples=20)
    def test_switch_sets_current_and_init_flag(self, ui_id):
        """∀ ui_id ∈ [0,7]: switch 后 current_ui == ui_id ∧ init_flag == True。"""
        sm, screens, _ = _make_sm()
        sm.switch(ui_id)
        assert sm.current_ui == ui_id
        assert sm._init_flag is True

    @given(ui_id=st.integers(min_value=0, max_value=7))
    @settings(max_examples=20)
    def test_render_calls_draw_full_on_init_flag(self, ui_id):
        """init_flag=True → draw_full() 被调用 → init_flag 重置为 False。"""
        sm, screens, _ = _make_sm()
        sm.switch(ui_id)
        assert sm._init_flag is True
        sm.render()
        assert screens[ui_id].full_drawn is True
        assert sm._init_flag is False

    @given(ui_id=st.integers(min_value=0, max_value=7))
    @settings(max_examples=20)
    def test_render_calls_draw_update_after_init(self, ui_id):
        """init_flag=False → draw_update() 被调用。"""
        sm, screens, _ = _make_sm()
        sm.switch(ui_id)
        sm.render()  # draw_full
        screens[ui_id].full_drawn = False
        sm.render()  # draw_update
        assert screens[ui_id].update_drawn is True
        assert screens[ui_id].full_drawn is False

    @given(old=st.integers(min_value=0, max_value=7),
           new=st.integers(min_value=0, max_value=7))
    @settings(max_examples=30)
    def test_switch_calls_on_exit_and_on_enter(self, old, new):
        """switch 时旧界面 on_exit + 新界面 on_enter。"""
        sm, screens, _ = _make_sm()
        sm.switch(old)
        sm.switch(new)
        assert screens[old].exited is True
        assert screens[new].entered is True
