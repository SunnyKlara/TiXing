# key.py - EC10滚轮编码器控制模块
# GPIO23：按键（低电平有效） | GPIO24：A相 | GPIO25：B相
from machine import Pin, Timer
import utime

# --------------------------
# 硬件参数配置
# --------------------------
# 编码器引脚定义
EC10_KEY_PIN = 19    # 按键脚（低电平有效）
EC10_A_PIN = 20      # A相脚
EC10_B_PIN = 21      # B相脚

# 按键识别参数（可根据需求调整）
KEY_DEBOUNCE_TIME = 20    # 按键消抖时间(ms)
LONG_PRESS_TIME = 1000    # 长按判定时间(ms)
MULTI_CLICK_INTERVAL = 500# 多击间隔时间(ms)

# --------------------------
# 全局变量（用于状态记录）
# --------------------------
# 滚轮状态
_encoder_count = 0        # 滚轮计数值（正转+1，反转-1）
_encoder_last_a = 1       # A相上一次电平

# 按键状态
_key_press_time = 0       # 按键按下时间戳
_key_release_time = 0     # 按键释放时间戳
_key_click_count = 0      # 按键点击次数
_key_long_press_flag = False # 长按标志
_key_event = None         # 按键事件缓存（None/click1/click2/click3/long_press）

# --------------------------
# 硬件初始化
# --------------------------
# 初始化按键引脚（上拉输入，低电平有效）
key_pin = Pin(EC10_KEY_PIN, Pin.IN, Pin.PULL_UP)

# 初始化编码器A/B相引脚（上拉输入）
encoder_a = Pin(EC10_A_PIN, Pin.IN, Pin.PULL_UP)
encoder_b = Pin(EC10_B_PIN, Pin.IN, Pin.PULL_UP)

# --------------------------
# 滚轮正反转识别（中断回调）
# --------------------------
def _encoder_irq_handler(pin):
    """A相电平变化中断回调，识别滚轮方向"""
    global _encoder_count, _encoder_last_a
    utime.sleep_ms(1)  # 简单消抖（避免机械抖动）
    a_val = encoder_a.value()
    b_val = encoder_b.value()
    
    # 仅处理A相上升沿/下降沿（避免重复触发）
    if a_val != _encoder_last_a:
        _encoder_last_a = a_val
        # 正转：A相先变，B相为1；反转：A相先变，B相为0
        if a_val == 1:  # A相上升沿
            if b_val == 0:
                _encoder_count += 1  # 正转
            else:
                _encoder_count -= 1  # 反转
        else:  # A相下降沿
            if b_val == 1:
                _encoder_count += 1  # 正转
            else:
                _encoder_count -= 1  # 反转

# 注册A相中断（上升沿+下降沿触发）
encoder_a.irq(trigger=Pin.IRQ_RISING | Pin.IRQ_FALLING, handler=_encoder_irq_handler)

# --------------------------
# 按键识别（定时器消抖+状态判断）
# --------------------------
def _key_irq_handler(pin):
    """按键电平变化中断回调，触发消抖定时器"""
    # 启动消抖定时器（20ms后执行实际判断）
    key_timer = Timer(-1)
    key_timer.init(period=KEY_DEBOUNCE_TIME, mode=Timer.ONE_SHOT, callback=_key_debounce_handler)

def _key_debounce_handler(timer):
    """按键消抖后的实际状态处理"""
    global _key_press_time, _key_release_time, _key_click_count, _key_long_press_flag
    
    key_val = key_pin.value()
    current_time = utime.ticks_ms()
    
    # 按键按下（低电平）
    if key_val == 0 and _key_press_time == 0:
        _key_press_time = current_time
        _key_long_press_flag = False
        # 启动长按检测定时器
        long_press_timer = Timer(-1)
        long_press_timer.init(period=LONG_PRESS_TIME, mode=Timer.ONE_SHOT, callback=_key_long_press_handler)
    
    # 按键释放（高电平）
    elif key_val == 1 and _key_press_time != 0:
        _key_release_time = current_time
        press_duration = utime.ticks_diff(_key_release_time, _key_press_time)
        
        # 排除长按（长按已单独处理）
        if not _key_long_press_flag:
            _key_click_count += 1
            # 启动多击判断定时器（500ms内无再次点击则判定最终点击次数）
            multi_click_timer = Timer(-1)
            multi_click_timer.init(period=MULTI_CLICK_INTERVAL, mode=Timer.ONE_SHOT, callback=_key_multi_click_handler)
        
        # 重置按下时间
        _key_press_time = 0

def _key_long_press_handler(timer):
    """长按判定回调"""
    global _key_long_press_flag, _key_event
    # 若按键仍按下，判定为长按
    if key_pin.value() == 0:
        _key_long_press_flag = True
        _key_event = "long_press"
        _key_click_count = 0  # 重置点击次数

def _key_multi_click_handler(timer):
    """多击判定回调（单击/双击/三击）"""
    global _key_event, _key_click_count
    if _key_click_count == 1:
        _key_event = "click1"
    elif _key_click_count == 2:
        _key_event = "click2"
    elif _key_click_count >= 3:
        _key_event = "click3"
    # 重置点击次数
    _key_click_count = 0

# 注册按键中断（上升沿+下降沿触发）
key_pin.irq(trigger=Pin.IRQ_RISING | Pin.IRQ_FALLING, handler=_key_irq_handler)

# --------------------------
# 对外提供的调用函数（main.py使用）
# --------------------------
def get_encoder_dir():
    """
    获取滚轮方向（单次调用仅返回一次变化）
    :return: 1=正转，-1=反转，0=无变化
    """
    global _encoder_count
    if _encoder_count > 0:
        _encoder_count -= 1
        return 1
    elif _encoder_count < 0:
        _encoder_count += 1
        return -1
    else:
        return 0

def get_key_event():
    """
    获取按键事件（单次调用仅返回一次事件，避免重复触发）
    :return: "click1"/"click2"/"click3"/"long_press"/None
    """
    global _key_event
    if _key_event is not None:
        event = _key_event
        _key_event = None  # 重置事件，避免重复读取
        return event
    return None

def clear_encoder_count():
    """重置滚轮计数值"""
    global _encoder_count
    _encoder_count = 0

def clear_key_event():
    """清空按键事件缓存"""
    global _key_event
    _key_event = None