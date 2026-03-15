def clamp(value, lo, hi):
    if value < lo:
        return lo
    if value > hi:
        return hi
    return value


class AppState:
    __slots__ = (
        'fan_speed', 'smoke_speed', 'pump_speed',
        'cob1_rgb', 'cob2_rgb',
        'brightness', 'breath_mode',
        'preset_idx', 'gradient_mode',
        'throttle_mode',
        'rgb_mode', 'rgb_strip', 'rgb_channel',
        'menu_idx',
    )

    def __init__(self):
        self.fan_speed = 0        # 0-100
        self.smoke_speed = 0      # 0-100
        self.pump_speed = 0       # 0-100
        self.cob1_rgb = [0, 0, 0] # each 0-255
        self.cob2_rgb = [0, 0, 0] # each 0-255
        self.brightness = 100     # 0-100
        self.breath_mode = False
        self.preset_idx = 0       # 0-11
        self.gradient_mode = False
        self.throttle_mode = False
        self.rgb_mode = 0         # 0-2
        self.rgb_strip = 0        # 0-1
        self.rgb_channel = 0      # 0-2
        self.menu_idx = 0         # 0-4
