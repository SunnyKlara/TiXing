# main.py — Pico Turbo 应用入口
# 初始化顺序: PWM停止 → 显示 → 输入 → 配置 → 状态 → 效果 → 联动 → 渲染 → 界面

try:
    from utime import sleep_ms
    import gc
except ImportError:
    from time import sleep
    def sleep_ms(ms):
        sleep(ms / 1000.0)
    import gc

# 只提前 import Display，其他模块延迟到 Logo 上屏后再加载
from hal.display import Display


def main():
    """应用入口：Logo → 进度条 → 主循环。"""

    # ── 第 1 步：复用 boot.py 已初始化的屏幕（不重复硬件复位）──
    display = Display(skip_init=True)

    # ── 进度条参数 ──
    BAR_H = 3
    BAR_Y = 76 - BAR_H
    BAR_W = 284
    STEPS = 40
    STEP_W = (BAR_W + STEPS - 1) // STEPS
    WHITE = 0xFFFF
    _step = [0]  # 用列表包装以便闭包修改

    def _bar(n):
        """推进 n 步进度条"""
        st = display._st
        for _ in range(n):
            if _step[0] >= STEPS:
                break
            x = _step[0] * STEP_W
            w = min(STEP_W, BAR_W - x)
            if w > 0:
                st.fill_rect(x, BAR_Y, w, BAR_H, WHITE)
            _step[0] += 1

    # ── 第 2 步：边加载模块边推进度条 ──
    _bar(2)
    from hal.pwm_devices import PwmDevices
    from hal.input import Input
    _bar(3)
    from services.config import Config
    from services.effects import Effects
    from services.coordinator import Coordinator
    _bar(3)
    from app_state import AppState
    from ui.renderer import Renderer
    from screens import Context
    from screen_manager import ScreenManager, UI_BOOT, UI_MENU, UI_RGB
    _bar(4)
    from screens.boot import BootScreen
    from screens.menu import MenuWheelScreen
    from screens.speed import SpeedScreen
    from screens.smoke import SmokeScreen
    _bar(3)
    from screens.pump import PumpScreen
    from screens.preset import PresetScreen
    from screens.rgb import RGBScreen
    from screens.brightness import BrightnessScreen
    _bar(3)

    devices = PwmDevices()
    devices.stop_all()
    inp = Input()
    _bar(2)

    config = Config()
    state = AppState()
    config.load_to_state(state)
    effects = Effects(devices)
    coordinator = Coordinator(devices)
    _bar(3)

    # ── 第 3 步：UI 初始化 ──
    renderer = Renderer(display)
    ctx = Context(renderer, devices, config, state,
                  effects, coordinator, inp=inp)
    sm = ScreenManager(ctx)
    _bar(2)

    # 注册所有界面
    sm.register(0, BootScreen(ctx))
    sm.register(1, MenuWheelScreen(ctx))
    sm.register(2, SpeedScreen(ctx))
    sm.register(3, SmokeScreen(ctx))
    sm.register(4, PumpScreen(ctx))
    sm.register(5, PresetScreen(ctx))
    sm.register(6, RGBScreen(ctx))
    sm.register(7, BrightnessScreen(ctx))
    _bar(3)

    # ── 第 4 步：剩余进度条匀速走完 ──
    remaining = STEPS - _step[0]
    if remaining > 0:
        delay = 1500 // remaining  # 剩余 1.5 秒匀速填满
        for _ in range(remaining):
            _bar(1)
            sleep_ms(delay)

    sm.switch(UI_MENU)
    gc.collect()

    # ── 第 6 步：主循环 ──
    retry_count = 0
    max_retries = 3

    while True:
        try:
            # 6.1 读取输入
            event = inp.poll()

            # 6.2 事件分发
            sm.dispatch(event)

            # 6.3 渲染
            sm.render()

            # 6.4 灯光效果（RGB 调色界面除外）
            if sm.current_ui != UI_RGB:
                effects.process(state)

            # 6.5 设备联动
            coordinator.process()

            # 6.6 帧间隔
            sleep_ms(10)
            retry_count = 0  # 成功帧重置重试计数

        except MemoryError:
            gc.collect()
            # 释放字体缓存
            try:
                from ui.font import _font_cache
                _font_cache.clear()
            except Exception:
                pass
            gc.collect()
            retry_count += 1
            if retry_count >= max_retries:
                print('FATAL: MemoryError after {} retries'.format(max_retries))
                devices.stop_all()
                break

        except KeyboardInterrupt:
            print('User interrupt, stopping...')
            devices.stop_all()
            break

        except Exception as e:
            print('ERROR:', e)
            retry_count += 1
            if retry_count >= max_retries:
                print('FATAL: {} retries exceeded, stopping'.format(max_retries))
                devices.stop_all()
                break
            # 尝试恢复到菜单
            try:
                sm.switch(UI_MENU)
            except Exception:
                pass


if __name__ == '__main__':
    main()
