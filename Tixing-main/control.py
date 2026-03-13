# control.py - 外设控制模块（风扇/气泵/发烟器/双路COB灯带）
# 适配Raspberry Pi Pico (MicroPython)
from machine import Pin, PWM

# --------------------------
# 全局参数配置（统一管理，便于修改）
# --------------------------
# 所有PWM外设统一频率：1KHz
PWM_FREQ = 1000
# PWM占空比最大值（MicroPython 16位范围：0-65535）
MAX_DUTY = 65535

# --------------------------
# 外设GPIO引脚定义（按功能分类）
# --------------------------
# 1. PWM调速类外设
FAN_GPIO = 6          # 主风扇
SMALL_FAN_GPIO = 10   # 小风扇
AIR_PUMP_GPIO = 11    # 气泵
SMOKE_GENERATOR_GPIO = 12  # 发烟器

# 2. COB灯带RGB引脚
# COB1灯带
COB1_R_GPIO = 13
COB1_G_GPIO = 14
COB1_B_GPIO = 15
# COB2灯带（新增）
COB2_R_GPIO = 7
COB2_G_GPIO = 8
COB2_B_GPIO = 9

# --------------------------
# 外设初始化（导入模块时自动执行，确保即插即用）
# --------------------------
# ========== PWM调速外设初始化 ==========
# 主风扇（GPIO6）
fan_pwm = PWM(Pin(FAN_GPIO, Pin.OUT))
fan_pwm.freq(PWM_FREQ)
fan_pwm.duty_u16(0)  # 初始停止

# 小风扇（GPIO10）
small_fan_pwm = PWM(Pin(SMALL_FAN_GPIO, Pin.OUT))
small_fan_pwm.freq(PWM_FREQ)
small_fan_pwm.duty_u16(0)

# 气泵（GPIO11）
air_pump_pwm = PWM(Pin(AIR_PUMP_GPIO, Pin.OUT))
air_pump_pwm.freq(PWM_FREQ)
air_pump_pwm.duty_u16(0)

# 发烟器（GPIO12）
smoke_generator_pwm = PWM(Pin(SMOKE_GENERATOR_GPIO, Pin.OUT))
smoke_generator_pwm.freq(PWM_FREQ)
smoke_generator_pwm.duty_u16(0)

# ========== COB灯带PWM初始化 ==========
# COB1灯带RGB
cob1_r_pwm = PWM(Pin(COB1_R_GPIO, Pin.OUT))
cob1_r_pwm.freq(PWM_FREQ)
cob1_r_pwm.duty_u16(0)

cob1_g_pwm = PWM(Pin(COB1_G_GPIO, Pin.OUT))
cob1_g_pwm.freq(PWM_FREQ)
cob1_g_pwm.duty_u16(0)

cob1_b_pwm = PWM(Pin(COB1_B_GPIO, Pin.OUT))
cob1_b_pwm.freq(PWM_FREQ)
cob1_b_pwm.duty_u16(0)

# COB2灯带RGB（新增）
cob2_r_pwm = PWM(Pin(COB2_R_GPIO, Pin.OUT))
cob2_r_pwm.freq(PWM_FREQ)
cob2_r_pwm.duty_u16(0)

cob2_g_pwm = PWM(Pin(COB2_G_GPIO, Pin.OUT))
cob2_g_pwm.freq(PWM_FREQ)
cob2_g_pwm.duty_u16(0)

cob2_b_pwm = PWM(Pin(COB2_B_GPIO, Pin.OUT))
cob2_b_pwm.freq(PWM_FREQ)
cob2_b_pwm.duty_u16(0)

# --------------------------
# 核心控制函数（参数：0-100百分比，带边界保护）
# --------------------------
# ========== 风扇类外设控制 ==========
def set_fan_speed(speed):
    """
    控制GPIO6主风扇转速
    :param speed: 转速百分比（0=停止，100=最大转速）
    """
    speed = max(0, min(100, speed))  # 边界保护，避免参数越界
    duty = int(speed / 100 * MAX_DUTY)
    fan_pwm.duty_u16(duty)

def set_small_fan_speed(speed):
    """
    控制GPIO10小风扇转速
    :param speed: 转速百分比（0=停止，100=最大转速）
    """
    speed = max(0, min(100, speed))
    duty = int(speed / 100 * MAX_DUTY)
    small_fan_pwm.duty_u16(duty)

