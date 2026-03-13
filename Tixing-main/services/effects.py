# services/effects.py — 灯光效果引擎
# 支持 normal / breathing / gradient 三种模式

try:
    from math import sin, pi
    from utime import sleep_ms
except ImportError:
    from math import sin, pi
    from time import sleep

    def sleep_ms(ms):
        sleep(ms / 1000.0)


class Effects:
    """灯光效果引擎。

    优先级: 手动 RGB > 呼吸灯 > 渐变 > 普通
    """

    def __init__(self, devices):
        self._devices = devices
        self._phase = 0       # 呼吸灯相位 (0-628, 映射 0-2π)
        self._grad_step = 0   # 渐变步进 (0-255)
        self._grad_dir = 1    # 渐变方向

    def process(self, state):
        """每帧更新灯带输出。

        根据 state.breath_mode 和 state.gradient_mode 决定模式。
        """
        c1 = state.cob1_rgb
        c2 = state.cob2_rgb
        br = state.brightness / 100.0

        if state.breath_mode:
            # 呼吸灯：sin 波调制亮度
            self._phase = (self._phase + 4) % 628
            factor = (sin(self._phase * pi / 314) + 1.0) / 2.0
            factor *= br
            r1 = int(c1[0] * factor)
            g1 = int(c1[1] * factor)
            b1 = int(c1[2] * factor)
            r2 = int(c2[0] * factor)
            g2 = int(c2[1] * factor)
            b2 = int(c2[2] * factor)
        elif state.gradient_mode:
            # 渐变：COB1 和 COB2 之间线性插值循环
            self._grad_step += self._grad_dir * 2
            if self._grad_step >= 255:
                self._grad_step = 255
                self._grad_dir = -1
            elif self._grad_step <= 0:
                self._grad_step = 0
                self._grad_dir = 1
            t = self._grad_step
            it = 255 - t
            r1 = int((c1[0] * it + c2[0] * t) / 255 * br)
            g1 = int((c1[1] * it + c2[1] * t) / 255 * br)
            b1 = int((c1[2] * it + c2[2] * t) / 255 * br)
            r2 = int((c2[0] * it + c1[0] * t) / 255 * br)
            g2 = int((c2[1] * it + c1[1] * t) / 255 * br)
            b2 = int((c2[2] * it + c1[2] * t) / 255 * br)
        else:
            # 普通模式：直接输出
            r1 = int(c1[0] * br)
            g1 = int(c1[1] * br)
            b1 = int(c1[2] * br)
            r2 = int(c2[0] * br)
            g2 = int(c2[1] * br)
            b2 = int(c2[2] * br)

        self._devices.set_cob1(r1, g1, b1)
        self._devices.set_cob2(r2, g2, b2)

    def startup_animation(self):
        """开机动画：COB 灯带交替闪烁 + 渐亮。"""
        d = self._devices
        # 交替闪烁 3 次
        for _ in range(3):
            d.set_cob1(255, 255, 255)
            d.set_cob2(0, 0, 0)
            sleep_ms(80)
            d.set_cob1(0, 0, 0)
            d.set_cob2(255, 255, 255)
            sleep_ms(80)
        d.set_cob2(0, 0, 0)

        # 渐亮
        for i in range(0, 256, 8):
            d.set_cob1(i, i, i)
            d.set_cob2(i, i, i)
            sleep_ms(10)

        # 渐灭
        for i in range(255, -1, -8):
            v = max(0, i)
            d.set_cob1(v, v, v)
            d.set_cob2(v, v, v)
            sleep_ms(10)

        d.set_cob1(0, 0, 0)
        d.set_cob2(0, 0, 0)

    def reset_phase(self):
        """重置效果相位（界面切换时调用）。"""
        self._phase = 0
        self._grad_step = 0
        self._grad_dir = 1
