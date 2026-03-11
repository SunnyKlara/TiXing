# screens/menu.py — 滑动式菜单滚轮界面 (Sliding Menu Wheel)
# 水平滚轮: 选中项居中放大高亮, 两侧项缩小淡化

from screens import Screen
from screen_manager import UI_SPEED, UI_SMOKE, UI_PUMP, UI_PRESET, UI_RGB, UI_BRIGHT
from hal.input import EVT_CLICK
from ui.theme import SCREEN_W, SCREEN_H, WHITE, BLACK, GRAY, DARK_GRAY, ACCENT
from ui.font import get_font
from ui.widgets import draw_nav_dots

# ── 菜单项定义 ──
# (标签, 目标界面, 状态属性, 后缀, 图标索引)
MENU_ITEMS = [
    ("Speed",  UI_SPEED,  "fan_speed",   "%",  0),
    ("Smoke",  UI_SMOKE,  "smoke_speed", "%",  1),
    ("Pump",   UI_PUMP,   "pump_speed",  "%",  2),
    ("Color",  UI_PRESET, "preset_idx",  "",   3),
    ("RGB",    UI_RGB,    None,          "",   4),
    ("Bright", UI_BRIGHT, "brightness",  "%",  5),
]

# ── 布局常量 ──
ITEM_SPACING = 64       # 菜单项间距 (中心到中心)
CENTER_X = SCREEN_W // 2  # 142
ICON_Y = 4              # 图标顶部 y
LABEL_Y_BIG = 40        # 选中项标签 y
LABEL_Y_SMALL = 38      # 侧边项标签 y
DOTS_Y = SCREEN_H - 6   # 导航点 y (70)
MAX_VISIBLE = 5          # 最大可见项数


def calc_visible_items(selected_idx, total_count):
    """计算所有可见菜单项的显示参数。

    中心锚定策略: 选中项固定在屏幕中心,
    其余项按固定间距向两侧排列。

    返回: [(item_idx, cx, icon_size, fg_color, font_size), ...]
    """
    result = []
    seen = set()
    half = MAX_VISIBLE // 2  # 2
    # 从中心向外遍历: 0, -1, +1, -2, +2
    offsets = [0]
    for d in range(1, half + 1):
        offsets.append(-d)
        offsets.append(d)

    for offset in offsets:
        item_idx = (selected_idx + offset) % total_count
        cx = CENTER_X + offset * ITEM_SPACING

        # 过滤超出屏幕范围的项
        if cx < -ITEM_SPACING // 2 or cx > SCREEN_W + ITEM_SPACING // 2:
            continue

        # 避免重复 (total_count < MAX_VISIBLE 时取模会重复)
        if item_idx in seen:
            continue
        seen.add(item_idx)

        if offset == 0:
            icon_size = 32
            fg_color = ACCENT
            font_size = 16
        elif abs(offset) == 1:
            icon_size = 20
            fg_color = WHITE
            font_size = 16
        else:
            icon_size = 16
            fg_color = GRAY
            font_size = 16

        result.append((item_idx, cx, icon_size, fg_color, font_size))
    return result


class MenuWheelScreen(Screen):
    """滑动式菜单滚轮界面。"""

    def __init__(self, ctx):
        super().__init__(ctx)
        self._icons = None
        self._prev_idx = -1

    def on_enter(self):
        """进入菜单: 加载图标资源, 重置滑动状态。"""
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
        """处理编码器旋转(切换选中项)和按键(进入子界面)。"""
        state = self.ctx.state
        if event.delta != 0:
            state.menu_idx = (state.menu_idx + event.delta) % len(MENU_ITEMS)
        if event.key == EVT_CLICK:
            _, target, _, _, _ = MENU_ITEMS[state.menu_idx]
            self.ctx.screen_manager.switch(target)

    def draw_full(self, r):
        """全屏绘制: 清屏 + 绘制所有可见菜单项 + 导航点。"""
        r.clear(BLACK)
        self._draw_wheel(r)
        self._prev_idx = self.ctx.state.menu_idx

    def draw_update(self, r):
        """局部刷新: 仅在选中项变化时重绘。"""
        state = self.ctx.state
        if state.menu_idx != self._prev_idx:
            # 清除内容区域 (保留导航点区域避免闪烁)
            r.fill_rect(0, 0, SCREEN_W, DOTS_Y - 2, BLACK)
            self._draw_wheel(r)
            self._prev_idx = state.menu_idx

    def _draw_wheel(self, r):
        """绘制完整的滑动式菜单滚轮。"""
        selected = self.ctx.state.menu_idx
        count = len(MENU_ITEMS)
        items = calc_visible_items(selected, count)

        # 分离选中项和侧边项, 从两侧向中心绘制
        center_item = None
        side_items = []
        for item in items:
            if item[3] == ACCENT:
                center_item = item
            else:
                side_items.append(item)

        for item_idx, cx, icon_size, fg_color, font_size in side_items:
            self._draw_item(r, item_idx, cx, icon_size, fg_color, font_size)

        if center_item:
            item_idx, cx, icon_size, fg_color, font_size = center_item
            self._draw_item(r, item_idx, cx, icon_size, fg_color, font_size)

        # 底部导航点
        draw_nav_dots(r, count, selected, cy=DOTS_Y)

    def _draw_item(self, r, item_idx, cx, icon_size, fg_color, font_size):
        """绘制单个菜单项 (图标 + 标签)。"""
        label, _, attr, suffix, icon_idx = MENU_ITEMS[item_idx]

        # 绘制图标
        if self._icons and 0 <= icon_idx < self._icons.count:
            iw = self._icons.icon_w
            ih = self._icons.icon_h
            ix = cx - iw // 2
            self._icons.draw(r._display, icon_idx, ix, ICON_Y, fg_color, BLACK)
        else:
            # fallback: 用首字母显示
            font = get_font(font_size)
            ch = label[0]
            tw = font.text_width(ch)
            r.draw_text(font, ch, cx - tw // 2, ICON_Y + 4, fg_color, BLACK)

        # 绘制文字标签
        font = get_font(font_size)
        tw = font.text_width(label)
        label_y = LABEL_Y_BIG if font_size == 16 else LABEL_Y_SMALL
        r.draw_text(font, label, cx - tw // 2, label_y, fg_color, BLACK)

    def on_exit(self):
        """释放图标资源。"""
        self._icons = None


# 向后兼容别名
MenuScreen = MenuWheelScreen