# ========== 气泵/发烟器控制 ==========
def set_air_pump_speed(speed):
    """
    控制GPIO11气泵功率（转速/气压）
    :param speed: 功率百分比（0=停止，100=最大功率）
    """
    speed = max(0, min(100, speed))
    duty = int(speed / 100 * MAX_DUTY)
    air_pump_pwm.duty_u16(duty)

def set_smoke_generator_speed(speed):
    """
    控制GPIO12发烟器出烟量
    :param speed: 功率百分比（0=停止，100=最大出烟量）
    """
    speed = max(0, min(100, speed))
    duty = int(speed / 100 * MAX_DUTY)
    smoke_generator_pwm.duty_u16(duty)

# ========== COB1灯带控制 ==========
def set_cob1_rgb(r, g, b):
    """
    控制COB1灯带（GPIO7=R, GPIO8=G, GPIO9=B）
    :param r: 红色亮度（0-100）
    :param g: 绿色亮度（0-100）
    :param b: 蓝色亮度（0-100）
    """
    # 边界保护
    r = max(0, min(100, r))
    g = max(0, min(100, g))
    b = max(0, min(100, b))
    # 转换为16位占空比
    r_duty = int(r / 100 * MAX_DUTY)
    g_duty = int(g / 100 * MAX_DUTY)
    b_duty = int(b / 100 * MAX_DUTY)
    # 设置亮度
    cob1_r_pwm.duty_u16(r_duty)
    cob1_g_pwm.duty_u16(g_duty)
    cob1_b_pwm.duty_u16(b_duty)

def set_cob1_single_color(channel, brightness):
    """
    单独控制COB1某一个颜色通道
    :param channel: 通道名（'r'/'g'/'b'）
    :param brightness: 亮度（0-100）
    """
    brightness = max(0, min(100, brightness))
    duty = int(brightness / 100 * MAX_DUTY)
    if channel.lower() == 'r':
        cob1_r_pwm.duty_u16(duty)
    elif channel.lower() == 'g':
        cob1_g_pwm.duty_u16(duty)
    elif channel.lower() == 'b':
        cob1_b_pwm.duty_u16(duty)
    else:
        raise ValueError("channel必须是 'r'/'g'/'b'")

# ========== COB2灯带控制（新增） ==========
def set_cob2_rgb(r, g, b):
    """
    控制COB2灯带（GPIO13=R, GPIO14=G, GPIO15=B）
    :param r: 红色亮度（0-100）
    :param g: 绿色亮度（0-100）
    :param b: 蓝色亮度（0-100）
    """
    r = max(0, min(100, r))
    g = max(0, min(100, g))
    b = max(0, min(100, b))
    r_duty = int(r / 100 * MAX_DUTY)
    g_duty = int(g / 100 * MAX_DUTY)
    b_duty = int(b / 100 * MAX_DUTY)
    cob2_r_pwm.duty_u16(r_duty)
    cob2_g_pwm.duty_u16(g_duty)
    cob2_b_pwm.duty_u16(b_duty)

def set_cob2_single_color(channel, brightness):
    """
    单独控制COB2某一个颜色通道
    :param channel: 通道名（'r'/'g'/'b'）
    :param brightness: 亮度（0-100）
    """
    brightness = max(0, min(100, brightness))
    duty = int(brightness / 100 * MAX_DUTY)
    if channel.lower() == 'r':
        cob2_r_pwm.duty_u16(duty)
    elif channel.lower() == 'g':
        cob2_g_pwm.duty_u16(duty)
    elif channel.lower() == 'b':
        cob2_b_pwm.duty_u16(duty)
    else:
        raise ValueError("channel必须是 'r'/'g'/'b'")

# ========== 全局控制函数（安全防护） ==========
def stop_all_devices():
    """
    一键停止所有外设（风扇/气泵/发烟器/所有灯带）
    """
    # 停止调速类外设
    set_fan_speed(0)
    set_small_fan_speed(0)
    set_air_pump_speed(0)
    set_smoke_generator_speed(0)
    # 关闭所有灯带
    set_cob1_rgb(0, 0, 0)
    set_cob2_rgb(0, 0, 0)

def set_all_cob_rgb(r1, g1, b1, r2, g2, b2):
    """
    同时设置COB1和COB2灯带颜色（批量控制）
    :param r1/g1/b1: COB1的RGB亮度
    :param r2/g2/b2: COB2的RGB亮度
    """
    set_cob1_rgb(r1, g1, b1)
    set_cob2_rgb(r2, g2, b2)