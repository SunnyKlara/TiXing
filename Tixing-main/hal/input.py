# hal/input.py — EC10 编码器输入管理
# GPIO19=按键（低电平有效）, GPIO20=A相, GPIO21=B相
# 旋转检测: GPIO 中断 + 2ms 消抖
# 按键状态机: 20ms 消抖, 300ms 多击窗口, 1000ms 长按

try:
    from machine import Pin, Timer
    from time import ticks_ms, ticks_diff
except ImportError:
    # PC 端测试用 stub
    Pin = None
    Timer = None
    from time import time as _time
    def ticks_ms():
        return int(_time() * 1000)
    def ticks_diff(a, b):
        return a - b

# 事件类型常量
EVT_NONE = 0
EVT_CLICK = 1
EVT_DOUBLE = 2
EVT_TRIPLE = 3
EVT_LONG = 4


class InputEvent:
    """输入事件数据，每帧 poll() 返回一个实例。"""
    __slots__ = ('delta', 'key')

    def __init__(self):
        self.delta = 0
        self.key = EVT_NONE


class Input:
    """EC10 编码器输入管理。

    旋转: A 相 GPIO 中断（双边沿），读 B 相判方向，2ms 消抖，累积 delta。
    按键: 中断触发 → 20ms 消抖定时器 → 状态机判定单/双/三击/长按。
    """

    def __init__(self, key_pin=19, a_pin=20, b_pin=21):
        # --- GPIO 初始化 ---
        self._key = Pin(key_pin, Pin.IN, Pin.PULL_UP)
        self._a = Pin(a_pin, Pin.IN, Pin.PULL_UP)
        self._b = Pin(b_pin, Pin.IN, Pin.PULL_UP)

        # --- 编码器状态 ---
        self._delta = 0
        self._last_a = self._a.value()
        self._last_enc_time = 0

        # --- 按键状态机 ---
        self._key_event = EVT_NONE      # 待读取的按键事件
        self._press_time = 0            # 按下时刻 (0 = 未按下)
        self._click_count = 0           # 当前累积点击次数
        self._long_flag = False         # 长按已触发标志

        # --- 持久化定时器（复用，避免内存碎片）---
        self._debounce_timer = Timer(-1)
        self._long_timer = Timer(-1)
        self._multi_timer = Timer(-1)

        # --- 注册中断 ---
        self._a.irq(
            trigger=Pin.IRQ_RISING | Pin.IRQ_FALLING,
            handler=self._encoder_irq,
        )
        self._key.irq(
            trigger=Pin.IRQ_RISING | Pin.IRQ_FALLING,
            handler=self._key_irq,
        )

    # ------------------------------------------------------------------
    # 编码器旋转中断
    # ------------------------------------------------------------------
    def _encoder_irq(self, pin):
        """A 相电平变化中断，读 B 相判方向，2ms 消抖。"""
        now = ticks_ms()
        if ticks_diff(now, self._last_enc_time) < 2:
            return  # 消抖：间隔 < 2ms 忽略

        a_val = self._a.value()
        if a_val == self._last_a:
            return  # 电平未变，忽略
        self._last_a = a_val
        self._last_enc_time = now

        b_val = self._b.value()
        if a_val == 1:  # A 相上升沿
            if b_val == 0:
                self._delta += 1   # 正转
            else:
                self._delta -= 1   # 反转
        else:            # A 相下降沿
            if b_val == 1:
                self._delta += 1   # 正转
            else:
                self._delta -= 1   # 反转

    # ------------------------------------------------------------------
    # 按键状态机
    # ------------------------------------------------------------------
    def _key_irq(self, pin):
        """按键电平变化中断，启动 20ms 消抖定时器。"""
        self._debounce_timer.init(
            period=20,
            mode=Timer.ONE_SHOT,
            callback=self._key_debounce,
        )

    def _key_debounce(self, timer):
        """消抖后读取按键真实状态并驱动状态机。"""
        val = self._key.value()

        # --- 按下（低电平）且之前未记录按下 ---
        if val == 0 and self._press_time == 0:
            self._press_time = ticks_ms()
            self._long_flag = False
            # 启动长按检测定时器 (1000ms)
            self._long_timer.init(
                period=1000,
                mode=Timer.ONE_SHOT,
                callback=self._long_press_cb,
            )

        # --- 释放（高电平）且之前已记录按下 ---
        elif val == 1 and self._press_time != 0:
            held = ticks_diff(ticks_ms(), self._press_time)
            self._press_time = 0

            if self._long_flag:
                # 长按已在定时器回调中处理，释放时不再产生事件
                return

            # 非长按释放 → 累积点击
            self._click_count += 1
            # 每次点击重置多击窗口定时器 (300ms)
            self._multi_timer.init(
                period=300,
                mode=Timer.ONE_SHOT,
                callback=self._multi_click_cb,
            )

    def _long_press_cb(self, timer):
        """长按 1000ms 定时器回调。"""
        if self._key.value() == 0:  # 仍然按着
            self._long_flag = True
            self._key_event = EVT_LONG
            self._click_count = 0
            # 取消多击定时器（长按与多击互斥）
            try:
                self._multi_timer.deinit()
            except Exception:
                pass

    def _multi_click_cb(self, timer):
        """300ms 多击窗口超时，根据 click_count 产生事件。"""
        cnt = self._click_count
        self._click_count = 0
        if cnt == 1:
            self._key_event = EVT_CLICK
        elif cnt == 2:
            self._key_event = EVT_DOUBLE
        elif cnt >= 3:
            self._key_event = EVT_TRIPLE

    # ------------------------------------------------------------------
    # 公开接口
    # ------------------------------------------------------------------
    def poll(self):
        """原子读取累积的编码器增量和按键事件，清零内部计数器。

        每帧主循环开头调用一次。
        返回 InputEvent 实例。
        """
        evt = InputEvent()
        # 原子读取并清零 delta
        evt.delta = self._delta
        self._delta = 0
        # 原子读取并清零按键事件
        evt.key = self._key_event
        self._key_event = EVT_NONE
        return evt

    def raw_key_pressed(self):
        """直接读取按键物理状态（油门模式专用）。

        返回 True 表示按键当前被按下（低电平有效）。
        """
        return self._key.value() == 0
