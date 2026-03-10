from machine import Pin, PWM
from app_state import clamp


class PwmDevices:
    """Unified PWM peripheral control for all output devices."""
    __slots__ = (
        '_fan', '_small_fan', '_pump', '_smoke',
        '_cob1_r', '_cob1_g', '_cob1_b',
        '_cob2_r', '_cob2_g', '_cob2_b',
    )

    def __init__(self):
        freq = 1000
        self._fan = PWM(Pin(6)); self._fan.freq(freq); self._fan.duty_u16(0)
        self._small_fan = PWM(Pin(10)); self._small_fan.freq(freq); self._small_fan.duty_u16(0)
        self._pump = PWM(Pin(11)); self._pump.freq(freq); self._pump.duty_u16(0)
        self._smoke = PWM(Pin(12)); self._smoke.freq(freq); self._smoke.duty_u16(0)
        self._cob1_r = PWM(Pin(13)); self._cob1_r.freq(freq); self._cob1_r.duty_u16(0)
        self._cob1_g = PWM(Pin(14)); self._cob1_g.freq(freq); self._cob1_g.duty_u16(0)
        self._cob1_b = PWM(Pin(15)); self._cob1_b.freq(freq); self._cob1_b.duty_u16(0)
        self._cob2_r = PWM(Pin(7)); self._cob2_r.freq(freq); self._cob2_r.duty_u16(0)
        self._cob2_g = PWM(Pin(8)); self._cob2_g.freq(freq); self._cob2_g.duty_u16(0)
        self._cob2_b = PWM(Pin(9)); self._cob2_b.freq(freq); self._cob2_b.duty_u16(0)

    def set_fan(self, pct):
        """Main fan GPIO6, 0-100%"""
        self._fan.duty_u16(clamp(pct, 0, 100) * 65535 // 100)

    def set_small_fan(self, pct):
        """Small fan GPIO10, 0-100%"""
        self._small_fan.duty_u16(clamp(pct, 0, 100) * 65535 // 100)

    def set_pump(self, pct):
        """Pump GPIO11, 0-100%"""
        self._pump.duty_u16(clamp(pct, 0, 100) * 65535 // 100)

    def set_smoke(self, pct):
        """Smoke generator GPIO12, 0-100%"""
        self._smoke.duty_u16(clamp(pct, 0, 100) * 65535 // 100)

    def set_cob1(self, r, g, b):
        """COB1 LED strip GPIO13=R, GPIO14=G, GPIO15=B, each 0-255"""
        self._cob1_r.duty_u16(clamp(r, 0, 255) * 65535 // 255)
        self._cob1_g.duty_u16(clamp(g, 0, 255) * 65535 // 255)
        self._cob1_b.duty_u16(clamp(b, 0, 255) * 65535 // 255)

    def set_cob2(self, r, g, b):
        """COB2 LED strip GPIO7=R, GPIO8=G, GPIO9=B, each 0-255"""
        self._cob2_r.duty_u16(clamp(r, 0, 255) * 65535 // 255)
        self._cob2_g.duty_u16(clamp(g, 0, 255) * 65535 // 255)
        self._cob2_b.duty_u16(clamp(b, 0, 255) * 65535 // 255)

    def stop_all(self):
        """Set all PWM duty to 0"""
        self._fan.duty_u16(0)
        self._small_fan.duty_u16(0)
        self._pump.duty_u16(0)
        self._smoke.duty_u16(0)
        self._cob1_r.duty_u16(0)
        self._cob1_g.duty_u16(0)
        self._cob1_b.duty_u16(0)
        self._cob2_r.duty_u16(0)
        self._cob2_g.duty_u16(0)
        self._cob2_b.duty_u16(0)
