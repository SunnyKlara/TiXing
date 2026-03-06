# main.py - 76*284横屏多级界面控制程序
# 导入驱动库核心组件
from st7789py import ST7789, BLACK, RED, GREEN, BLUE, WHITE, sleep_ms, BGR, color565
# 导入硬件控制模块
from machine import Pin, SPI, PWM
# 导入自定义字体模块
from lcdfont import font8x8
# 导入control模块的控制函数
from control import (
    set_fan_speed, set_smoke_generator_speed,
    set_cob1_rgb, stop_all_devices
)
# 导入编码器控制模块
from key import get_encoder_dir, get_key_event, clear_encoder_count

# --------------------------
# 全局常量定义
# --------------------------
# 界面状态定义 
LOGO_SCREEN = 0
MODE_SELECT_SCREEN = 1
WINDSPEED_SCREEN = 2
SMOKESPEED_SCREEN = 3
RGBCONTROL_SCREEN = 4

# 模式选项
MODES = ["windspeed", "smokespeed", "rgbcontrol"]
# 颜色定义
HIGHLIGHT_COLOR = GREEN    # 高亮色（选中状态）
NORMAL_COLOR = WHITE       # 正常文字色
GRAY_COLOR = color565(80, 80, 80)  # 灰色（未选中/背景）
DARK_COLOR = color565(40, 40, 40)  # 暗色调（RGB未选中通道）
BACKGROUND_COLOR = BLACK   # 背景色

# 界面布局参数（76*284横屏，rotation=1）
SCREEN_WIDTH = 284  # 实际显示宽度
SCREEN_HEIGHT = 76  # 实际显示高度
CHAR_WIDTH = font8x8.WIDTH
CHAR_HEIGHT = font8x8.HEIGHT

# LOGO界面参数
LOGO_TEXT = "Hello World"
LOGO_X = (SCREEN_WIDTH - len(LOGO_TEXT)*CHAR_WIDTH) // 2
LOGO_Y = (SCREEN_HEIGHT - CHAR_HEIGHT) // 2

# 模式选择界面参数
MODE_TEXT_Y = 20  # 模式文字Y坐标（居中靠上）
DOT_RADIUS = 3    # 小圆点半径
DOT_Y = MODE_TEXT_Y + CHAR_HEIGHT + 10  # 圆点Y坐标
# 三个圆点X坐标（均匀分布）
DOT_X_LIST = [
    SCREEN_WIDTH//4 - DOT_RADIUS,
    SCREEN_WIDTH//2 - DOT_RADIUS,
    3*SCREEN_WIDTH//4 - DOT_RADIUS
]

# 风速/发烟速度界面参数
SPEED_TEXT_Y = 20
PROGRESS_BAR_Y = SPEED_TEXT_Y + CHAR_HEIGHT + 10
PROGRESS_BAR_HEIGHT = 10  # 进度条高度（竖条总高度）
PROGRESS_BAR_WIDTH = 200  # 进度条总宽度
PROGRESS_BAR_X = (SCREEN_WIDTH - PROGRESS_BAR_WIDTH) // 2
PROGRESS_MIN_X = PROGRESS_BAR_X
PROGRESS_MAX_X = PROGRESS_BAR_X + PROGRESS_BAR_WIDTH
# 进度条刻度参数（密集竖条）
BAR_COUNT = 100    # 100个竖条对应0-100%
BAR_WIDTH = 2      # 单个竖条宽度
BAR_SPACING = 0    # 竖条间距
# 百分比显示位置
PERCENT_X = PROGRESS_MAX_X + 10
PERCENT_Y = PROGRESS_BAR_Y

# RGB控制界面参数
RGB_TEXT_Y = 10
RGB_CHANNEL_Y_LIST = [
    RGB_TEXT_Y + CHAR_HEIGHT + 10,
    RGB_TEXT_Y + CHAR_HEIGHT + 30,
    RGB_TEXT_Y + CHAR_HEIGHT + 50
]
RGB_CHANNEL_NAMES = ["R", "G", "B"]
RGB_PERCENT_X = 40  # 百分比显示X坐标

# --------------------------
# 全局状态变量
# --------------------------
current_screen = LOGO_SCREEN          # 当前界面
current_mode_idx = 0                  # 模式选择界面当前选中索引（0:风速,1:发烟,2:RGB）
fan_speed = 0                         # 风速值(0-100)
smoke_speed = 0                       # 发烟速度值(0-100)
rgb_values = [0, 0, 0]                # COB1 RGB值(0-100)
rgb_selected_channel = 0              # RGB当前选中通道(0:R,1:G,2:B)
rgb_editing = False                   # RGB编辑状态（True:编辑中，False:仅选择）
last_mode_idx = 0                     # 记录上次选中的模式（返回时恢复）

