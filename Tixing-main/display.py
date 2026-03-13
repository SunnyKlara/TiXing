# display.py - 界面绘制模块（f4 风格 UI 升级版）
# 模仿 f4 的 LCD_DrawRoundedBar / LCD_DrawGradientBar / LCD_DrawSolidBar 视觉风格
# 适配 76×284 横屏（rotation=1），纯代码绘制，无需图片资源

from st7789py import BLACK, GREEN, WHITE, color565
from lcdfont import font8x8

# --------------------------
# 显示相关常量
# --------------------------
# 颜色定义（参考 f4 的深色主题）
ACCENT = color565(0, 220, 120)       # 主强调色（科技绿）
ACCENT_DIM = color565(0, 120, 60)    # 暗强调色
WARN = color565(255, 180, 0)         # 警告色（油门模式）
NORMAL_COLOR = WHITE
GRAY = color565(60, 60, 60)          # 暗灰（未激活元素）
MID_GRAY = color565(100, 100, 100)   # 中灰
DARK = color565(20, 20, 20)          # 深色背景填充
BG = BLACK                           # 主背景
BACKGROUND_COLOR = BG                # 向后兼容别名（main.py 使用）

# 界面布局参数（76×284 横屏）
SW = 284   # Screen Width
SH = 76    # Screen Height
CW = font8x8.WIDTH    # 8
CH = font8x8.HEIGHT   # 8

# 12 种颜色预设（从 f4 的 streamlight_colors 数组移植）
COLOR_PRESETS = [
    {"name": "Cyber",   "cob1": (138, 43, 226), "cob2": (0, 255, 128)},
    {"name": "Ice",     "cob1": (0, 234, 255),  "cob2": (0, 234, 255)},
    {"name": "Sunset",  "cob1": (255, 100, 0),  "cob2": (0, 200, 255)},
    {"name": "Racing",  "cob1": (255, 210, 0),  "cob2": (255, 210, 0)},
    {"name": "Flame",   "cob1": (255, 0, 0),    "cob2": (255, 0, 0)},
    {"name": "Police",  "cob1": (255, 0, 0),    "cob2": (0, 80, 255)},
    {"name": "Sakura",  "cob1": (255, 105, 180), "cob2": (255, 0, 80)},
    {"name": "Aurora",  "cob1": (180, 0, 255),  "cob2": (0, 255, 200)},
    {"name": "Violet",  "cob1": (148, 0, 211),  "cob2": (148, 0, 211)},
    {"name": "Mint",    "cob1": (0, 255, 180),  "cob2": (100, 200, 255)},
    {"name": "Jungle",  "cob1": (0, 255, 65),   "cob2": (0, 255, 65)},
    {"name": "White",   "cob1": (225, 225, 225), "cob2": (225, 225, 225)},
]

MODES = ["windspeed", "smokespeed", "airpump", "preset", "rgbcontrol", "brightness"]
MODE_LABELS = ["Wind", "Smoke", "AirPump", "Preset", "RGB", "Bright"]


# --------------------------
# f4 风格绘图基元（纯代码实现，替代 f4 的 LCD_DrawRoundedBar 等）
# --------------------------
def _rgb565(r, g, b):
    """RGB888 转 RGB565"""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def draw_rounded_bar(tft, x, y, w, h, color):
    """绘制两端半圆的胶囊形进度条（参考 f4 LCD_DrawRoundedBar）
    x, y: 左上角坐标; w: 总宽度; h: 高度
    """
    if w <= h or h < 4:
        tft.fill_rect(x, y, w, h, color)
        return
    r = h // 2
    # 中间矩形
    if w - h > 0:
        tft.fill_rect(x + r, y, w - h, h, color)
    # 左右半圆（逐行水平线）
    for dy in range(-r, r + 1):
        dx = int((r * r - dy * dy) ** 0.5)
        # 左半圆
        tft.hline(x + r - dx, y + r + dy, dx, color)
        # 右半圆
        tft.hline(x + w - r, y + r + dy, dx, color)


def draw_gradient_bar(tft, x, y, w, h, r1, g1, b1, r2, g2, b2):
    """绘制圆角胶囊渐变色条（参考 f4 LCD_DrawGradientBar）
    从 (r1,g1,b1) 渐变到 (r2,g2,b2)，两端半圆。
    为节省内存，逐列绘制纯色竖线。
    """
    if w <= 0 or h < 4:
        return
    r = h // 2
    for col in range(w):
        # 线性插值颜色
        t = col / max(w - 1, 1)
        cr = int(r1 + (r2 - r1) * t)
        cg = int(g1 + (g2 - g1) * t)
        cb = int(b1 + (b2 - b1) * t)
        c = _rgb565(cr, cg, cb)
        # 计算该列的垂直范围（胶囊形裁剪）
        if col < r:
            dx = col
            max_dy = int((r * r - (r - dx) * (r - dx)) ** 0.5)
        elif col >= w - r:
            dx = w - 1 - col
            max_dy = int((r * r - (r - dx) * (r - dx)) ** 0.5)
        else:
            max_dy = r
        y_top = y + r - max_dy
        line_h = max_dy * 2 + 1
        if line_h > 0:
            tft.vline(x + col, y_top, line_h, c)


