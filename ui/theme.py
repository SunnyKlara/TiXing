# ui/theme.py — 主题常量：颜色 / 布局 / 预设
# 所有颜色为 RGB565 格式，布局参数为模块级常量避免运行时计算


def rgb888_to_565(r, g, b):
    """将 RGB888 (各 0-255) 转换为 RGB565 (16-bit)"""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


# ── 屏幕尺寸 ──
SCREEN_W = 284
SCREEN_H = 76

# ── 三区布局参数 ──
TOP_Y = 0
TOP_H = 12
MAIN_Y = 12
MAIN_H = 44
BOT_Y = 56
BOT_H = 20

# ── 常用 RGB565 颜色 ──
BLACK     = 0x0000
WHITE     = 0xFFFF
RED       = 0xF800
GREEN     = 0x07E0
BLUE      = 0x001F
YELLOW    = 0xFFE0
ORANGE    = 0xFD20
CYAN      = 0x07FF
MAGENTA   = 0xF81F
GRAY      = 0x7BEF
DARK_GRAY = 0x39E7
ACCENT    = 0x06BF  # 蓝青强调色
ACCENT_END = 0x64BF  # 渐变终止色 (100, 150, 255) → Logo 蓝
WARNING   = ORANGE  # 警告色（油门模式等）

# ── 12 种颜色预设 (name, cob1_rgb888, cob2_rgb888) ──
PRESETS = [
    ("Cyber",   (138, 43, 226), (0, 255, 128)),
    ("Ice",     (0, 234, 255),  (0, 234, 255)),
    ("Sunset",  (255, 100, 0),  (0, 200, 255)),
    ("Racing",  (255, 210, 0),  (255, 210, 0)),
    ("Flame",   (255, 0, 0),    (255, 0, 0)),
    ("Police",  (255, 0, 0),    (0, 80, 255)),
    ("Sakura",  (255, 105, 180),(255, 0, 80)),
    ("Aurora",  (180, 0, 255),  (0, 255, 200)),
    ("Violet",  (148, 0, 211),  (148, 0, 211)),
    ("Mint",    (0, 255, 180),  (100, 200, 255)),
    ("Jungle",  (0, 255, 65),   (0, 255, 65)),
    ("White",   (225, 225, 225),(225, 225, 225)),
]
