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

from hal.pwm_devices import PwmDevices
from hal.display import Display
from hal.input import Input
from services.config import Config
from services.effects import Effects
from services.coordinator import Coordinator
from app_state import AppState
from ui.renderer import Renderer
from screens import Context
from screen_manager import ScreenManager, UI_BOOT, UI_MENU, UI_RGB

# 界面导入
from screens.boot import BootScreen
from screens.menu import MenuWheelScreen
from screens.speed import SpeedScreen
from screens.smoke import SmokeScreen
from screens.pump import PumpScreen
from screens.preset import PresetScreen
from screens.rgb import RGBScreen
from screens.brightness import BrightnessScreen


def main():
    """应用入口：初始化 → 开机动画 → 主循环。"""

    # ── 第 1 步：硬件初始化 ──
    devices = PwmDevices()
    devices.stop_all()
    display = Display()
    inp = Input()

    # ── 第 2 步：服务初始化 ──
    config = Config()
    state = AppState()
    config.load_to_state(state)
    effects = Effects(devices)
    coordinator = Coordinator(devices)

    # ── 第 3 步：UI 初始化 ──
    renderer = Renderer(display)
    ctx = Context(renderer, devices, config, state,
                  effects, coordinator, inp=inp)
    sm = ScreenManager(ctx)

    # 注册所有界面
    sm.register(0, BootScreen(ctx))    # UI_BOOT
    sm.register(1, MenuWheelScreen(ctx))    # UI_MENU
    sm.register(2, SpeedScreen(ctx))   # UI_SPEED
    sm.register(3, SmokeScreen(ctx))   # UI_SMOKE
    sm.register(4, PumpScreen(ctx))    # UI_PUMP
    sm.register(5, PresetScreen(ctx))  # UI_PRESET
    sm.register(6, RGBScreen(ctx))     # UI_RGB
    sm.register(7, BrightnessScreen(ctx))  # UI_BRIGHT

    # ── 第 4 步：开机动画 ──
    sm.switch(UI_BOOT)
    sm.render()
    effects.startup_animation()

    # ── 第 5 步：进入菜单 ──
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
