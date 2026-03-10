# ui/widgets.py — 可复用 UI 控件库
# 所有控件接受 Renderer 实例，不直接操作硬件

from ui.theme import (
    SCREEN_W, TOP_Y, TOP_H, MAIN_Y, MAIN_H, BOT_Y, BOT_H,
    WHITE, BLACK, GREEN, RED, GRAY, DARK_GRAY, ACCENT
)
from ui.font import get_font


def draw_capsule_bar(r, x, y, w, h, pct, fg, bg=DARK_GRAY):
    """圆角胶囊进度条。"""
    r.draw_hbar(x, y, w, h, pct, fg, bg)


def draw_gradient_bar(r, x, y, w, h, c1, c2):
    """渐变胶囊色条（RGB888 元组）。"""
    r.draw_gradient_bar(x, y, w, h, c1, c2)


def draw_big_number(r, value, cx, cy, font=None, fg=WHITE, bg=BLACK):
    """居中大数字显示。"""
    if font is None:
        font = get_font(32)
    r.draw_number(value, cx, cy, font, fg, bg)


def draw_status_dot(r, cx, cy, radius=3, active=False):
    """状态指示圆点（绿=开, 红=关）。"""
    color = GREEN if active else RED
    r.draw_dot(cx, cy, radius, color)


def draw_nav_dots(r, count, active_idx, cy=None):
    """底部导航圆点指示器。"""
    if cy is None:
        cy = BOT_Y + BOT_H // 2
    spacing = 10
    total_w = (count - 1) * spacing
    start_x = SCREEN_W // 2 - total_w // 2
    for i in range(count):
        cx = start_x + i * spacing
        color = ACCENT if i == active_idx else DARK_GRAY
        radius = 3 if i == active_idx else 2
        r.draw_dot(cx, cy, radius, color)


def draw_rgb_channel_bar(r, y, channel_idx, value, active, color):
    """RGB 通道条：标签 + 彩色进度条 + 数值。

    channel_idx: 0=R, 1=G, 2=B
    value: 0-255
    active: 是否高亮选中
    color: 进度条前景色 (RGB565)
    """
    labels = ('R', 'G', 'B')
    font8 = get_font(8)

    # 标签
    label_fg = WHITE if active else GRAY
    r.draw_text(font8, labels[channel_idx], 4, y, label_fg, BLACK)

    # 进度条
    bar_x = 16
    bar_w = SCREEN_W - 60
    bar_h = 8
    pct = value * 100 // 255
    bg = DARK_GRAY if not active else 0x2104  # 选中时稍亮背景
    r.draw_hbar(bar_x, y, bar_w, bar_h, pct, color, bg)

    # 数值
    val_text = str(value)
    r.draw_text(font8, val_text.rjust(3), SCREEN_W - 28, y, label_fg, BLACK)


def draw_hint(r, text="2x:Back"):
    """底部操作提示文字。"""
    font8 = get_font(8)
    tw = font8.text_width(text)
    x = SCREEN_W - tw - 4
    y = BOT_Y + (BOT_H - 8) // 2
    r.draw_text(font8, text, x, y, GRAY, BLACK)