# --------------------------
# 硬件初始化（保留原有配置）
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
    rotation=1,  # 横屏模式
    color_order=BGR,
    custom_rotations=custom_rotations
)

# --------------------------
# 核心绘制函数（无残影）
# --------------------------
def draw_logo_screen():
    """绘制LOGO界面，居中显示LOGO"""
    tft.fill(BACKGROUND_COLOR)  # 清屏避免残影
    # 绘制LOGO文字（居中）
    tft.text(font8x8, LOGO_TEXT, LOGO_X, LOGO_Y, NORMAL_COLOR, BACKGROUND_COLOR)

def draw_mode_select_screen():
    """绘制模式选择界面"""
    tft.fill(BACKGROUND_COLOR)  # 清屏
    
    # 1. 绘制当前选中的模式文字（居中）
    current_mode = MODES[current_mode_idx]
    text_x = (SCREEN_WIDTH - len(current_mode)*CHAR_WIDTH) // 2
    tft.text(font8x8, current_mode, text_x, MODE_TEXT_Y, NORMAL_COLOR, BACKGROUND_COLOR)
    
    # 2. 绘制三个小圆点（选中的高亮，其余灰色）
    for i, dot_x in enumerate(DOT_X_LIST):
        # 绘制实心圆（先清区域再画）
        # 清除圆点区域
        tft.fill_rect(
            dot_x - DOT_RADIUS - 1, 
            DOT_Y - DOT_RADIUS - 1, 
            DOT_RADIUS*2 + 2, 
            DOT_RADIUS*2 + 2, 
            BACKGROUND_COLOR
        )
        # 绘制圆点
        draw_circle(dot_x + DOT_RADIUS, DOT_Y + DOT_RADIUS, DOT_RADIUS, 
                    HIGHLIGHT_COLOR if i == current_mode_idx else GRAY_COLOR, fill=True)

def draw_windspeed_screen():
    """绘制风速控制界面"""
    tft.fill(BACKGROUND_COLOR)  # 清屏
    
    # 1. 绘制windspeed文字
    text = "windspeed"
    text_x = (SCREEN_WIDTH - len(text)*CHAR_WIDTH) // 2
    tft.text(font8x8, text, text_x, SPEED_TEXT_Y, NORMAL_COLOR, BACKGROUND_COLOR)
    
    # 2. 绘制进度条背景（灰色竖条矩阵）
    for i in range(BAR_COUNT):
        bar_x = PROGRESS_BAR_X + i*(BAR_WIDTH + BAR_SPACING)
        # 绘制灰色背景竖条
        tft.fill_rect(
            bar_x, PROGRESS_BAR_Y, 
            BAR_WIDTH, PROGRESS_BAR_HEIGHT, 
            GRAY_COLOR
        )
    
    # 3. 绘制高亮进度条（根据当前风速）
    for i in range(fan_speed):
        bar_x = PROGRESS_BAR_X + i*(BAR_WIDTH + BAR_SPACING)
        tft.fill_rect(
            bar_x, PROGRESS_BAR_Y, 
            BAR_WIDTH, PROGRESS_BAR_HEIGHT, 
            HIGHLIGHT_COLOR
        )
    
    # 4. 绘制数值标签（0和100）
    tft.text(font8x8, "0", PROGRESS_MIN_X - 10, PROGRESS_BAR_Y + PROGRESS_BAR_HEIGHT + 2, NORMAL_COLOR, BACKGROUND_COLOR)
    tft.text(font8x8, "100", PROGRESS_MAX_X - 10, PROGRESS_BAR_Y + PROGRESS_BAR_HEIGHT + 2, NORMAL_COLOR, BACKGROUND_COLOR)
    
    # 5. 绘制当前百分比
    percent_text = f"{fan_speed}%"
    percent_text_x = PERCENT_X
    # 清除百分比区域避免残影
    tft.fill_rect(
        percent_text_x - 2, PERCENT_Y - 2, 
        len(percent_text)*CHAR_WIDTH + 4, CHAR_HEIGHT + 4, 
        BACKGROUND_COLOR
    )
    tft.text(font8x8, percent_text, percent_text_x, PERCENT_Y, HIGHLIGHT_COLOR, BACKGROUND_COLOR)

