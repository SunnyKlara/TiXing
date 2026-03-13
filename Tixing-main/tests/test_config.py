# tests/test_config.py — Config 属性测试
# 属性 5: ∀ config: load(save(config)) == config（配置往返一致性）

import sys
import os
import tempfile
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from hypothesis import given, strategies as st, settings
from services.config import Config, DEFAULT_CONFIG
from app_state import AppState


def _rgb_strategy():
    return st.lists(st.integers(min_value=0, max_value=255), min_size=3, max_size=3)


class TestConfigProperties:

    def _make_config(self):
        tmp = tempfile.NamedTemporaryFile(suffix='.json', delete=False)
        tmp.close()
        return Config(tmp.name), tmp.name

    @given(
        fan=st.integers(min_value=0, max_value=100),
        smoke=st.integers(min_value=0, max_value=100),
        pump=st.integers(min_value=0, max_value=100),
        brightness=st.integers(min_value=0, max_value=100),
        preset=st.integers(min_value=0, max_value=11),
        c1=_rgb_strategy(),
        c2=_rgb_strategy(),
        breath=st.booleans(),
        gradient=st.booleans(),
    )
    @settings(max_examples=50)
    def test_roundtrip_consistency(self, fan, smoke, pump, brightness,
                                   preset, c1, c2, breath, gradient):
        """∀ config: load(save(config)) == config。"""
        cfg, path = self._make_config()
        try:
            state = AppState()
            state.fan_speed = fan
            state.smoke_speed = smoke
            state.pump_speed = pump
            state.brightness = brightness
            state.preset_idx = preset
            state.cob1_rgb = list(c1)
            state.cob2_rgb = list(c2)
            state.breath_mode = breath
            state.gradient_mode = gradient

            cfg.save_from_state(state)

            cfg2 = Config(path)
            state2 = AppState()
            cfg2.load_to_state(state2)

            assert state2.fan_speed == fan
            assert state2.smoke_speed == smoke
            assert state2.pump_speed == pump
            assert state2.brightness == brightness
            assert state2.preset_idx == preset
            assert state2.cob1_rgb == list(c1)
            assert state2.cob2_rgb == list(c2)
            assert state2.breath_mode == breath
            assert state2.gradient_mode == gradient
        finally:
            os.unlink(path)

    @given(
        fan=st.integers(min_value=-100, max_value=200),
        brightness=st.integers(min_value=-50, max_value=300),
        preset=st.integers(min_value=-10, max_value=20),
    )
    @settings(max_examples=30)
    def test_load_validates_ranges(self, fan, brightness, preset):
        """load 后所有值在有效范围内。"""
        cfg, path = self._make_config()
        try:
            import json
            data = dict(DEFAULT_CONFIG)
            data["fan_speed"] = fan
            data["brightness"] = brightness
            data["preset_idx"] = preset
            with open(path, 'w') as f:
                json.dump(data, f)

            cfg2 = Config(path)
            state = AppState()
            cfg2.load_to_state(state)

            assert 0 <= state.fan_speed <= 100
            assert 0 <= state.brightness <= 100
            assert 0 <= state.preset_idx <= 11
        finally:
            os.unlink(path)

    def test_corrupted_file_uses_defaults(self):
        """损坏文件使用默认值。"""
        cfg, path = self._make_config()
        try:
            with open(path, 'w') as f:
                f.write('not json{{{')
            cfg2 = Config(path)
            state = AppState()
            cfg2.load_to_state(state)
            assert state.fan_speed == 0
            assert state.brightness == 100
        finally:
            os.unlink(path)

    def test_missing_file_uses_defaults(self):
        """缺失文件使用默认值。"""
        cfg = Config('/tmp/nonexistent_pico_turbo_test.json')
        state = AppState()
        cfg.load_to_state(state)
        assert state.fan_speed == 0
        assert state.brightness == 100
