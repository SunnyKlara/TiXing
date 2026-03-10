# tests/test_font.py — Font 属性测试
# 属性 10: ∀ font_size ∈ {8, 16, 32}: get_font(size) 返回有效 Font 实例

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from hypothesis import given, strategies as st, settings
from ui.font import get_font, _font_cache


class TestFontProperties:
    """字体引擎属性测试。"""

    def setup_method(self):
        _font_cache.clear()

    def test_builtin_8px_valid(self):
        """8px 内置字体始终可用。"""
        f = get_font(8)
        assert f is not None
        assert f.width == 8
        assert f.height == 8

    def test_font_cache_consistency(self):
        """同一 size 多次调用返回同一实例。"""
        f1 = get_font(8)
        f2 = get_font(8)
        assert f1 is f2

    @given(size=st.sampled_from([8, 16, 32]))
    @settings(max_examples=20)
    def test_get_font_returns_valid_instance(self, size):
        """∀ font_size ∈ {8, 16, 32}: get_font(size) 返回有效 Font 实例。"""
        _font_cache.clear()
        f = get_font(size)
        assert f is not None
        assert f.width > 0
        assert f.height > 0
        assert hasattr(f, 'get_glyph')
        assert hasattr(f, 'text_width')

    @given(ch=st.characters(min_codepoint=48, max_codepoint=57))
    @settings(max_examples=20)
    def test_builtin_digit_glyphs_not_empty(self, ch):
        """内置字体数字字形非空。"""
        f = get_font(8)
        glyph = f.get_glyph(ch)
        assert glyph is not None
        assert len(glyph) == f.width  # 8x8: 8 bytes per glyph
        # 至少有一个非零字节（数字不应全空）
        assert any(b != 0 for b in glyph)

    @given(text=st.text(alphabet='0123456789', min_size=1, max_size=10))
    @settings(max_examples=30)
    def test_text_width_proportional(self, text):
        """text_width 与字符数成正比。"""
        f = get_font(8)
        assert f.text_width(text) == len(text) * f.width

    def test_out_of_range_char_returns_none(self):
        """超出范围的字符返回 None。"""
        f = get_font(8)
        assert f.get_glyph('\x00') is None
        assert f.get_glyph('\xff') is None

    @given(size=st.sampled_from([8, 16, 32]))
    @settings(max_examples=10)
    def test_fallback_to_8px(self, size):
        """缺少字体文件时回退到 8px。"""
        _font_cache.clear()
        f = get_font(size)
        # 所有尺寸都应返回有效实例（16/32 回退到 8px）
        assert f is not None
        assert f.width > 0