def draw_smokespeed_screen():
    """绘制发烟速度控制界面（逻辑同风速）"""
    tft.fill(BACKGROUND_COLOR)  # 清屏
    
    # 1. 绘制smokespeed文字
    text = "smokespeed"
    text_x = (SCREEN_WIDTH - len(text)*CHAR_WIDTH) // 2
    tft.text(font8x8, text, text_x, SPEED_TEXT_Y, NORMAL_COLOR, BACKGROUND_COLOR)
    
    # 2. 绘制进度条背景
    for i in range(BAR_COUNT):
        bar_x = PROGRESS_BAR_X + i*(BAR_WIDTH + BAR_SPACING)
        tft.fill_rect(
            bar_x, PROGRESS_BAR_Y, 
            BAR_WIDTH, PROGRESS_BAR_HEIGHT, 
            GRAY_COLOR
        )
    
    # 3. 绘制高亮进度条
    for i in range(smoke_speed):
        bar_x = PROGRESS_BAR_X + i*(BAR_WIDTH + BAR_SPACING)
        tft.fill_rect(
            bar_x, PROGRESS_BAR_Y, 
            BAR_WIDTH, PROGRESS_BAR_HEIGHT, 
            HIGHLIGHT_COLOR
        )
    
    # 4. 绘制数值标签
    tft.text(font8x8, "0", PROGRESS_MIN_X - 10, PROGRESS_BAR_Y + PROGRESS_BAR_HEIGHT + 2, NORMAL_COLOR, BACKGROUND_COLOR)
    tft.text(font8x8, "100", PROGRESS_MAX_X - 10, PROGRESS_BAR_Y + PROGRESS_BAR_HEIGHT + 2, NORMAL_COLOR, BACKGROUND_COLOR)
    
    # 5. 绘制当前百分比
    percent_text = f"{smoke_speed}%"
    percent_text_x = PERCENT_X
    tft.fill_rect(
        percent_text_x - 2, PERCENT_Y - 2, 
        len(percent_text)*CHAR_WIDTH + 4, CHAR_HEIGHT + 4, 
        BACKGROUND_COLOR
    )
    tft.text(font8x8, percent_text, percent_text_x, PERCENT_Y, HIGHLIGHT_COLOR, BACKGROUND_COLOR)

def draw_rgbcontrol_screen():
    """绘制RGB控制界面"""
    tft.fill(BACKGROUND_COLOR)  # 清屏
    
    # 1. 绘制rgbcontrol文字
    text = "rgbcontrol"
    text_x = (SCREEN_WIDTH - len(text)*CHAR_WIDTH) // 2
    tft.text(font8x8, text, text_x, RGB_TEXT_Y, NORMAL_COLOR, BACKGROUND_COLOR)
    
    # 2. 绘制三个通道的数值
    for i, (channel, y) in enumerate(zip(RGB_CHANNEL_NAMES, RGB_CHANNEL_Y_LIST)):
        # 通道名称
        tft.text(font8x8, channel, 10, y, 
                 HIGHLIGHT_COLOR if i == rgb_selected_channel else DARK_COLOR, 
                 BACKGROUND_COLOR)
        # 百分比数值
        percent_text = f"{rgb_values[i]}%"
        # 清除数值区域残影
        tft.fill_rect(
            RGB_PERCENT_X - 2, y - 2, 
            len(percent_text)*CHAR_WIDTH + 4, CHAR_HEIGHT + 4, 
            BACKGROUND_COLOR
        )
        # 绘制数值（选中通道高亮）
        tft.text(font8x8, percent_text, RGB_PERCENT_X, y, 
                 HIGHLIGHT_COLOR if i == rgb_selected_channel else DARK_COLOR, 
                 BACKGROUND_COLOR)

# --------------------------
# 辅助函数
# --------------------------
def draw_circle(x, y, r, color, fill=False):
    """绘制圆形（填充/空心）"""
    for dy in range(-r, r+1):
        dx = int((r*r - dy*dy)**0.5)
        if fill:
            tft.hline(x-dx, y+dy, 2*dx, color)
        else:
            tft.pixel(x-dx, y+dy, color)
            tft.pixel(x+dx, y+dy, color)

def update_hardware():
    """同步参数到硬件"""
    # 更新风扇速度
    set_fan_speed(fan_speed)
    # 更新发烟速度
    set_smoke_generator_speed(smoke_speed)
    # 更新RGB值（转换为0-100到实际硬件范围）
    r = int(rgb_values[0] * 2.55)  # 0-100 → 0-255
    g = int(rgb_values[1] * 2.55)
    b = int(rgb_values[2] * 2.55)
    set_cob1_rgb(r, g, b)

