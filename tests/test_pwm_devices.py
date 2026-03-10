"""Property-based tests for hal/pwm_devices.py

Validates: Requirements 4.4, 4.6
- Property 3: ∀ speed ∈ [0,100]: set_fan(speed) 后 PWM duty = speed × 65535 / 100
- Property 3 extended: ∀ rgb ∈ [0,255]³: set_cob(r,g,b) 后各通道占空比正确
- Clamp logic: values outside range should be clamped
- stop_all(): all duties set to 0
"""

import sys
import types
from unittest.mock import MagicMock

# ---------------------------------------------------------------------------
# Mock the `machine` module before importing hal.pwm_devices
# ---------------------------------------------------------------------------

_mock_machine = types.ModuleType("machine")


class FakePWM:
    """Records duty_u16 calls so tests can inspect the last value."""

    def __init__(self, pin):
        self.pin = pin
        self._duty = 0

    def freq(self, f):
        pass

    def duty_u16(self, val=None):
        if val is None:
            return self._duty
        self._duty = val


class FakePin:
    def __init__(self, num, *a, **kw):
        self.num = num


_mock_machine.Pin = FakePin
_mock_machine.PWM = FakePWM
sys.modules["machine"] = _mock_machine

# Now safe to import the module under test
from hal.pwm_devices import PwmDevices  # noqa: E402
from app_state import clamp  # noqa: E402

import pytest  # noqa: E402
from hypothesis import given, settings  # noqa: E402
from hypothesis import strategies as st  # noqa: E402


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _fresh_devices() -> PwmDevices:
    """Create a fresh PwmDevices instance (uses FakePWM under the hood)."""
    return PwmDevices()


# ---------------------------------------------------------------------------
# Property 3: ∀ speed ∈ [0,100]: set_fan(speed) → duty == speed * 65535 // 100
# **Validates: Requirements 4.4, 4.6**
# ---------------------------------------------------------------------------

@given(pct=st.integers(min_value=0, max_value=100))
@settings(max_examples=200)
def test_set_fan_duty_matches_formula(pct: int):
    """**Validates: Requirements 4.4, 4.6**

    For every percentage in [0,100], the PWM duty must equal pct * 65535 // 100.
    """
    dev = _fresh_devices()
    dev.set_fan(pct)
    expected = pct * 65535 // 100
    assert dev._fan._duty == expected, f"fan duty {dev._fan._duty} != expected {expected} for pct={pct}"


@given(pct=st.integers(min_value=0, max_value=100))
@settings(max_examples=200)
def test_set_small_fan_duty(pct: int):
    """**Validates: Requirements 4.4, 4.6**"""
    dev = _fresh_devices()
    dev.set_small_fan(pct)
    assert dev._small_fan._duty == pct * 65535 // 100


@given(pct=st.integers(min_value=0, max_value=100))
@settings(max_examples=200)
def test_set_pump_duty(pct: int):
    """**Validates: Requirements 4.4, 4.6**"""
    dev = _fresh_devices()
    dev.set_pump(pct)
    assert dev._pump._duty == pct * 65535 // 100


@given(pct=st.integers(min_value=0, max_value=100))
@settings(max_examples=200)
def test_set_smoke_duty(pct: int):
    """**Validates: Requirements 4.4, 4.6**"""
    dev = _fresh_devices()
    dev.set_smoke(pct)
    assert dev._smoke._duty == pct * 65535 // 100


# ---------------------------------------------------------------------------
# Property 3 extended: ∀ rgb ∈ [0,255]³: set_cob(r,g,b) → each channel correct
# **Validates: Requirements 4.4, 4.6**
# ---------------------------------------------------------------------------

