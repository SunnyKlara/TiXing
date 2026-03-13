# tests/test_integration.py — ScreenManager 事件序列属性测试
# 属性 5(状态机): ∀ event sequence: screen_manager 状态始终在有效范围内
# 属性 8: ∀ 界面退出: on_exit() 在 switch() 中被调用
# 属性 9: ∀ 异常: stop_all_devices() 被调用

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from hypothesis import given, strategies as st, settings
from hal.input import InputEvent, EVT_NONE, EVT_CLICK, EVT_DOUBLE, EVT_TRIPLE, EVT_LONG


class MockRenderer:
    def clear(self, color=0): pass
    def fill_rect(self, *a): pass
    def blit_bitmap(self, *a): pass
    def draw_text(self, *a): return 0
    def draw_number(self, *a): pass
    def draw_hbar(self, *a): pass
    def draw_dot(self, *a): pass
    def draw_gradient_bar(self, *a): pass


class TrackingScreen:
    def __init__(self):
        self.exit_count = 0
        self.enter_count = 0
    def on_enter(self):
        self.enter_count += 1
    def on_exit(self):
        self.exit_count += 1
    def on_input(self, event):
        pass
    def draw_full(self, r):
        pass
    def draw_update(self, r):
        pass


def _make_sm():
    from screens import Context
    from screen_manager import ScreenManager
    r = MockRenderer()
    ctx = Context(r, None, None, None, None, None)
    sm = ScreenManager(ctx)
    screens = {}
    for i in range(8):
        s = TrackingScreen()
        sm.register(i, s)
        screens[i] = s
    return sm, screens


class TestEventSequenceProperties:

    @given(switches=st.lists(st.integers(min_value=0, max_value=7),
                             min_size=1, max_size=50))
    @settings(max_examples=50)
    def test_state_always_valid_after_switches(self, switches):
        """∀ switch sequence: current_ui 始终在 [0,7]。"""
        sm, _ = _make_sm()
        for ui_id in switches:
            sm.switch(ui_id)
            assert 0 <= sm.current_ui <= 7

    @given(switches=st.lists(st.integers(min_value=0, max_value=7),
                             min_size=2, max_size=20))
    @settings(max_examples=30)
    def test_on_exit_called_on_switch(self, switches):
        """∀ 界面退出: on_exit() 在 switch() 中被调用。"""
        sm, screens = _make_sm()
        sm.switch(switches[0])
        for ui_id in switches[1:]:
            prev = sm.current_ui
            prev_exit_before = screens[prev].exit_count
            sm.switch(ui_id)
            assert screens[prev].exit_count == prev_exit_before + 1

    @given(
        switches=st.lists(st.integers(min_value=0, max_value=7),
                          min_size=1, max_size=30),
        renders=st.lists(st.booleans(), min_size=1, max_size=30),
    )
    @settings(max_examples=30)
    def test_interleaved_switch_render(self, switches, renders):
        """交替 switch 和 render 不会崩溃，状态始终有效。"""
        sm, _ = _make_sm()
        for i in range(min(len(switches), len(renders))):
            sm.switch(switches[i])
            if renders[i]:
                sm.render()
            assert 0 <= sm.current_ui <= 7

    @given(events=st.lists(
        st.tuples(
            st.integers(min_value=-5, max_value=5),
            st.sampled_from([EVT_NONE, EVT_CLICK, EVT_DOUBLE, EVT_TRIPLE, EVT_LONG])
        ),
        min_size=1, max_size=50
    ))
    @settings(max_examples=30)
    def test_dispatch_never_crashes(self, events):
        """∀ event sequence: dispatch 不会崩溃。"""
        sm, _ = _make_sm()
        sm.switch(0)
        for delta, key in events:
            event = InputEvent()
            event.delta = delta
            event.key = key
            sm.dispatch(event)
            assert 0 <= sm.current_ui <= 7