# --------------------------
# 界面切换处理函数
# --------------------------
def switch_screen(new_screen):
    """切换界面并绘制"""
    global current_screen
    current_screen = new_screen
    # 根据新界面绘制
    if current_screen == LOGO_SCREEN:
        draw_logo_screen()
    elif current_screen == MODE_SELECT_SCREEN:
        draw_mode_select_screen()
    elif current_screen == WINDSPEED_SCREEN:
        draw_windspeed_screen()
    elif current_screen == SMOKESPEED_SCREEN:
        draw_smokespeed_screen()
    elif current_screen == RGBCONTROL_SCREEN:
        draw_rgbcontrol_screen()

# --------------------------
# 主程序
# --------------------------
if __name__ == "__main__":
    print("76*284横屏多级界面程序启动...")
    
    # 初始化：停止所有外设
    stop_all_devices()
    
    # 第一步：显示LOGO界面2秒
    switch_screen(LOGO_SCREEN)
    sleep_ms(2000)  # 显示2秒
    
    # 自动切换到模式选择界面
    switch_screen(MODE_SELECT_SCREEN)
    
    try:
        while True:
            # 获取编码器旋转方向
            encoder_dir = get_encoder_dir()
            # 获取按键事件
            key_event = get_key_event()
            
            # --------------------------
            # LOGO界面（仅开机显示，无交互）
            # --------------------------
            if current_screen == LOGO_SCREEN:
                pass  # 已自动切换，无需处理
            
            # --------------------------
            # 模式选择界面交互
            # --------------------------
            elif current_screen == MODE_SELECT_SCREEN:
                # 旋转旋钮切换模式
                if encoder_dir != 0:
                    current_mode_idx = (current_mode_idx + encoder_dir) % len(MODES)
                    draw_mode_select_screen()  # 重绘界面
                
                # 单击进入对应三级界面
                if key_event == "click1":
                    last_mode_idx = current_mode_idx  # 记录当前选中模式
                    if current_mode_idx == 0:
                        switch_screen(WINDSPEED_SCREEN)
                    elif current_mode_idx == 1:
                        switch_screen(SMOKESPEED_SCREEN)
                    elif current_mode_idx == 2:
                        switch_screen(RGBCONTROL_SCREEN)
            
            # --------------------------
            # 风速控制界面交互
            # --------------------------
            elif current_screen == WINDSPEED_SCREEN:
                # 旋转旋钮调整风速
                if encoder_dir != 0:
                    fan_speed = max(0, min(100, fan_speed + encoder_dir))
                    draw_windspeed_screen()
                    update_hardware()  # 同步到硬件
                
                # 双击返回模式选择界面
                if key_event == "click2":
                    current_mode_idx = last_mode_idx  # 恢复上次选中模式
                    switch_screen(MODE_SELECT_SCREEN)
            
            # --------------------------
            # 发烟速度控制界面交互
            # --------------------------
            elif current_screen == SMOKESPEED_SCREEN:
                # 旋转旋钮调整发烟速度
                if encoder_dir != 0:
                    smoke_speed = max(0, min(100, smoke_speed + encoder_dir))
                    draw_smokespeed_screen()
                    update_hardware()  # 同步到硬件
                
                # 双击返回模式选择界面
                if key_event == "click2":
                    current_mode_idx = last_mode_idx
                    switch_screen(MODE_SELECT_SCREEN)
            
            # --------------------------
            # RGB控制界面交互
            # --------------------------
            elif current_screen == RGBCONTROL_SCREEN:
                if key_event == "click1":
                    # 单击切换编辑状态
                    rgb_editing = not rgb_editing
                    draw_rgbcontrol_screen()  # 重绘高亮状态
                
                if encoder_dir != 0:
                    if rgb_editing:
                        # 编辑状态：调整当前通道数值
                        rgb_values[rgb_selected_channel] = max(0, min(100, 
                                                    rgb_values[rgb_selected_channel] + encoder_dir))
                        update_hardware()  # 同步到硬件
                    else:
                        # 非编辑状态：切换通道
                        rgb_selected_channel = (rgb_selected_channel + encoder_dir) % 3
                    draw_rgbcontrol_screen()  # 重绘界面
                
                # 双击返回模式选择界面
                if key_event == "click2":
                    rgb_editing = False  # 退出编辑状态
                    current_mode_idx = last_mode_idx
                    switch_screen(MODE_SELECT_SCREEN)
            
            # 短暂延时降低CPU占用
            sleep_ms(10)
            
    except KeyboardInterrupt:
        # 程序中断清理
        stop_all_devices()
        tft.fill(BACKGROUND_COLOR)
        print("程序已停止")