def draw_solid_bar(tft, x, y, w, h, r, g, b):
    """绘制圆角胶囊纯色条（参考 f4 LCD_DrawSolidBar）"""
    draw_rounded_bar(tft, x, y, w, h, _rgb565(r, g, b))


def draw_indicator_dot(tft, x, y, r, active):
    """绘制状态指示圆点（参考 f4 的 gImage_l_deng/gImage_h_deng）
    active=True: 绿色; active=False: 红色暗点
    """
    color = ACCENT if active else color565(180, 0, 0)
    for dy in range(-r, r + 1):
        dx = int((r * r - dy * dy) ** 0.5)
        tft.hline(x - dx, y + dy, 2 * dx + 1, color)


def draw_big_number(tft, value, cx, cy, color):
    """绘制大号数字（3x 放大，模拟 f4 的大数字图片显示）
    cx, cy: 数字区域中心坐标
    使用 font8x8 字体 3 倍放大绘制
    """
    text = str(value)
    scale = 3
    char_w = CW * scale  # 24
    char_h = CH * scale  # 24
    total_w = len(text) * char_w + (len(text) - 1) * 2  # 2px 间距
    sx = cx - total_w // 2
    sy = cy - char_h // 2
    for i, ch in enumerate(text):
        _draw_scaled_char(tft, ch, sx + i * (char_w + 2), sy, scale, color)


def _draw_scaled_char(tft, ch, x, y, scale, color):
    """用 font8x8 字体放大绘制单个字符（列优先点阵）"""
    idx = ord(ch) - 0x20
    if idx < 0 or idx >= 96:
        return
    offset = idx * CW
    for col in range(CW):
        byte_val = font8x8.FONT[offset + col]
        for row in range(CH):
            if byte_val & (1 << row):
                tft.fill_rect(x + col * scale, y + row * scale, scale, scale, color)


# --------------------------
# 界面绘制函数（f4 风格重构）
# --------------------------

def draw_logo_screen(tft, state):
    """开机 Logo 界面（参考 f4 Logo_PlayAnimation 几何风格）
    76×284 横屏布局：居中几何徽标 + 矢量品牌文字 + 装饰线
    纯代码绘制，无需图片资源，无像素字体
    """
    tft.fill(BG)

    # ── 布局参数（76×284 横屏居中） ──
    # 整体内容区约 220px 宽，水平居中
    base_x = 32  # 左边距

    # ── 1. 顶部/底部装饰细线 ──
    tft.hline(base_x, 6, 220, GRAY)
    tft.hline(base_x, SH - 7, 220, GRAY)

    # ── 2. 几何徽标：圆环 + 内部风扇图案 ──
    emblem_cx = base_x + 30   # 62
    emblem_cy = SH // 2       # 38
    emblem_r = 22

    # 双线圆环
    _draw_circle_ring(tft, emblem_cx, emblem_cy, emblem_r, ACCENT)
    _draw_circle_ring(tft, emblem_cx, emblem_cy, emblem_r - 1, ACCENT)

    # 内部风扇扇叶
    _draw_fan_blades(tft, emblem_cx, emblem_cy, 14, ACCENT)

    # 中心圆点
    for dy in range(-2, 3):
        dx = int((4 - dy * dy) ** 0.5)
        tft.hline(emblem_cx - dx, emblem_cy + dy, 2 * dx + 1, ACCENT)

    # ── 3. 连接线（徽标到文字） ──
    tft.hline(emblem_cx + emblem_r + 4, emblem_cy, 12, ACCENT_DIM)

    # ── 4. 矢量品牌文字 "TURBO"（用矩形/线条手绘，非像素字体） ──
    tx = emblem_cx + emblem_r + 20  # 文字起始 X ≈ 104
    ty = 14                          # 文字顶部 Y
    th = 22                          # 字母高度
    tw = 2                           # 笔画粗细
    sp = 4                           # 字间距
    _draw_vec_T(tft, tx, ty, th, tw, ACCENT)
    tx += 16 + sp
    _draw_vec_U(tft, tx, ty, th, tw, ACCENT)
    tx += 14 + sp
    _draw_vec_R(tft, tx, ty, th, tw, ACCENT)
    tx += 14 + sp
    _draw_vec_B(tft, tx, ty, th, tw, ACCENT)
    tx += 14 + sp
    _draw_vec_O(tft, tx, ty, th, tw, ACCENT)

    # ── 5. 版本号（小字体，文字下方居中） ──
    ver = "v1.1"
    vx = emblem_cx + emblem_r + 20 + 24  # 大致居中于 TURBO 下方
    vy = ty + th + 8
    tft.text(font8x8, ver, vx, vy, MID_GRAY, BG)

    # ── 6. 底部品牌副标题 ──
    sub = "RideWind"
    sx = (SW - len(sub) * CW) // 2
    tft.text(font8x8, sub, sx, SH - 18, color565(40, 40, 40), BG)


# ── 矢量字母绘制函数（用矩形构建，干净无锯齿感） ──

def _draw_vec_T(tft, x, y, h, w, c):
    """矢量字母 T：顶部横杠 + 中间竖杠"""
    tft.fill_rect(x, y, 16, w, c)              # 顶横
    tft.fill_rect(x + 7, y, w, h, c)           # 中竖

