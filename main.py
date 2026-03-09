# main.py - 76*284横屏多级界面控制程序
# 主循环、状态管理和交互逻辑
# 采用 f4 风格的 ui/init_flag 双变量界面状态机

from st7789py import ST7789, BGR, sleep_ms
from machine import Pin, SPI, PWM
import utime
from control import (
    set_fan_speed, set_smoke_generator_speed,
    set_cob1_rgb, set_cob2_rgb, set_air_pump_speed, set_small_fan_speed,
    stop_all_devices
)
from coordinator import DeviceCoordinator
from key import get_encoder_delta, get_key_event, clear_encoder_count, key_pin
from display import (
    draw_logo_screen, draw_mode_select_screen,
    draw_windspeed_screen, draw_smokespeed_screen,
    draw_rgbcontrol_screen, draw_airpump_screen,
    draw_preset_screen, draw_brightness_screen,
    update_windspeed_screen, update_smokespeed_screen,
    update_airpump_screen, update_brightness_screen,
    update_preset_screen, update_rgbcontrol_screen,
    BACKGROUND_COLOR, COLOR_PRESETS
)
from effects import effects_process, reset_effects, startup_animation
import config

# --------------------------
# 界面状态常量（参考 f4 UI0-UI7，适配 Pico 6 选项菜单）
# --------------------------
UI_LOGO = 0
UI_MENU = 1
UI_WINDSPEED = 2
UI_SMOKESPEED = 3
UI_AIRPUMP = 4
UI_PRESET = 5
UI_RGBCONTROL = 6
UI_BRIGHTNESS = 7

# 菜单索引到界面常量的映射（6 选项）
MENU_SCREEN_MAP = [UI_WINDSPEED, UI_SMOKESPEED, UI_AIRPUMP, UI_PRESET, UI_RGBCONTROL, UI_BRIGHTNESS]

# --------------------------
# 全局状态变量
# --------------------------
# f4 风格界面状态机：ui 控制当前界面，init_flag 控制一次性初始化
ui = UI_LOGO            # 当前界面编号（对应 f4 的 ui 变量）
init_flag = True        # 界面初始化标志（对应 f4 的 chu 变量）

current_mode_idx = 0    # 菜单选中项索引（0-5，对应 6 个选项）
fan_speed = 0
smoke_speed = 0
air_pump_speed = 0      # 气泵速度
throttle_mode = False   # 油门模式开关（参考 f4 的 wuhuaqi_state==2）
brightness = 100        # 全局亮度
breath_mode = False     # 呼吸灯模式开关（参考 f4 的 breath_mode）
color_preset_idx = 0    # 灯光预设索引（0-11）
gradient_mode = False   # 渐变模式开关
cob1_rgb = [0, 0, 0]       # COB1 RGB 值（0-255）
cob2_rgb = [0, 0, 0]       # COB2 RGB 值（0-255）
rgb_mode = 0                # 三层状态机：0=选灯带, 1=选通道, 2=调数值
rgb_strip = 0               # 0=COB1, 1=COB2
rgb_channel = 0             # 0=R, 1=G, 2=B
last_mode_idx = 0

# 上一帧状态缓存（用于局部刷新对比）
_prev_state = {}

# 菜单滚动防跳（150ms 间隔，避免鼠标滚轮快速滚动跳过多个选项）
_last_menu_scroll_time = 0
MENU_SCROLL_DEBOUNCE_MS = 150

# --------------------------
# 硬件初始化
# --------------------------
custom_rotations = (
    (0x00, 76, 284, 82, 18, False),
    (0x60, 284, 76, 18, 82, False),
    (0xC0, 76, 284, 82, 18, False),
    (0xA0, 284, 76, 18, 82, False),
)

spi = SPI(
    0,
    baudrate=20000000,
    sck=Pin(2),
    mosi=Pin(3),
    miso=None
)

dc = Pin(0, Pin.OUT)
cs = Pin(1, Pin.OUT)
reset = Pin(5, Pin.OUT)