@given(
    r=st.integers(min_value=0, max_value=255),
    g=st.integers(min_value=0, max_value=255),
    b=st.integers(min_value=0, max_value=255),
)
@settings(max_examples=200)
def test_set_cob1_rgb_duty(r: int, g: int, b: int):
    """**Validates: Requirements 4.4, 4.6**

    For every (r,g,b) in [0,255]³, each COB1 channel duty == val * 65535 // 255.
    """
    dev = _fresh_devices()
    dev.set_cob1(r, g, b)
    assert dev._cob1_r._duty == r * 65535 // 255
    assert dev._cob1_g._duty == g * 65535 // 255
    assert dev._cob1_b._duty == b * 65535 // 255


@given(
    r=st.integers(min_value=0, max_value=255),
    g=st.integers(min_value=0, max_value=255),
    b=st.integers(min_value=0, max_value=255),
)
@settings(max_examples=200)
def test_set_cob2_rgb_duty(r: int, g: int, b: int):
    """**Validates: Requirements 4.4, 4.6**"""
    dev = _fresh_devices()
    dev.set_cob2(r, g, b)
    assert dev._cob2_r._duty == r * 65535 // 255
    assert dev._cob2_g._duty == g * 65535 // 255
    assert dev._cob2_b._duty == b * 65535 // 255


# ---------------------------------------------------------------------------
# Clamp tests: out-of-range values should be clamped
# **Validates: Requirements 4.4**
# ---------------------------------------------------------------------------

@given(pct=st.integers(min_value=-1000, max_value=-1))
@settings(max_examples=50)
def test_set_fan_clamps_negative(pct: int):
    """**Validates: Requirements 4.4**

    Negative percentages should be clamped to 0 → duty == 0.
    """
    dev = _fresh_devices()
    dev.set_fan(pct)
    assert dev._fan._duty == 0


@given(pct=st.integers(min_value=101, max_value=1000))
@settings(max_examples=50)
def test_set_fan_clamps_above_100(pct: int):
    """**Validates: Requirements 4.4**

    Percentages > 100 should be clamped to 100 → duty == 65535.
    """
    dev = _fresh_devices()
    dev.set_fan(pct)
    assert dev._fan._duty == 100 * 65535 // 100


@given(
    r=st.integers(min_value=-500, max_value=500),
    g=st.integers(min_value=-500, max_value=500),
    b=st.integers(min_value=-500, max_value=500),
)
@settings(max_examples=100)
def test_set_cob1_clamps_out_of_range(r: int, g: int, b: int):
    """**Validates: Requirements 4.4**

    RGB values outside [0,255] should be clamped before computing duty.
    """
    dev = _fresh_devices()
    dev.set_cob1(r, g, b)
    cr, cg, cb = clamp(r, 0, 255), clamp(g, 0, 255), clamp(b, 0, 255)
    assert dev._cob1_r._duty == cr * 65535 // 255
    assert dev._cob1_g._duty == cg * 65535 // 255
    assert dev._cob1_b._duty == cb * 65535 // 255


# ---------------------------------------------------------------------------
# stop_all(): all duties set to 0
# **Validates: Requirements 4.5**
# ---------------------------------------------------------------------------

@given(
    pct=st.integers(min_value=0, max_value=100),
    r=st.integers(min_value=0, max_value=255),
    g=st.integers(min_value=0, max_value=255),
    b=st.integers(min_value=0, max_value=255),
)
@settings(max_examples=100)
def test_stop_all_zeroes_everything(pct: int, r: int, g: int, b: int):
    """**Validates: Requirements 4.5**

    After setting arbitrary values, stop_all() must reset every channel to 0.
    """
    dev = _fresh_devices()
    dev.set_fan(pct)
    dev.set_small_fan(pct)
    dev.set_pump(pct)
    dev.set_smoke(pct)
    dev.set_cob1(r, g, b)
    dev.set_cob2(r, g, b)

    dev.stop_all()

    for attr in (
        '_fan', '_small_fan', '_pump', '_smoke',
        '_cob1_r', '_cob1_g', '_cob1_b',
        '_cob2_r', '_cob2_g', '_cob2_b',
    ):
        assert getattr(dev, attr)._duty == 0, f"{attr} duty not 0 after stop_all()"