def _draw_vec_U(tft, x, y, h, w, c):
    """矢量字母 U：左竖 + 右竖 + 底横"""
    tft.fill_rect(x, y, w, h, c)               # 左竖
    tft.fill_rect(x + 12, y, w, h, c)          # 右竖
    tft.fill_rect(x, y + h - w, 14, w, c)      # 底横

def _draw_vec_R(tft, x, y, h, w, c):
    """矢量字母 R：左竖 + 上半圆角矩形 + 斜腿"""
    tft.fill_rect(x, y, w, h, c)               # 左竖
    tft.fill_rect(x, y, 12, w, c)              # 顶横
    tft.fill_rect(x + 10, y, w, h // 2, c)     # 右竖（上半）
    tft.fill_rect(x, y + h // 2 - 1, 12, w, c) # 中横
    # 斜腿：从中间到右下
    for i in range(h // 2 + 1):
        lx = x + 4 + (i * 10) // (h // 2)
        ly = y + h // 2 + i
        if ly < y + h:
            tft.fill_rect(lx, ly, w, 1, c)

def _draw_vec_B(tft, x, y, h, w, c):
    """矢量字母 B：左竖 + 上下两个凸起"""
    tft.fill_rect(x, y, w, h, c)               # 左竖
    tft.fill_rect(x, y, 11, w, c)              # 顶横
    tft.fill_rect(x + 9, y, w, h // 2, c)      # 右竖（上半）
    tft.fill_rect(x, y + h // 2 - 1, 12, w, c) # 中横
    tft.fill_rect(x + 10, y + h // 2, w, h // 2, c)  # 右竖（下半）
    tft.fill_rect(x, y + h - w, 12, w, c)      # 底横

def _draw_vec_O(tft, x, y, h, w, c):
    """矢量字母 O：四边矩形框"""
    tft.fill_rect(x, y, w, h, c)               # 左竖
    tft.fill_rect(x + 12, y, w, h, c)          # 右竖
    tft.fill_rect(x, y, 14, w, c)              # 顶横
    tft.fill_rect(x, y + h - w, 14, w, c)      # 底横


def _draw_circle_ring(tft, cx, cy, r, color):
    """绘制空心圆环（单像素宽）"""
    for dy in range(-r, r + 1):
        dx = int((r * r - dy * dy) ** 0.5)
        tft.pixel(cx - dx, cy + dy, color)
        tft.pixel(cx + dx, cy + dy, color)


def _draw_fan_blades(tft, cx, cy, r, color):
    """绘制三片风扇扇叶图案（120° 间隔，简化为径向弧线）"""
    import math
    for angle_deg in (90, 210, 330):
        rad = angle_deg * 3.14159 / 180
        for t in range(4, r + 1):
            sweep = (t - 4) * 0.15
            x = cx + int(t * math.cos(rad + sweep))
            y = cy - int(t * math.sin(rad + sweep))
            tft.fill_rect(x, y, 2, 2, color)




def _draw_icon_wind(tft, cx, cy, size, color):
    """风速图标：三条弧形风线"""
    s = size // 2
    for i in range(3):
        y = cy - s + i * s
        tft.hline(cx - s, y, s + s // 2, color)
        # 弧形尾部（短竖线模拟弯曲）
        tft.vline(cx + s // 2, y, 2, color)
        tft.hline(cx - s + 3, y + 1, s + s // 2 - 6, color)


def _draw_icon_smoke(tft, cx, cy, size, color):
    """发烟图标：底部方块 + 上方波浪烟雾"""
    s = size // 2
    # 底部发烟器方块
    bw = s + 4
    bh = s // 2
    tft.fill_rect(cx - bw // 2, cy + 2, bw, bh, color)
    # 上方烟雾波浪线（3 条）
    for i in range(3):
        y = cy - s + i * 4
        x = cx - 4 + (i % 2) * 3
        tft.hline(x, y, 8, color)
        tft.hline(x + 1, y + 1, 6, color)


def _draw_icon_pump(tft, cx, cy, size, color):
    """气泵图标：圆形泵体 + 出口管"""
    s = size // 2
    r = s - 2
    # 圆形泵体
    draw_indicator_dot(tft, cx, cy, r, True)
    # 用背景色挖出内圆（环形效果）
    if r > 4:
        for dy in range(-(r - 3), r - 2):
            dx = int(((r - 3) * (r - 3) - dy * dy) ** 0.5)
            tft.hline(cx - dx, cy + dy, 2 * dx + 1, BG)
    # 出口管
    tft.fill_rect(cx + r - 2, cy - 2, 5, 4, color)


def _draw_icon_preset(tft, cx, cy, size, color):
    """预设图标：调色板形状（圆 + 小色点）"""
    s = size // 2
    r = s - 1
    # 主圆
    for dy in range(-r, r + 1):
        dx = int((r * r - dy * dy) ** 0.5)
        tft.hline(cx - dx, cy + dy, 2 * dx + 1, color)
    # 内部小色点（模拟调色板上的颜色孔）
    dots = [(-3, -3, color565(255, 60, 60)), (3, -2, color565(60, 255, 60)),
            (-2, 3, color565(60, 100, 255)), (3, 3, BG)]
    for dx, dy, c in dots:
        tft.fill_rect(cx + dx - 1, cy + dy - 1, 3, 3, c)


def _draw_icon_rgb(tft, cx, cy, size, color):
    """RGB 图标：三个重叠的小色条"""
    s = size // 2
    bw = s + 6
    bh = 3
    # R 条
    tft.fill_rect(cx - bw // 2, cy - s, bw, bh, color565(255, 60, 60))
    # G 条
    tft.fill_rect(cx - bw // 2 + 2, cy - 1, bw, bh, color565(60, 255, 60))
    # B 条
    tft.fill_rect(cx - bw // 2 + 4, cy + s - bh, bw, bh, color565(60, 100, 255))
    # 竖线连接
    tft.vline(cx, cy - s, size, color)


def _draw_icon_bright(tft, cx, cy, size, color):
    """亮度图标：太阳形状（中心圆 + 放射线）"""
    s = size // 2
    r = s // 2
    # 中心实心圆
    for dy in range(-r, r + 1):
        dx = int((r * r - dy * dy) ** 0.5)
        tft.hline(cx - dx, cy + dy, 2 * dx + 1, color)
    # 放射线（上下左右 + 四角）
    rl = s - 1
    tft.vline(cx, cy - rl, r, color)       # 上
    tft.vline(cx, cy + r + 1, r, color)     # 下
    tft.hline(cx - rl, cy, r, color)        # 左
    tft.hline(cx + r + 1, cy, r, color)     # 右


# 图标绘制函数列表（按菜单顺序）
_ICON_DRAWERS = [
    _draw_icon_wind, _draw_icon_smoke, _draw_icon_pump,
    _draw_icon_preset, _draw_icon_rgb, _draw_icon_bright
]


def draw_mode_select_screen(tft, state):
    """菜单界面（参考 f4 LCD_Menu_Init + Draw_Menu_Page）
    横向 3 项可见：左侧小图标+灰色文字，中间大图标+高亮文字+值预览，右侧小图标+灰色文字。
    底部 6 个导航点。
    """
    tft.fill(BG)
    idx = state["current_mode_idx"]

    # --- 中间选中项：大图标 + 标签 ---
    icon_cx = SW // 2
    icon_cy = 18
    icon_size = 24
    _ICON_DRAWERS[idx](tft, icon_cx, icon_cy, icon_size, ACCENT)

    # 标签文字（图标下方，2x 放大）
    label = MODE_LABELS[idx]
    lx = (SW - len(label) * (CW * 2 + 1)) // 2
    ly = 34
    for i, ch in enumerate(label):
        _draw_scaled_char(tft, ch, lx + i * (CW * 2 + 1), ly, 2, ACCENT)

    # --- 左侧项（小图标 + 1x 文字） ---
    left_idx = (idx - 1) % 6
    left_cx = 36
    left_cy = 20
    _ICON_DRAWERS[left_idx](tft, left_cx, left_cy, 14, GRAY)
    left_label = MODE_LABELS[left_idx]
    llx = left_cx - len(left_label) * CW // 2
    tft.text(font8x8, left_label, max(2, llx), 34, GRAY, BG)

    # --- 右侧项（小图标 + 1x 文字） ---
    right_idx = (idx + 1) % 6
    right_cx = SW - 36
    right_cy = 20
    _ICON_DRAWERS[right_idx](tft, right_cx, right_cy, 14, GRAY)
    right_label = MODE_LABELS[right_idx]
    rlx = right_cx - len(right_label) * CW // 2
    tft.text(font8x8, right_label, min(SW - len(right_label) * CW - 2, rlx), 34, GRAY, BG)

    # --- 分隔线（中间区域与两侧的视觉分隔） ---
    tft.vline(80, 4, 44, DARK)
    tft.vline(SW - 80, 4, 44, DARK)

    # --- 底部导航点（6 个，参考 f4 菜单页面指示） ---
    dot_r = 2
    dot_y = SH - 12
    total_dot_w = 6 * (dot_r * 2 + 1) + 5 * 10
    dot_sx = (SW - total_dot_w) // 2
    for i in range(6):
        dx = dot_sx + i * (dot_r * 2 + 1 + 10) + dot_r
        draw_indicator_dot(tft, dx, dot_y, dot_r, i == idx)

    # --- 选中项值预览（Requirements: 1.3） ---
    _draw_menu_value_preview(tft, idx, state)

    # --- 操作提示（Requirements: 6） ---
    tft.text(font8x8, "Click:Enter", SW - 11 * CW - 4, SH - 10, GRAY, BG)


def _draw_menu_value_preview(tft, idx, state):
    """在菜单选中项下方显示当前值预览（Requirements: 1.3）"""
    values = [
        "{}%".format(state["fan_speed"]),
        "{}%".format(state["smoke_speed"]),
        "{}%".format(state.get("air_pump_speed", 0)),
        COLOR_PRESETS[state.get("color_preset_idx", 0)]["name"],
        "COB{}".format(state.get("rgb_strip", 0) + 1),
        "{}%".format(state.get("brightness", 100)),
    ]
    text = values[idx]
    tx = (SW - len(text) * CW) // 2
    tft.text(font8x8, text, tx, 52, ACCENT, BG)
def _draw_hint(tft, text="2x:Back"):
    """在界面底部显示操作提示"""
    tx = SW - len(text) * CW - 4
    tft.text(font8x8, text, tx, SH - 10, GRAY, BG)




def _draw_speed_screen(tft, state, title, value, throttle=False):
    """通用速度/百分比控制界面（参考 f4 LCD_ui1 + LCD_picture 大数字风格）
    布局：左侧大数字 + 右侧标题文字 + 底部圆角进度条
    """
    tft.fill(BG)

    # --- 顶部标题（右上角小字，参考 f4 的 fengshubiao 标签图片位置） ---
    tx = SW - len(title) * CW - 8
    tft.text(font8x8, title, tx, 4, MID_GRAY, BG)

    # 油门模式指示（参考 f4 的 wuhuaqi 橙色指示灯）
    if throttle:
        tft.text(font8x8, "THR", tx - 32, 4, WARN, BG)

    # --- 中央大数字（参考 f4 的 LCD_picture 大数字显示） ---
    draw_big_number(tft, value, SW // 2, 28, WHITE)

    # --- 百分号（紧跟数字右侧） ---
    text = str(value)
    scale = 3
    char_w = CW * scale
    total_w = len(text) * char_w + (len(text) - 1) * 2
    pct_x = SW // 2 + total_w // 2 + 4
    tft.text(font8x8, "%", pct_x, 22, MID_GRAY, BG)

    # --- 底部圆角进度条（参考 f4 LCD_DrawRoundedBar） ---
    bar_x = 20
    bar_y = SH - 20
    bar_w = SW - 40
    bar_h = 10
    # 背景条（暗灰胶囊）
    draw_rounded_bar(tft, bar_x, bar_y, bar_w, bar_h, GRAY)
    # 前景条（绿色胶囊，按比例）
    fg_w = max(bar_h + 1, int(bar_w * value / 100)) if value > 0 else 0
    if fg_w > 0:
        bar_color = WARN if throttle else ACCENT
        draw_rounded_bar(tft, bar_x, bar_y, fg_w, bar_h, bar_color)

    _draw_hint(tft)


def draw_windspeed_screen(tft, state):
    """风速控制界面"""
    _draw_speed_screen(tft, state, "WIND",
                       state["fan_speed"],
                       state.get("throttle_mode", False))

# --------------------------
# 局部刷新辅助函数（速度类界面共用）
# --------------------------
def _update_speed_number(tft, value, old_value):
    """局部刷新大数字区域：先用背景色矩形清除旧数字，再绘制新数字和百分号。
    数字区域中心: (SW//2, 28), 3x 放大, 最多 3 位数 + '%'
    """
    scale = 3
    char_w = CW * scale   # 24
    char_h = CH * scale   # 24
    # 清除区域：覆盖最大可能宽度（"100%" = 3 digits + gaps + %)
    # 最大数字宽度: 3 * 24 + 2 * 2 = 76, 加上 % 号约 12px, 总共约 96px
    clear_w = 100
    clear_x = SW // 2 - clear_w // 2
    clear_y = 28 - char_h // 2
    tft.fill_rect(clear_x, clear_y, clear_w, char_h, BG)
    # 绘制新数字
    draw_big_number(tft, value, SW // 2, 28, WHITE)
    # 绘制百分号
    text = str(value)
    total_w = len(text) * char_w + (len(text) - 1) * 2
    pct_x = SW // 2 + total_w // 2 + 4
    tft.text(font8x8, "%", pct_x, 22, MID_GRAY, BG)


def _update_speed_bar(tft, value, throttle):
    """局部刷新进度条区域：重绘整个进度条（背景 + 前景），避免残影。
    进度条位置: (20, SH-20, SW-40, 10)
    """
    bar_x = 20
    bar_y = SH - 20
    bar_w = SW - 40
    bar_h = 10
    # 先绘制完整背景条（灰色胶囊）
    draw_rounded_bar(tft, bar_x, bar_y, bar_w, bar_h, GRAY)
    # 再绘制前景条（按比例）
    fg_w = max(bar_h + 1, int(bar_w * value / 100)) if value > 0 else 0
    if fg_w > 0:
        bar_color = WARN if throttle else ACCENT
        draw_rounded_bar(tft, bar_x, bar_y, fg_w, bar_h, bar_color)


def update_windspeed_screen(tft, state, old_state):
    """风速界面局部刷新：仅更新变化的数字和进度条区域，避免全屏重绘闪烁。
    Requirements: 2.3, 7.2, 7.4
    """
    fan_speed = state["fan_speed"]
    old_fan_speed = old_state.get("fan_speed")
    throttle = state.get("throttle_mode", False)
    old_throttle = old_state.get("throttle_mode", False)

    speed_changed = fan_speed != old_fan_speed
    throttle_changed = throttle != old_throttle

    if speed_changed:
        _update_speed_number(tft, fan_speed, old_fan_speed)
    if speed_changed or throttle_changed:
        _update_speed_bar(tft, fan_speed, throttle)

def update_smokespeed_screen(tft, state, old_state):
    """发烟速度界面局部刷新：仅更新变化的数字和进度条区域。
    发烟无油门模式，throttle 固定为 False。
    Requirements: 2.3, 7.2
    """
    smoke_speed = state["smoke_speed"]
    old_smoke_speed = old_state.get("smoke_speed")
    if smoke_speed != old_smoke_speed:
        _update_speed_number(tft, smoke_speed, old_smoke_speed)
        _update_speed_bar(tft, smoke_speed, False)


def update_airpump_screen(tft, state, old_state):
    """气泵界面局部刷新：仅更新变化的数字和进度条区域。
    气泵无油门模式，throttle 固定为 False。
    Requirements: 2.3, 7.2
    """
    air_pump_speed = state.get("air_pump_speed", 0)
    old_air_pump_speed = old_state.get("air_pump_speed", 0)
    if air_pump_speed != old_air_pump_speed:
        _update_speed_number(tft, air_pump_speed, old_air_pump_speed)
        _update_speed_bar(tft, air_pump_speed, False)

def update_brightness_screen(tft, state, old_state):
    """亮度界面局部刷新：仅更新变化的数字、进度条和呼吸灯状态指示。
    Requirements: 5.4, 7.2
    """
    brightness = state.get("brightness", 100)
    old_brightness = old_state.get("brightness", 100)
    breath = state.get("breath_mode", False)
    old_breath = old_state.get("breath_mode", False)

    if brightness != old_brightness:
        _update_speed_number(tft, brightness, old_brightness)
        _update_speed_bar(tft, brightness, False)

    if breath != old_breath:
        # 重绘指示点
        draw_indicator_dot(tft, SW - 44, 7, 3, breath)
        # 清除旧文字区域（"Breath:OFF" = 10 chars * 8px = 80px）
        tft.fill_rect(8, 4, 80, CH, BG)
        # 绘制新文字
        btext = "Breath:ON" if breath else "Breath:OFF"
        bcolor = ACCENT if breath else color565(180, 0, 0)
        tft.text(font8x8, btext, 8, 4, bcolor, BG)


def update_preset_screen(tft, state, old_state):
    """预设界面局部刷新：仅更新变化的预设编号、名称、色条和渐变指示。
    Requirements: 7.2
    """
    idx = state.get("color_preset_idx", 0)
    old_idx = old_state.get("color_preset_idx", 0)
    grad = state.get("gradient_mode", False)
    old_grad = old_state.get("gradient_mode", False)

    if idx != old_idx:
        preset = COLOR_PRESETS[idx]
        c1 = preset["cob1"]
        c2 = preset["cob2"]

        # 清除并重绘预设编号（左上，最多 "12/12" = 5 chars = 40px）
        tft.fill_rect(8, 4, 40, CH, BG)
        num_text = "{}/{}".format(idx + 1, len(COLOR_PRESETS))
        tft.text(font8x8, num_text, 8, 4, MID_GRAY, BG)

        # 清除并重绘预设名称（顶部居中，最大 8 chars = 64px）
        name_area_x = (SW - 64) // 2
        tft.fill_rect(name_area_x, 4, 64, CH, BG)
        name = preset["name"]
        nx = (SW - len(name) * CW) // 2
        tft.text(font8x8, name, nx, 4, WHITE, BG)

        # 重绘色条预览
        bar_x = 20
        bar_y = 20
        bar_w = SW - 40
        bar_h = 20
        tft.fill_rect(bar_x, bar_y, bar_w, bar_h, BG)
        if c1 == c2:
            draw_solid_bar(tft, bar_x, bar_y, bar_w, bar_h, c1[0], c1[1], c1[2])
        else:
            draw_gradient_bar(tft, bar_x, bar_y, bar_w, bar_h,
                              c1[0], c1[1], c1[2], c2[0], c2[1], c2[2])

        # 重绘 COB1/COB2 颜色小方块
        preview_y = 50
        preview_h = 14
        preview_w = 60
        gap = 20
        c1_x = (SW - preview_w * 2 - gap) // 2
        tft.fill_rect(c1_x, preview_y, preview_w, preview_h, BG)
        draw_solid_bar(tft, c1_x, preview_y, preview_w, preview_h, c1[0], c1[1], c1[2])
        c2_x = c1_x + preview_w + gap
        tft.fill_rect(c2_x, preview_y, preview_w, preview_h, BG)
        draw_solid_bar(tft, c2_x, preview_y, preview_w, preview_h, c2[0], c2[1], c2[2])

    if grad != old_grad:
        # 重绘渐变模式指示点
        draw_indicator_dot(tft, SW - 16, 7, 3, grad)
        # 清除并重绘 "GRD"/"FIX" 标签（3 chars = 24px）
        tft.fill_rect(SW - 40, 4, 24, CH, BG)
        grad_label = "GRD" if grad else "FIX"
        tft.text(font8x8, grad_label, SW - 40, 4, ACCENT if grad else GRAY, BG)

def update_rgbcontrol_screen(tft, state, old_state):
    """RGB 调色界面局部刷新：仅更新变化的模式指示、通道高亮、数值和颜色预览。
    Requirements: 7.2
    """
    rgb_mode = state.get("rgb_mode", 0)
    rgb_strip = state.get("rgb_strip", 0)
    rgb_channel = state.get("rgb_channel", 0)
    cob1 = state.get("cob1_rgb", [0, 0, 0])
    cob2 = state.get("cob2_rgb", [0, 0, 0])

    old_mode = old_state.get("rgb_mode", 0)
    old_strip = old_state.get("rgb_strip", 0)
    old_channel = old_state.get("rgb_channel", 0)
    old_cob1 = old_state.get("cob1_rgb", [0, 0, 0])
    old_cob2 = old_state.get("cob2_rgb", [0, 0, 0])

    vals = cob1 if rgb_strip == 0 else cob2

    mode_changed = rgb_mode != old_mode
    strip_changed = rgb_strip != old_strip
    channel_changed = rgb_channel != old_channel
    vals_changed = cob1 != old_cob1 or cob2 != old_cob2

    # --- 模式指示文字 ---
    if mode_changed:
        mode_labels = ["Strip>", "Chan>", "Val>"]
        # 清除旧标签区域（最长 "Strip>" = 6 chars = 48px）
        tft.fill_rect(4, 2, 48, CH, BG)
        tft.text(font8x8, mode_labels[rgb_mode], 4, 2, MID_GRAY, BG)

    # --- 灯带名称（模式或灯带变化时重绘） ---
    if mode_changed or strip_changed:
        strip_name = "COB1" if rgb_strip == 0 else "COB2"
        strip_color = ACCENT if rgb_mode == 0 else WHITE
        tft.fill_rect(56, 2, 32, CH, BG)
        tft.text(font8x8, strip_name, 56, 2, strip_color, BG)

    # --- 通道行 + 颜色预览（灯带/通道/数值任一变化时全部重绘） ---
    if strip_changed or channel_changed or vals_changed or mode_changed:
        ch_names = ["R", "G", "B"]
        ch_colors_active = [color565(255, 60, 60), color565(60, 255, 60), color565(60, 100, 255)]
        ch_colors_dim = [color565(100, 20, 20), color565(20, 100, 20), color565(20, 40, 100)]
        bar_x = 28
        bar_w = 180
        bar_h = 8

        for i in range(3):
            y = 16 + i * 20
            is_active = (rgb_mode == 1 or rgb_mode == 2) and i == rgb_channel
            ch_color = ch_colors_active[i] if is_active else ch_colors_dim[i]
            label_color = ACCENT if is_active else GRAY

            # 通道名
            tft.fill_rect(8, y + 1, CW, CH, BG)
            tft.text(font8x8, ch_names[i], 8, y + 1, label_color, BG)

            # 背景条
            draw_rounded_bar(tft, bar_x, y, bar_w, bar_h, DARK)

            # 前景条
            fg_w = max(bar_h + 1, int(bar_w * vals[i] / 255)) if vals[i] > 0 else 0
            if fg_w > 0:
                draw_rounded_bar(tft, bar_x, y, fg_w, bar_h, ch_color)

            # 数值
            val_text = str(vals[i])
            vx = bar_x + bar_w + 8
            tft.fill_rect(vx, y, 32, CH + 2, BG)
            tft.text(font8x8, val_text, vx, y + 1, label_color, BG)

        # 颜色预览小条
        preview_x = SW - 60
        draw_solid_bar(tft, preview_x, 2, 52, 8, vals[0], vals[1], vals[2])



def draw_smokespeed_screen(tft, state):
    """发烟速度控制界面"""
    _draw_speed_screen(tft, state, "SMOKE", state["smoke_speed"])


def draw_airpump_screen(tft, state):
    """气泵控制界面"""
    _draw_speed_screen(tft, state, "PUMP", state.get("air_pump_speed", 0))


def draw_brightness_screen(tft, state):
    """亮度调节界面（参考 f4 LCD_ui4 + 呼吸灯指示）"""
    brightness = state.get("brightness", 100)
    breath = state.get("breath_mode", False)

    tft.fill(BG)

    # 标题
    tft.text(font8x8, "BRT", SW - 32, 4, MID_GRAY, BG)

    # 呼吸灯状态指示点（参考 f4 的 lcd_ui4_deng 红/绿点）
    draw_indicator_dot(tft, SW - 44, 7, 3, breath)

    # 大数字
    draw_big_number(tft, brightness, SW // 2, 28, WHITE)
    text = str(brightness)
    scale = 3
    char_w = CW * scale
    total_w = len(text) * char_w + (len(text) - 1) * 2
    pct_x = SW // 2 + total_w // 2 + 4
    tft.text(font8x8, "%", pct_x, 22, MID_GRAY, BG)

    # 底部进度条
    bar_x = 20
    bar_y = SH - 20
    bar_w = SW - 40
    bar_h = 10
    draw_rounded_bar(tft, bar_x, bar_y, bar_w, bar_h, GRAY)
    fg_w = max(bar_h + 1, int(bar_w * brightness / 100)) if brightness > 0 else 0
    if fg_w > 0:
        draw_rounded_bar(tft, bar_x, bar_y, fg_w, bar_h, ACCENT)

    # 呼吸灯模式文字
    btext = "Breath:ON" if breath else "Breath:OFF"
    bcolor = ACCENT if breath else color565(180, 0, 0)
    tft.text(font8x8, btext, 8, 4, bcolor, BG)

    _draw_hint(tft)


def draw_preset_screen(tft, state):
    """灯光预设界面（参考 f4 LCD_ui2 + lcd_pei_se 渐变/纯色胶囊条）
    布局：顶部预设编号 + 名称，中间渐变色条预览，底部流水灯指示
    """
    tft.fill(BG)

    idx = state.get("color_preset_idx", 0)
    grad = state.get("gradient_mode", False)
    preset = COLOR_PRESETS[idx]

    # --- 顶部：预设编号（左）+ 名称（中）+ 渐变指示（右） ---
    num_text = "{}/{}".format(idx + 1, len(COLOR_PRESETS))
    tft.text(font8x8, num_text, 8, 4, MID_GRAY, BG)

    name = preset["name"]
    nx = (SW - len(name) * CW) // 2
    tft.text(font8x8, name, nx, 4, WHITE, BG)

    # 渐变模式指示点（参考 f4 的 lcd_pei_se_dot）
    draw_indicator_dot(tft, SW - 16, 7, 3, grad)
    grad_label = "GRD" if grad else "FIX"
    tft.text(font8x8, grad_label, SW - 40, 4, ACCENT if grad else GRAY, BG)

    # --- 中间：颜色预览胶囊条（参考 f4 lcd_pei_se 的渐变/纯色条） ---
    bar_x = 20
    bar_y = 20
    bar_w = SW - 40
    bar_h = 20
    c1 = preset["cob1"]
    c2 = preset["cob2"]
    if c1 == c2:
        # 纯色条（参考 f4 LCD_DrawSolidBar）
        draw_solid_bar(tft, bar_x, bar_y, bar_w, bar_h, c1[0], c1[1], c1[2])
    else:
        # 渐变条（参考 f4 LCD_DrawGradientBar）
        draw_gradient_bar(tft, bar_x, bar_y, bar_w, bar_h,
                          c1[0], c1[1], c1[2], c2[0], c2[1], c2[2])

    # --- 底部：COB1 / COB2 颜色小方块预览 ---
    preview_y = 50
    preview_h = 14
    preview_w = 60
    gap = 20
    # COB1
    c1_x = (SW - preview_w * 2 - gap) // 2
    tft.text(font8x8, "C1", c1_x - 20, preview_y + 3, GRAY, BG)
    draw_solid_bar(tft, c1_x, preview_y, preview_w, preview_h, c1[0], c1[1], c1[2])
    # COB2
    c2_x = c1_x + preview_w + gap
    draw_solid_bar(tft, c2_x, preview_y, preview_w, preview_h, c2[0], c2[1], c2[2])
    tft.text(font8x8, "C2", c2_x + preview_w + 4, preview_y + 3, GRAY, BG)

    _draw_hint(tft)


def draw_rgbcontrol_screen(tft, state):
    """RGB 控制界面（参考 f4 LCD_ui3 三层状态机 + 颜色条实时预览）
    布局：顶部灯带选择 + 三行 R/G/B 通道条 + 数值
    """
    tft.fill(BG)

    rgb_mode = state.get("rgb_mode", 0)
    rgb_strip = state.get("rgb_strip", 0)
    rgb_channel = state.get("rgb_channel", 0)
    cob1 = state.get("cob1_rgb", [0, 0, 0])
    cob2 = state.get("cob2_rgb", [0, 0, 0])

    strip_name = "COB1" if rgb_strip == 0 else "COB2"
    vals = cob1 if rgb_strip == 0 else cob2

    # --- 顶部：模式指示 + 灯带名称 ---
    mode_labels = ["Strip>", "Chan>", "Val>"]
    tft.text(font8x8, mode_labels[rgb_mode], 4, 2, MID_GRAY, BG)
    strip_color = ACCENT if rgb_mode == 0 else WHITE
    tft.text(font8x8, strip_name, 56, 2, strip_color, BG)

    # 顶部颜色预览小条（实时显示当前灯带颜色）
    preview_x = SW - 60
    draw_solid_bar(tft, preview_x, 2, 52, 8, vals[0], vals[1], vals[2])

    # --- 三行 RGB 通道（参考 f4 LCD_ui3 的 R/G/B 条 + 数值） ---
    ch_names = ["R", "G", "B"]
    ch_colors_active = [color565(255, 60, 60), color565(60, 255, 60), color565(60, 100, 255)]
    ch_colors_dim = [color565(100, 20, 20), color565(20, 100, 20), color565(20, 40, 100)]
    bar_x = 28
    bar_w = 180
    bar_h = 8

    for i in range(3):
        y = 16 + i * 20
        # 通道是否被选中
        is_active = (rgb_mode == 1 and i == rgb_channel) or (rgb_mode == 2 and i == rgb_channel)
        ch_color = ch_colors_active[i] if is_active else ch_colors_dim[i]
        label_color = ACCENT if is_active else GRAY

        # 通道名
        tft.text(font8x8, ch_names[i], 8, y + 1, label_color, BG)

        # 背景条
        draw_rounded_bar(tft, bar_x, y, bar_w, bar_h, DARK)

        # 前景条（按 0-255 比例）
        fg_w = max(bar_h + 1, int(bar_w * vals[i] / 255)) if vals[i] > 0 else 0
        if fg_w > 0:
            draw_rounded_bar(tft, bar_x, y, fg_w, bar_h, ch_color)

        # 数值（右侧）
        val_text = str(vals[i])
        vx = bar_x + bar_w + 8
        tft.text(font8x8, val_text, vx, y + 1, label_color, BG)
        # 清除旧数值残留
        tft.fill_rect(vx + len(val_text) * CW, y, 24, CH + 2, BG)

    _draw_hint(tft)