tft = ST7789(
    spi=spi,
    width=76,
    height=284,
    dc=dc,
    cs=cs,
    reset=reset,
    rotation=1,
    color_order=BGR,
    custom_rotations=custom_rotations
)

# --------------------------
# 状态字典（传递给 display 绘制函数）
# --------------------------
def get_state():
    """构建当前状态字典，供 display 模块使用"""
    return {
        "current_mode_idx": current_mode_idx,
        "fan_speed": fan_speed,
        "smoke_speed": smoke_speed,
        "air_pump_speed": air_pump_speed,
        "throttle_mode": throttle_mode,
        "brightness": brightness,
        "breath_mode": breath_mode,
        "color_preset_idx": color_preset_idx,
        "gradient_mode": gradient_mode,
        "cob1_rgb": cob1_rgb,
        "cob2_rgb": cob2_rgb,
        "rgb_mode": rgb_mode,
        "rgb_strip": rgb_strip,
        "rgb_channel": rgb_channel,
    }

# --------------------------
# 灯光预设辅助函数
# --------------------------
def apply_preset_colors(idx):
    """将预设颜色应用到 COB1/COB2 灯带（RGB 0-255 转 0-100）"""
    preset = COLOR_PRESETS[idx]
    c1 = preset["cob1"]
    c2 = preset["cob2"]
    set_cob1_rgb(c1[0] * 100 // 255, c1[1] * 100 // 255, c1[2] * 100 // 255)
    set_cob2_rgb(c2[0] * 100 // 255, c2[1] * 100 // 255, c2[2] * 100 // 255)

# --------------------------
# 硬件同步
# --------------------------
def update_hardware():
    """同步参数到硬件（RGB 0-255 转 0-100 给 control.py）"""
    set_fan_speed(fan_speed)
    set_smoke_generator_speed(smoke_speed)
    set_cob1_rgb(cob1_rgb[0] * 100 // 255, cob1_rgb[1] * 100 // 255, cob1_rgb[2] * 100 // 255)
    set_cob2_rgb(cob2_rgb[0] * 100 // 255, cob2_rgb[1] * 100 // 255, cob2_rgb[2] * 100 // 255)

# --------------------------
# 配置持久化辅助函数（参考 f4 的 deng_update() 写入时机）
# --------------------------
def save_current_config():
    """双击返回菜单时调用：将当前全局状态批量写入 config 并保存"""
    # 确定当前 LED 模式
    if breath_mode:
        led_mode = "breathing"
    elif gradient_mode:
        led_mode = "gradient"
    else:
        led_mode = "normal"

    config.set("fan_speed", fan_speed)
    config.set("smoke_speed", smoke_speed)
    config.set("air_pump_speed", air_pump_speed)
    config.set("cob1_rgb", list(cob1_rgb))
    config.set("cob2_rgb", list(cob2_rgb))
    config.set("brightness", brightness)
    config.set("color_preset", color_preset_idx)
    config.set("led_mode", led_mode)
    config.save_config()

# --------------------------
# 界面切换（f4 风格：只设状态，不绘制）
# --------------------------
def switch_screen(new_screen):
    """切换界面：设置 ui 和 init_flag，实际绘制由 update_display() 处理"""
    global ui, init_flag
    ui = new_screen
    init_flag = True
    # 进入 RGB 调色界面时重置效果状态（参考 f4 进入 UI3 时暂停灯效）
    if new_screen == UI_RGBCONTROL:
        reset_effects()

# --------------------------
# 界面刷新（f4 风格：含初始化检测）
# --------------------------
def update_display():
    """
    每帧调用一次。
    init_flag=True 时执行完整绘制（清屏+静态元素+动态元素），然后重置。
    init_flag=False 时调用局部刷新函数，仅更新变化的区域。
    参考 f4 的 chu 变量机制：进入新界面时 chu 触发一次性初始化，
    之后每帧只刷新变化的部分，避免全屏重绘。
    """
    global init_flag, _prev_state
    state = get_state()

    if ui == UI_LOGO:
        if init_flag:
            draw_logo_screen(tft, state)
            init_flag = False

    elif ui == UI_MENU:
        if init_flag:
            draw_mode_select_screen(tft, state)
            init_flag = False

    elif ui == UI_WINDSPEED:
        if init_flag:
            draw_windspeed_screen(tft, state)
            init_flag = False
        else:
            update_windspeed_screen(tft, state, _prev_state)

    elif ui == UI_SMOKESPEED:
        if init_flag:
            draw_smokespeed_screen(tft, state)
            init_flag = False
        else:
            update_smokespeed_screen(tft, state, _prev_state)

    elif ui == UI_AIRPUMP:
        if init_flag:
            draw_airpump_screen(tft, state)
            init_flag = False
        else:
            update_airpump_screen(tft, state, _prev_state)

    elif ui == UI_PRESET:
        if init_flag:
            draw_preset_screen(tft, state)
            init_flag = False
        else:
            update_preset_screen(tft, state, _prev_state)

    elif ui == UI_RGBCONTROL:
        if init_flag:
            draw_rgbcontrol_screen(tft, state)
            init_flag = False
        else:
            update_rgbcontrol_screen(tft, state, _prev_state)

    elif ui == UI_BRIGHTNESS:
        if init_flag:
            draw_brightness_screen(tft, state)
            init_flag = False
        else:
            update_brightness_screen(tft, state, _prev_state)

    # 每帧结束时缓存当前状态的深拷贝（MicroPython 无 copy.deepcopy，手动复制列表）
    _prev_state = dict(state)
    _prev_state["cob1_rgb"] = list(state["cob1_rgb"])
    _prev_state["cob2_rgb"] = list(state["cob2_rgb"])


# --------------------------
# 主程序
# --------------------------
if __name__ == "__main__":
    print("76*284横屏多级界面程序启动...")

    # ========== 启动序列（Req 10.1）==========
    # 步骤 1/5：硬件引脚（SPI, Pin, tft 已在模块级初始化）
    stop_all_devices()

    # 步骤 2/5：配置加载（参考 f4 的 deng_init() 从 Flash 读取上次设置）
    cfg = config.load_config()
    fan_speed = cfg.get("fan_speed", 0)
    smoke_speed = cfg.get("smoke_speed", 0)
    air_pump_speed = cfg.get("air_pump_speed", 0)
    cob1_rgb = cfg.get("cob1_rgb", [0, 0, 0])
    cob2_rgb = cfg.get("cob2_rgb", [0, 0, 0])
    brightness = cfg.get("brightness", 100)
    color_preset_idx = cfg.get("color_preset", 0)
    _led_mode = cfg.get("led_mode", "normal")
    breath_mode = _led_mode == "breathing"
    gradient_mode = _led_mode == "gradient"

    # 步骤 3/5：外设初始化（设备联动协调器，Req 9.1, 9.2, 9.3, 9.5）
    coordinator = DeviceCoordinator(cfg)

    # 步骤 4/5：开机动画（Logo 显示 + COB 灯带闪烁渐亮，参考 f4 Startup_TaillightFlash）
    switch_screen(UI_LOGO)
    update_display()
    startup_animation(set_cob1_rgb, set_cob2_rgb, sleep_ms)

    # 步骤 5/5：进入菜单
    switch_screen(UI_MENU)

    # ========== 主循环（带重试，Req 10.3）==========
    MAX_RETRIES = 3
    _retry_count = 0

    while _retry_count < MAX_RETRIES:
        try:
            while True:
                # 统一读取编码器增量（参考 f4 的 Encoder() 每帧读一次模式）
                encoder_delta = get_encoder_delta()
                key_event = get_key_event()

                # --- 交互处理（按界面分发） ---

                # LOGO界面（仅开机显示，无交互）
                if ui == UI_LOGO:
                    pass

                # 菜单界面交互（6 选项水平滚动，带 150ms 防跳）
                elif ui == UI_MENU:
                    if encoder_delta != 0:
                        now = utime.ticks_ms()
                        if utime.ticks_diff(now, _last_menu_scroll_time) >= MENU_SCROLL_DEBOUNCE_MS:
                            _last_menu_scroll_time = now
                            # 菜单只移动一格，忽略 delta 大小（防止快速滚动跳过选项）
                            direction = 1 if encoder_delta > 0 else -1
                            current_mode_idx = (current_mode_idx + direction) % 6
                            init_flag = True  # 触发重绘

                    if key_event == "click1":
                        last_mode_idx = current_mode_idx
                        switch_screen(MENU_SCREEN_MAP[current_mode_idx])

                # 风速控制界面交互（0-100 范围，delta*1 加速）
                # 油门模式：三击进入，按住加速松开减速，旋转退出（参考 f4 wuhuaqi_state==2）
                elif ui == UI_WINDSPEED:
                    if throttle_mode:
                        # 油门模式下：旋转退出油门模式（参考 f4 Throttle_ResetState）
                        if encoder_delta != 0:
                            throttle_mode = False
                            # 退出油门模式后，正常处理这次旋转
                            fan_speed = max(0, min(100, fan_speed + encoder_delta * 1))
                            init_flag = True
                            update_hardware()
                            coordinator.on_fan_speed_change(fan_speed, set_small_fan_speed)
                        else:
                            # 油门模式核心：读取按键物理状态（key_pin 低电平=按下）
                            if key_pin.value() == 0:
                                # 按住：非线性加速（参考 f4 加速曲线）
                                # speed < 50 时 +1/帧（慢区精细控制），>= 50 时 +2/帧（快区快速拉满）
                                accel = 1 if fan_speed < 50 else 2
                                fan_speed = min(100, fan_speed + accel)
                            else:
                                # 松开：线性减速 -1/帧
                                fan_speed = max(0, fan_speed - 1)
                            # 不设 init_flag，由局部刷新处理连续变化
                            update_hardware()
                            coordinator.on_fan_speed_change(fan_speed, set_small_fan_speed)
                    else:
                        # 普通模式：旋转调速
                        if encoder_delta != 0:
                            fan_speed = max(0, min(100, fan_speed + encoder_delta * 1))
                            # 不设 init_flag，由局部刷新处理值变化
                            update_hardware()
                            coordinator.on_fan_speed_change(fan_speed, set_small_fan_speed)

                        # 三击进入油门模式
                        if key_event == "click3":
                            throttle_mode = True
                            init_flag = True

                    if key_event == "click2":
                        throttle_mode = False  # 退出时重置油门模式
                        save_current_config()
                        current_mode_idx = last_mode_idx
                        switch_screen(UI_MENU)

                # 发烟速度控制界面交互（0-100 范围，delta*1 加速）
                elif ui == UI_SMOKESPEED:
                    if encoder_delta != 0:
                        smoke_speed = max(0, min(100, smoke_speed + encoder_delta * 1))
                        # 不设 init_flag，由局部刷新处理值变化
                        update_hardware()
                        coordinator.on_smoke_change(smoke_speed, set_air_pump_speed, utime.ticks_ms())

                    if key_event == "click2":
                        save_current_config()
                        current_mode_idx = last_mode_idx
                        switch_screen(UI_MENU)

                # 气泵控制界面交互（0-100 范围，占位）
                elif ui == UI_AIRPUMP:
                    if encoder_delta != 0:
                        air_pump_speed = max(0, min(100, air_pump_speed + encoder_delta * 1))
                        # 不设 init_flag，由局部刷新处理值变化
                        set_air_pump_speed(air_pump_speed)

                    if key_event == "click2":
                        save_current_config()
                        current_mode_idx = last_mode_idx
                        switch_screen(UI_MENU)

                # 灯光预设界面交互（旋转切换预设，单击切换渐变，双击返回）
                elif ui == UI_PRESET:
                    if encoder_delta != 0:
                        color_preset_idx = (color_preset_idx + encoder_delta) % len(COLOR_PRESETS)
                        apply_preset_colors(color_preset_idx)
                        # 不设 init_flag，由局部刷新处理预设切换

                    if key_event == "click1":
                        gradient_mode = not gradient_mode
                        # 不设 init_flag，由局部刷新处理渐变模式切换

                    if key_event == "click2":
                        save_current_config()
                        current_mode_idx = last_mode_idx
                        switch_screen(UI_MENU)

                # RGB控制界面交互（三层状态机：选灯带→选通道→调数值，参考 f4 UI3）
                elif ui == UI_RGBCONTROL:
                    if encoder_delta != 0:
                        if rgb_mode == 0:
                            # Mode 0: 选灯带 - 旋转切换 COB1/COB2
                            direction = 1 if encoder_delta > 0 else -1
                            rgb_strip = (rgb_strip + direction) % 2
                        elif rgb_mode == 1:
                            # Mode 1: 选通道 - 旋转切换 R/G/B
                            direction = 1 if encoder_delta > 0 else -1
                            rgb_channel = (rgb_channel + direction) % 3
                        elif rgb_mode == 2:
                            # Mode 2: 调数值 - 旋转调 0-255，delta*2 加速
                            vals = cob1_rgb if rgb_strip == 0 else cob2_rgb
                            vals[rgb_channel] = max(0, min(255, vals[rgb_channel] + encoder_delta * 2))
                            update_hardware()
                        # 不设 init_flag，由局部刷新处理 RGB 变化

                    if key_event == "click1":
                        if rgb_mode == 0:
                            rgb_mode = 1  # 选灯带 → 选通道
                        elif rgb_mode == 1:
                            rgb_mode = 2  # 选通道 → 调数值
                        elif rgb_mode == 2:
                            rgb_mode = 1  # 调数值 → 回到选通道
                        # 不设 init_flag，由局部刷新处理模式切换

                    if key_event == "click2":
                        rgb_mode = 0  # 任何模式双击：重置并返回菜单
                        save_current_config()
                        current_mode_idx = last_mode_idx
                        switch_screen(UI_MENU)

                # 亮度调节界面交互（0-100 范围，单击切换呼吸灯，双击返回）
                elif ui == UI_BRIGHTNESS:
                    if encoder_delta != 0:
                        brightness = max(0, min(100, brightness + encoder_delta * 1))
                        # 不设 init_flag，由局部刷新处理亮度变化

                    if key_event == "click1":
                        breath_mode = not breath_mode
                        # 不设 init_flag，由局部刷新处理呼吸灯切换

                    if key_event == "click2":
                        save_current_config()
                        current_mode_idx = last_mode_idx
                        switch_screen(UI_MENU)

                # --- 界面刷新（每帧一次） ---
                update_display()

                # --- 灯光效果处理（每帧一次，参考 f4 主循环中的 LED 效果调用） ---
                # 在 RGB 调色界面时不运行效果（normal 模式，用户直接控制颜色）
                if ui != UI_RGBCONTROL:
                    effects_process(get_state(), set_cob1_rgb, set_cob2_rgb)

                # 设备联动协调器每帧处理（非阻塞延迟任务，如气泵延迟关闭）
                coordinator.process(utime.ticks_ms(), set_air_pump_speed)

                sleep_ms(10)

        except KeyboardInterrupt:
            # 用户主动中断：安全停止，不重试
            stop_all_devices()
            tft.fill(BACKGROUND_COLOR)
            print("程序已停止")
            break

        except Exception as e:
            # 未捕获异常：停止外设，输出错误信息（Req 10.3）
            _retry_count += 1
            stop_all_devices()
            print("ERROR:", e)
            if _retry_count < MAX_RETRIES:
                print("重启主循环... (重试 {}/{})".format(_retry_count, MAX_RETRIES))
                # 重置到菜单界面后重新进入主循环
                switch_screen(UI_MENU)
                sleep_ms(500)
            else:
                print("已达最大重试次数 ({}), 系统停止".format(MAX_RETRIES))
                tft.fill(BACKGROUND_COLOR)
