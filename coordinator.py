# coordinator.py - 设备联动协调器
# 管理多设备联动逻辑：风扇联动、发烟-气泵联动、延迟关闭
# Requirements: 9.1, 9.2, 9.3, 9.4


class DeviceCoordinator:
    def __init__(self, config):
        self.link_mode = config.get("link_mode", "auto")  # "auto" | "manual"
        self.fan_link_ratio = config.get("fan_link_ratio", 0.8)
        self.pump_delay_ms = 2000
        self._pump_off_tick = 0
        self._pump_pending_off = False

    def on_fan_speed_change(self, fan_speed, set_small_fan_fn):
        """主风扇变化时按联动比例同步小风扇 (Req 9.1)"""
        if self.link_mode == "auto":
            small_speed = int(fan_speed * self.fan_link_ratio)
            set_small_fan_fn(small_speed)

    def on_smoke_change(self, smoke_speed, set_air_pump_fn, current_tick):
        """发烟器变化时联动气泵 (Req 9.2, 9.3)
        
        smoke_speed > 0: 立即启动气泵 (speed + 20, 上限100)
        smoke_speed == 0: 标记延迟关闭，由 process() 执行
        """
        if self.link_mode != "auto":
            return
        if smoke_speed > 0:
            set_air_pump_fn(min(100, smoke_speed + 20))
            self._pump_pending_off = False
        else:
            self._pump_pending_off = True
            self._pump_off_tick = current_tick

    def process(self, current_tick, set_air_pump_fn):
        """每帧调用，处理非阻塞延迟任务 (Req 9.3, 9.5)
        
        使用简单减法比较 tick，兼容 CPython 和 MicroPython。
        """
        if self._pump_pending_off:
            if current_tick - self._pump_off_tick >= self.pump_delay_ms:
                set_air_pump_fn(0)
                self._pump_pending_off = False
