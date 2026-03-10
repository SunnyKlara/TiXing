# services/coordinator.py — 设备联动
# 风扇联动 + 发烟器/气泵延迟关闭

try:
    from utime import ticks_ms, ticks_diff
except ImportError:
    from time import time as _time

    def ticks_ms():
        return int(_time() * 1000)

    def ticks_diff(a, b):
        return a - b

from app_state import clamp


class Coordinator:
    """设备联动管理。

    - 风扇联动：主风扇变化时按比例同步小风扇
    - 发烟器联动：启动时自动启动气泵，关闭时延迟 2s 关闭气泵
    """

    PUMP_DELAY_MS = 2000
    FAN_LINK_RATIO = 0.8

    def __init__(self, devices):
        self._devices = devices
        self._pump_off_time = 0   # 气泵延迟关闭时间戳
        self._pump_pending = False
        self._last_smoke = 0

    def sync_fan(self, state):
        """同步风扇输出（含联动）。"""
        d = self._devices
        d.set_fan(state.fan_speed)
        small = int(state.fan_speed * self.FAN_LINK_RATIO)
        d.set_small_fan(clamp(small, 0, 100))

    def sync_smoke(self, state):
        """同步发烟器输出（含气泵联动）。"""
        d = self._devices
        d.set_smoke(state.smoke_speed)

        if state.smoke_speed > 0 and self._last_smoke == 0:
            # 发烟器启动 → 自动启动气泵
            if state.pump_speed == 0:
                d.set_pump(50)  # 默认气泵速度
            self._pump_pending = False
        elif state.smoke_speed == 0 and self._last_smoke > 0:
            # 发烟器关闭 → 延迟关闭气泵
            self._pump_off_time = ticks_ms()
            self._pump_pending = True

        self._last_smoke = state.smoke_speed

    def sync_pump(self, state):
        """同步气泵输出。"""
        self._devices.set_pump(state.pump_speed)
        if state.pump_speed > 0:
            self._pump_pending = False

    def process(self):
        """每帧处理延迟任务（非阻塞）。"""
        if self._pump_pending:
            if ticks_diff(ticks_ms(), self._pump_off_time) >= self.PUMP_DELAY_MS:
                self._devices.set_pump(0)
                self._pump_pending = False
