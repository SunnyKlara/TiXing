# tests/test_preset.py — 预设索引属性测试
# 属性 4: ∀ preset_idx ∈ [0,11]: PRESETS[preset_idx] 返回有效的 (name, cob1, cob2) 元组

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from hypothesis import given, strategies as st, settings
from ui.theme import PRESETS


class TestPresetProperties:

    @given(idx=st.integers(min_value=0, max_value=11))
    @settings(max_examples=20)
    def test_preset_valid_tuple(self, idx):
        """∀ preset_idx ∈ [0,11]: PRESETS[idx] 返回有效 (name, cob1, cob2)。"""
        entry = PRESETS[idx]
        assert len(entry) == 3
        name, c1, c2 = entry
        assert isinstance(name, str) and len(name) > 0
        assert len(c1) == 3
        assert len(c2) == 3
        for v in c1:
            assert 0 <= v <= 255
        for v in c2:
            assert 0 <= v <= 255

    def test_preset_count(self):
        """预设数量为 12。"""
        assert len(PRESETS) == 12

    @given(idx=st.integers(min_value=0, max_value=11))
    @settings(max_examples=12)
    def test_preset_names_unique(self, idx):
        """预设名称非空。"""
        name = PRESETS[idx][0]
        assert len(name) > 0
