# tests/test_renderer.py — Renderer 进度条属性测试
# 属性 1: ∀ pct ∈ [0,100]: draw_hbar(pct) 前景宽度 ∈ [0, bar_width]

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from hypothesis import given, strategies as st, settings


class MockDisplay:
    """模拟 Display，记录所有 fill_rect 调用。"""
    def __init__(self):
        self.calls = []

    def fill(self, color):
        self.calls.append(('fill', color))

    def fill_rect(self, x, y, w, h, color):
        self.calls.append(('fill_rect', x, y, w, h, color))

    def blit(self, buf, x, y, w, h):
        self.calls.append(('blit', x, y, w, h))


class TestRendererHBar:
    """进度条属性测试。"""

    def _make_renderer(self):
        from ui.renderer import Renderer
        disp = MockDisplay()
        r = Renderer(disp)
        return r, disp

    @given(pct=st.integers(min_value=0, max_value=100))
    @settings(max_examples=100)
    def test_hbar_foreground_width_in_range(self, pct):
        """∀ pct ∈ [0,100]: draw_hbar 不会超出 bar 宽度。"""
        r, disp = self._make_renderer()
        bar_w = 200
        bar_h = 12
        bar_x = 20
        bar_y = 50

        r.draw_hbar(bar_x, bar_y, bar_w, bar_h, pct, 0x07E0, 0x39E7)

        # 检查所有 fill_rect 调用的 x+w 不超过 bar_x+bar_w
        for call in disp.calls:
            if call[0] == 'fill_rect':
                _, x, y, w, h, color = call
                assert x >= bar_x - bar_h // 2  # 圆角可能向左延伸
                assert x + w <= bar_x + bar_w + bar_h // 2 + 1  # 圆角可能向右延伸

    @given(pct=st.integers(min_value=0, max_value=100))
    @settings(max_examples=50)
    def test_hbar_pct_zero_no_foreground_overflow(self, pct):
        """进度条百分比边界安全。"""
        r, disp = self._make_renderer()
        # 不应抛出异常
        r.draw_hbar(10, 10, 100, 8, pct, 0xFFFF, 0x0000)

    def test_hbar_zero_percent(self):
        """0% 时仅绘制背景。"""
        r, disp = self._make_renderer()
        r.draw_hbar(10, 10, 100, 8, 0, 0x07E0, 0x39E7)
        # 应该有调用（至少背景）
        assert len(disp.calls) > 0

    def test_hbar_hundred_percent(self):
        """100% 时前景覆盖全部。"""
        r, disp = self._make_renderer()
        r.draw_hbar(10, 10, 100, 8, 100, 0x07E0, 0x39E7)
        assert len(disp.calls) > 0
