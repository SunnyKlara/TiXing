# screens/menu.py — 滑动式菜单滚轮界面 (Sliding Menu Wheel)
# 水平滚轮: 选中项居中高亮, 两侧项淡化
# 无过渡动画, 一次性切换, 最小化刷新

from screens import Screen
from screen_manager import UI_SPEED, UI_SMOKE, UI_PUMP, UI_PRESET, UI_RGB, UI_BRIGHT
from hal.input import EVT_CLICK
from ui.theme import SCREEN_W, SCREEN_H, WHITE, BLACK, GRAY, DARK_GRAY, ACCENT, ACCENT_END
from ui.font import get_font
from ui.widgets import draw_nav_dots

# ── 菜单项定义 ──
MENU_ITEMS = [
    ("Speed",  UI_SPEED,  "fan_speed",   "%",  0),
    ("Smoke",  UI_SMOKE,  "smoke_speed", "%",  1),
    ("Pump",   UI_PUMP,   "pump_speed",  "%",  2),
    ("Color",  UI_PRESET, "preset_idx",  "",   3),
    ("RGB",    UI_RGB,    None,          "",   4),
    ("Bright", UI_BRIGHT, "brightness",  "%",  5),
]
ITEM_COUNT = len(MENU_ITEMS)

# ── 布局常量 ──
ITEM_SPACING = 64       # 菜单项间距 (中心到中心)
CENTER_X = SCREEN_W // 2
ICON_Y = 4              # 图标顶部 y
LABEL_Y = 40            # 标签 y
DOTS_Y = SCREEN_H - 6   # 导航点 y
MAX_VISIBLE = 5          # 最大可见项数


def calc_visible_items(selected_idx, total_count):
    """计算所有可见菜单项的显示参数。

    返回: [(item_idx, cx, fg_color, is_center), ...]
    """
    result = []
    seen = set()
    half = MAX_VISIBLE // 2
    offsets = [0]
    for d in range(1, half + 1):
        offsets.append(-d)
        offsets.append(d)

    for offset in offsets:
        item_idx = (selected_idx + offset) % total_count
        cx = CENTER_X + offset * ITEM_SPACING

        if cx < -ITEM_SPACING // 2 or cx > SCREEN_W + ITEM_SPACING // 2:
            continue
        if item_idx in seen:
            continue
        seen.add(item_idx)

        if offset == 0:
            fg_color = ACCENT
            is_center = True
        elif abs(offset) == 1:
            fg_color = WHITE
            is_center = False
        else:
            fg_color = GRAY
            is_center = False

        result.append((item_idx, cx, fg_color, is_center))
    return result


class MenuWheelScreen(Screen):
    """滑动式菜单滚轮界面 — 即时切换版。"""

    def __init__(self, ctx):
        super().__init__(ctx)
        self._icons = None
        self._prev_idx = -1

    def on_enter(self):
        self._prev_idx = -1
        try:
            from ui.icon import AlphaIconSheet
            self._icons = AlphaIconSheet('assets/menu_icons_alpha.bin')
        except (OSError, ImportError) as e:
            print('WARN: menu icons load failed:', e)
            self._icons = None
        except MemoryError:
            print('WARN: not enough memory for menu icons')
            self._icons = None

    def on_input(self, event):
        state = self.ctx.state
        if event.delta != 0:
            state.menu_idx = (state.menu_idx + event.delta) % ITEM_COUNT
        if event.key == EVT_CLICK:
            _, target, _, _, _ = MENU_ITEMS[state.menu_idx]
            self.ctx.screen_manager.switch(target)

    def draw_full(self, r):
        r.clear(BLACK)
        self._draw_wheel(r)
        self._prev_idx = self.ctx.state.menu_idx

    def draw_update(self, r):
        idx = self.ctx.state.menu_idx
        if idx != self._prev_idx:
            r.fill_rect(0, 0, SCREEN_W, DOTS_Y - 2, BLACK)
            self._draw_wheel(r)
            self._prev_idx = idx

    def _draw_wheel(self, r):
        selected = self.ctx.state.menu_idx
        items = calc_visible_items(selected, ITEM_COUNT)

        # 画家算法: 远的先画
        center_item = None
        side_items = []
        for item in items:
            if item[3]:
                center_item = item
            else:
                side_items.append(item)

        for item_idx, cx, fg_color, is_center in side_items:
            self._draw_item(r, item_idx, cx, fg_color, is_center)
        if center_item:
            self._draw_item(r, *center_item)

        draw_nav_dots(r, ITEM_COUNT, selected, cy=DOTS_Y)

    def _draw_item(self, r, item_idx, cx, fg_color, is_center):
        label, _, attr, suffix, icon_idx = MENU_ITEMS[item_idx]

        # 图标
        if self._icons and 0 <= icon_idx < self._icons.count:
            iw = self._icons.icon_w
            ix = cx - iw // 2
            self._icons.draw(r._display, icon_idx, ix, ICON_Y,
                             fg_color, BLACK)
        else:
            font = get_font(16)
            ch = label[0]
            tw = font.text_width(ch)
            r.draw_text(font, ch, cx - tw // 2, ICON_Y + 4,
                        fg_color, BLACK)

        # 文字标签
        font = get_font(16)
        tw = font.text_width(label)
        lx = cx - tw // 2
        if is_center:
            r.draw_gradient_text(font, label, lx, LABEL_Y,
                                 ACCENT, ACCENT_END, BLACK)
        else:
            r.draw_text(font, label, lx, LABEL_Y, fg_color, BLACK)

    def on_exit(self):
        self._icons = None


# 向后兼容
MenuScreen = MenuWheelScreen
